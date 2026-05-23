# Radar Lamp

ESP32-C3 Super Mini project that dims a lamp based on distance measured by an Acconeer XE121 radar sensor (A121 chip).

## Hardware

- **MCU**: ESP32-C3 Super Mini
- **Sensor**: Acconeer XE121 (A121 radar)

## Wiring

| Signal    | ESP32-C3 GPIO |
|-----------|--------------|
| ENABLE    | 3            |
| INTERRUPT | 5            |
| SCLK      | 6            |
| MOSI      | 7            |
| MISO      | 2            |
| CS        | 10           |
| SEL0      | 1            |
| SEL1      | 0            |
| SEL2      | 4            |
| LED       | 8 (built-in) |

> **Note:** GPIO 9 (BOOT), 8 (LED), and 18/19 (USB) were intentionally avoided for sensor SEL pins.

## Build

Requires ESP-IDF 6.1.

```bash
. ~/esp/esp-idf/export.sh
cd radar_lamp
idf.py set-target esp32c3
idf.py build
idf.py -p /dev/cu.YOURPORT flash monitor
```

## Key notes

- SPI speed is set to **10 MHz** — 40 MHz causes calibration failures over breadboard wires due to signal integrity issues
- Distance range configured: **0.25 m to 3.0 m** (balanced preset)

## License

Acconeer source files are licensed under BSD 3-Clause. See [LICENSES/license_acconeer.txt](LICENSES/license_acconeer.txt).
