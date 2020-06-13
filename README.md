# Canokey on STM32
[![FOSSA Status](https://app.fossa.com/api/projects/git%2Bgithub.com%2Fcanokeys%2Fcanokey-stm32.svg?type=shield)](https://app.fossa.com/projects/git%2Bgithub.com%2Fcanokeys%2Fcanokey-stm32?ref=badge_shield)

CanoKey is an open-source USB/NFC security token, providing the following functions:

- OpenPGP Card V3.4 (RSA, ECDSA, ED25519 supported)
- PIV Card
- TOTP / HOTP (RFC6238 / RFC4226)
- U2F
- FIDO2 (WebAuthn)

It works on modern Linux/Windows/macOS operating systems without additional driver required.

## Hardware

Current Canokey implementation is based on STM32L423KC MCU, which features a Cortex-M4 processor, 256KiB Flash, 64 KiB SRAM, and a full-speed USB controller. 

### Canokey NFC-A

This official hardware design features a USB Type-A plug, NFC antenna and touch sensing. It's an open-source hardware design. Schematics and PCB design files are published at [canokey-hardware](https://github.com/canokeys/canokey-hardware).

### NUCLEO-L432KC (Development Board)
For demonstration purposes, you may run this project on the [NUCLEO-L432KC](https://os.mbed.com/platforms/ST-Nucleo-L432KC/) development board with the following hardware connection:

- D2 (PA12) <-> USB D+
- D10 (PA11) <-> USB D-
- GND <-> USB GND
- 5V <-> USB Power

The NFC and touch sensing functions are unavailable on NUCLEO board.

**The micro USB port on board connects to ST-LINK. Do not confuse it with USB signal pins of MCU.**

## Build and test
### Build the Firmware

Prerequisites:

- CMake >= 3.6
- GNU ARM Embedded Toolchain, downloaded from [ARM](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads)
- git (used to generate embedding version string)

Build steps:

```shell
# clone this repo and all the submodules
# in the top-level folder
mkdir build
cd build
cmake -DCROSS_COMPILE=<path-to-toolchain>/bin/arm-none-eabi- \
    -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release ..
make canokey.bin
```

Then download the firmware file `canokey.bin` to the STM32 with flash programming tools (e.g., ST-Link Utility if you use the NUCLEO-L432KC, or the [dfu-util](https://github.com/z4yx/dfu-util), or the [WebDFU](https://dfu.canokeys.org)).

### Initialize and Test

Prerequisites:

- Linux OS with pcscd, pcsc_scan and scriptor installed

Connect the Canokey to PC, an USB CCID device should show up. The `pcsc_scan` command should be able to detect a smart card:

```
$ pcsc_scan
TODO: output
```

Then, initialize the Canokey by running `./init-fido-demo.sh`. This script will set admin PIN to `123456`, set serial number to current timestamp, and write an attestation certificate used by FIDO. Refer to [admin doc](https://doc.canokeys.org/development/protocols/admin/) if you want to customize these values.

```
$ ./init-fido-demo.sh
TODO: output
```

After initialization, you are free to use Canokey with applications, such as:

- GPG, e.g. `gpg --card-status`
- SSH pkcs11
- piv-tool
- Websites with U2F enabled, e.g., https://github.com/settings/security
- Websites with WebAuthn enabled, e.g., https://webauthn.me/
- TOTP dashboard (under development)

### Internals

The hardware-independent code (including applets, storage, cryptography and USB stack) is in a submodule named [canokey-core](https://github.com/canokeys/canokey-core).

Major hardware-dependent components in this repo:

- `Drivers`: STM32 HAL Drivers
- `Src/device.c`: Hardware operations called by canokey-core
- `Src/lfs_init.c`: Flash operations used by file system
- `Src/main.c`: Hardware initialization
- `Src/usbd_conf.c`: USB interface

## Documentation

Check out our official [documentation](https://doc.canokeys.org).

## License

All software, unless otherwise noted, is licensed under Apache 2.0.

Unless you explicitly state, otherwise, any contribution intentionally submitted for inclusion in the work by you, as defined in the Apache-2.0 license without any additional terms or conditions.

All hardware, unless otherwise noted, is dual licensed under CERN and CC-BY-SA. You may use CanoKey hardware under the terms of either the CERN 1.2 license or CC-BY-SA 4.0 license.

All documentation, unless otherwise noted, is licensed under CC-BY-SA. You may use CanoKey documentation under the terms of the CC-BY-SA 4.0 license

[![FOSSA Status](https://app.fossa.com/api/projects/git%2Bgithub.com%2Fcanokeys%2Fcanokey-stm32.svg?type=large)](https://app.fossa.com/projects/git%2Bgithub.com%2Fcanokeys%2Fcanokey-stm32?ref=badge_large)
