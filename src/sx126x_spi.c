// SPDX-License-Identifier: GPL-2.0-only
/*
 * Linux SPI driver skeleton for Semtech SX126x packet radio transceivers.
 */

#include <linux/bitfield.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/gpio/consumer.h>
#include <linux/jiffies.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/spi/spi.h>

#define SX126X_CMD_GET_STATUS		0xc0
#define SX126X_NOP			0x00

#define SX126X_STATUS_CHIP_MODE		GENMASK(6, 4)
#define SX126X_STATUS_CMD_STATUS	GENMASK(3, 1)

#define SX126X_BUSY_TIMEOUT_MS		100

struct sx126x_variant {
	const char *name;
};

struct sx126x {
	struct device *dev;
	struct spi_device *spi;
	const struct sx126x_variant *variant;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *busy_gpio;
	struct mutex lock;
};

static const struct sx126x_variant sx1261_variant = {
	.name = "SX1261",
};

static const struct sx126x_variant sx1262_variant = {
	.name = "SX1262",
};

static const struct sx126x_variant sx1268_variant = {
	.name = "SX1268",
};

static const char *sx126x_chip_mode_name(u8 mode)
{
	switch (mode) {
	case 0x2:
		return "standby-rc";
	case 0x3:
		return "standby-xosc";
	case 0x4:
		return "fs";
	case 0x5:
		return "rx";
	case 0x6:
		return "tx";
	default:
		return "unknown";
	}
}

static const char *sx126x_cmd_status_name(u8 status)
{
	switch (status) {
	case 0x1:
		return "reserved";
	case 0x2:
		return "data-available";
	case 0x3:
		return "timeout";
	case 0x4:
		return "processing-error";
	case 0x5:
		return "execution-failure";
	case 0x6:
		return "tx-done";
	default:
		return "unknown";
	}
}

static int sx126x_wait_while_busy(struct sx126x *radio)
{
	unsigned long timeout;
	int busy;

	if (!radio->busy_gpio)
		return 0;

	timeout = jiffies + msecs_to_jiffies(SX126X_BUSY_TIMEOUT_MS);

	do {
		busy = gpiod_get_value_cansleep(radio->busy_gpio);
		if (busy < 0)
			return busy;
		if (!busy)
			return 0;

		usleep_range(100, 200);
	} while (time_before(jiffies, timeout));

	return -ETIMEDOUT;
}

static int sx126x_hw_reset(struct sx126x *radio)
{
	if (!radio->reset_gpio)
		return 0;

	gpiod_set_value_cansleep(radio->reset_gpio, 1);
	usleep_range(100, 200);
	gpiod_set_value_cansleep(radio->reset_gpio, 0);
	msleep(10);

	return sx126x_wait_while_busy(radio);
}

static int sx126x_get_status(struct sx126x *radio, u8 *status)
{
	u8 cmd = SX126X_CMD_GET_STATUS;
	u8 rx = SX126X_NOP;
	int ret;

	ret = sx126x_wait_while_busy(radio);
	if (ret)
		return ret;

	ret = spi_write_then_read(radio->spi, &cmd, sizeof(cmd), &rx, sizeof(rx));
	if (ret)
		return ret;

	*status = rx;

	return sx126x_wait_while_busy(radio);
}

static int sx126x_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	const struct spi_device_id *id = spi_get_device_id(spi);
	struct sx126x *radio;
	u8 status;
	u8 chip_mode;
	u8 cmd_status;
	int ret;

	radio = devm_kzalloc(dev, sizeof(*radio), GFP_KERNEL);
	if (!radio)
		return -ENOMEM;

	radio->dev = dev;
	radio->spi = spi;
	radio->variant = device_get_match_data(dev);
	if (!radio->variant && id)
		radio->variant = (const struct sx126x_variant *)id->driver_data;
	if (!radio->variant)
		return dev_err_probe(dev, -ENODEV, "missing device match data\n");

	mutex_init(&radio->lock);
	spi_set_drvdata(spi, radio);

	spi->mode = SPI_MODE_0;
	ret = spi_setup(spi);
	if (ret)
		return dev_err_probe(dev, ret, "failed to configure SPI\n");

	radio->reset_gpio = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(radio->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(radio->reset_gpio),
				     "failed to get reset GPIO\n");

	radio->busy_gpio = devm_gpiod_get_optional(dev, "busy", GPIOD_IN);
	if (IS_ERR(radio->busy_gpio))
		return dev_err_probe(dev, PTR_ERR(radio->busy_gpio),
				     "failed to get busy GPIO\n");

	mutex_lock(&radio->lock);
	ret = sx126x_hw_reset(radio);
	if (!ret)
		ret = sx126x_get_status(radio, &status);
	mutex_unlock(&radio->lock);

	if (ret)
		return dev_err_probe(dev, ret, "failed to read radio status\n");

	chip_mode = FIELD_GET(SX126X_STATUS_CHIP_MODE, status);
	cmd_status = FIELD_GET(SX126X_STATUS_CMD_STATUS, status);

	dev_info(dev, "%s detected: status=0x%02x chip_mode=%s cmd_status=%s\n",
		 radio->variant->name, status, sx126x_chip_mode_name(chip_mode),
		 sx126x_cmd_status_name(cmd_status));

	return 0;
}

static void sx126x_remove(struct spi_device *spi)
{
	struct sx126x *radio = spi_get_drvdata(spi);

	dev_dbg(radio->dev, "removed\n");
}

static const struct of_device_id sx126x_of_match[] = {
	{ .compatible = "semtech,sx1261", .data = &sx1261_variant },
	{ .compatible = "semtech,sx1262", .data = &sx1262_variant },
	{ .compatible = "semtech,sx1268", .data = &sx1268_variant },
	{ }
};
MODULE_DEVICE_TABLE(of, sx126x_of_match);

static const struct spi_device_id sx126x_spi_ids[] = {
	{ "sx1261", (kernel_ulong_t)&sx1261_variant },
	{ "sx1262", (kernel_ulong_t)&sx1262_variant },
	{ "sx1268", (kernel_ulong_t)&sx1268_variant },
	{ }
};
MODULE_DEVICE_TABLE(spi, sx126x_spi_ids);

static struct spi_driver sx126x_driver = {
	.driver = {
		.name = "sx126x",
		.of_match_table = sx126x_of_match,
	},
	.probe = sx126x_probe,
	.remove = sx126x_remove,
	.id_table = sx126x_spi_ids,
};
module_spi_driver(sx126x_driver);

MODULE_DESCRIPTION("Semtech SX126x packet radio SPI driver");
MODULE_AUTHOR("Ina");
MODULE_LICENSE("GPL");
