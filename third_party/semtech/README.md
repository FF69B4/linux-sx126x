# Semtech Third-Party Sources

## sx126x_driver

The `sx126x_driver/` directory contains a vendored snapshot of Semtech's SX126x
radio driver.

- Source: https://github.com/Lora-net/sx126x_driver
- Imported version: v2.5.0
- Imported commit: `a10c5dfdf89788c6ac805e9fe98889de44175aa2`
- License: Clear BSD License / BSD-3-Clause-Clear

Keep the vendored source close to upstream. Linux-specific integration code
should live under `src/` unless a change intentionally updates the bundled
Semtech source.
