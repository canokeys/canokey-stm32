# Canokey on STM32
[![Build Status](https://travis-ci.com/canokeys/canokey-stm32.svg?branch=master)](https://travis-ci.com/canokeys/canokey-stm32) [![FOSSA Status](https://app.fossa.com/api/projects/git%2Bgithub.com%2Fcanokeys%2Fcanokey-stm32.svg?type=shield)](https://app.fossa.com/projects/git%2Bgithub.com%2Fcanokeys%2Fcanokey-stm32?ref=badge_shield)

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

```shell
$ pcsc_scan
Using reader plug'n play mechanism
Scanning present readers...
0: Kingtrust Multi-Reader [OpenPGP PIV OATH] (00000000) 00 00
 
Tue Jul  7 15:21:00 2020
 Reader 0: Kingtrust Multi-Reader [OpenPGP PIV OATH] (00000000) 00 00
  Event number: 0
  Card state: Card inserted, 
  ATR: 3B F7 11 00 00 81 31 FE 65 43 61 6E 6F 6B 65 79 99

ATR: 3B F7 11 00 00 81 31 FE 65 43 61 6E 6F 6B 65 79 99
+ TS = 3B --> Direct Convention
+ T0 = F7, Y(1): 1111, K: 7 (historical bytes)
  TA(1) = 11 --> Fi=372, Di=1, 372 cycles/ETU
    10752 bits/s at 4 MHz, fMax for Fi = 5 MHz => 13440 bits/s
  TB(1) = 00 --> VPP is not electrically connected
  TC(1) = 00 --> Extra guard time: 0
  TD(1) = 81 --> Y(i+1) = 1000, Protocol T = 1 
-----
  TD(2) = 31 --> Y(i+1) = 0011, Protocol T = 1 
-----
  TA(3) = FE --> IFSC: 254
  TB(3) = 65 --> Block Waiting Integer: 6 - Character Waiting Integer: 5
+ Historical bytes: 43 61 6E 6F 6B 65 79
  Category indicator byte: 43 (proprietary format)
+ TCK = 99 (correct checksum)

Possibly identified card (using /home/zhang/.cache/smartcard_list.txt):
3B F7 11 00 00 81 31 FE 65 43 61 6E 6F 6B 65 79 99
        Canokey (Other)
        http://canokeys.org/
```

Then, initialize the Canokey by running `device-config-init.sh`. This script will login as admin with default PIN `123456`, set device serial number to current Unix timestamp, write the EEPROM of NFC chip (if any), then write an attestation certificate used by FIDO. Refer to [admin doc](https://doc.canokeys.org/development/protocols/admin/) if you want to customize these steps.

```shell
$ cd utils
$ ./device-config-init.sh 'Kingtrust Multi-Reader [OpenPGP PIV OATH] (00000000) 00 00'
Reader name: Kingtrust Multi-Reader [OpenPGP PIV OATH] (00000000) 00 00
Using given card reader: Kingtrust Multi-Reader [OpenPGP PIV OATH] (00000000) 00 00
Using T=1 protocol
Reading commands from STDIN
> 00 A4 04 00 05 F0 00 00 00 00 
< 90 00 : Normal processing.
> 00 20 00 00 06 31 32 33 34 35 36 
< 90 00 : Normal processing.
> 00 FF 01 01 09 03 B0 05 72 03 00 B3 99 00 
< 90 00 : Normal processing.
> 00 FF 01 01 03 03 91 00 
< 90 00 : Normal processing.
> 00 FF 01 01 06 03 A0 44 00 04 20 
< 90 00 : Normal processing.
> 00 30 00 00 04 5f 04 21 aa 
< 90 00 : Normal processing.
> 00 01 00 00 20 D9 5C 12 15 D1 0A BB 57 91 B6 47 52 DF 9D 25 3C A4 17 31 37 5D 41 CD 9C D9 3C DA 00 51 36 E6 4E 
< 90 00 : Normal processing.
> 00 02 00 00 00 01 C7 30 82 01 c3 30 82 01 6a a0 03 02 01 02 02 08 1b de 06 7b 4c d9 49 e8 30 0a 06 08 2a 86 48 ce 3d 04 03 02 30 4f 31 0b 30 09 06 03 55 04 06 13 02 63 6e 31 0d 30 0b 06 03 55 04 0a 13 04 7a 34 79 78 31 22 30 20 06 03 55 04 0b 13 19 41 75 74 68 65 6e 74 69 63 61 74 6f 72 20 41 74 74 65 73 74 61 74 69 6f 6e 31 0d 30 0b 06 03 55 04 03 13 04 7a 34 79 78 30 1e 17 0d 31 39 30 39 32 30 31 33 31 32 30 30 5a 17 0d 32 30 30 39 32 30 31 33 31 32 30 30 5a 30 4f 31 0b 30 09 06 03 55 04 06 13 02 63 6e 31 0d 30 0b 06 03 55 04 0a 13 04 7a 34 79 78 31 22 30 20 06 03 55 04 0b 13 19 41 75 74 68 65 6e 74 69 63 61 74 6f 72 20 41 74 74 65 73 74 61 74 69 6f 6e 31 0d 30 0b 06 03 55 04 03 13 04 7a 34 79 78 30 59 30 13 06 07 2a 86 48 ce 3d 02 01 06 08 2a 86 48 ce 3d 03 01 07 03 42 00 04 8e 6e 62 2a ad 84 87 00 e2 ba 76 4a 0f be 68 be db cc 61 2d aa 11 00 46 07 16 c1 3c 5d 96 32 c3 ae 49 f4 e9 a2 db 6f d5 ee 2b 64 53 fa 7b 3d 2f 1b da a7 e5 51 6f 4d 53 32 40 97 10 f3 0e 8e fc a3 30 30 2e 30 09 06 03 55 1d 13 04 02 30 00 30 21 06 0b 2b 06 01 04 01 82 e5 1c 01 01 04 04 12 00 00 24 4e b2 9e e0 90 4e 49 81 fe 1f 20 f8 d3 b8 f4 30 0a 06 08 2a 86 48 ce 3d 04 03 02 03 47 00 30 44 02 20 12 87 b2 31 72 f0 2a 97 17 a0 b6 27 da 39 36 26 4f 45 21 e4 58 45 52 15 78 45 99 a5 be 7d fa 7d 02 20 20 79 69 3a 78 31 d3 ec 3a 9b 17 1b 30 47 04 a8 35 3b 5b 58 d5 57 f5 77 59 eb 7a 07 ba 0e 1c 67 
< 90 00 : Normal processing.
```

After initialization, you are free to use Canokey with applications, such as:

- GPG, e.g. `gpg --card-status`
- SSH with pkcs11, e.g. `ssh -I /usr/lib/x86_64-linux-gnu/opensc-pkcs11.so user@host`
- piv-tool
- Websites with U2F enabled, e.g., https://github.com/settings/security
- Websites with WebAuthn enabled, e.g., https://webauthn.me/
- TOTP dashboard: https://console.canokeys.org/oath/ (under development)
- CanoKey Web Console: https://console.canokeys.org/ (under development)

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
