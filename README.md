<img src="assets/aura.svg" width="100%" />

# Aura Control
A straightforward firmware utilizing the esp-matter SDK to develop a Matter-enabled 8-channel relay with support for external inputs.
The Firmware provides flexible design to easily enable/disable & configure available channels based on your requirements.

## Building and flashing
Requirements
- esp-idf installed and configured
- esp-matter installed and configured
Steps
- clone the repository
- set gpio pins for input & output using Kconfig
- build and flash


| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |

## Demo device
This project involves a custom PCB that integrates an 8-channel relay system with connectivity for external input switches, with a Hi-Link converter to run the device from AC. The board is powered by an ESP32 Devkit.
- [PCB Schema](https://365.altium.com/files/C64237F8-63B1-4CE4-85EA-31C3FFC568F1)
- [Demo Device](assets/device.jpg)

## Other firmwares
- [Aura Climate](https://github.com/jrsarath/aura-climate)

## Credits
- [esp-idf](https://github.com/espressif/esp-idf)
- [esp-matter](https://github.com/espressif/esp-matter)

<br />
<br />
Made in Kolkata with ❤️ 