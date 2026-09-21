# linux-sx126x

A kernel driver implementation of [Semtech's sx126x series drivers](https://github.com/Lora-net/sx126x_driver).

## Repository layout

- `src/` - Linux kernel driver work area.
- `third_party/semtech/sx126x_driver/` - bundled Semtech SX126x driver source.

## Current driver status

The in-tree Linux driver work currently builds as an out-of-tree kernel module.
It probes SX1261/SX1262/SX1268 devices over SPI, handles optional `reset` and
`busy` GPIOs, resets the radio, sends `GetStatus`, and logs the returned status.

Build with:

```sh
make
```

Clean with:

```sh
make clean
```

## Attribution

This project is expected to use or adapt code from Semtech's
[sx126x_driver](https://github.com/Lora-net/sx126x_driver), which is licensed
under the Clear BSD License. Semtech-derived files must retain Semtech's
copyright notice, license terms, and disclaimer.

See [NOTICE.md](NOTICE.md) for third-party attribution notes.
