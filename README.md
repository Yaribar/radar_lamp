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
# 1. Source IDF (required every new terminal session)
. ~/esp/esp-idf/export.sh

# 2. Go to project
cd ~/esp/radar_lamp

# 3. Set target (only needed once or after cleaning)
idf.py set-target esp32c3

# 4. Build
idf.py build

# 5. Find your port
ls /dev/cu.*

# 6. Flash and open monitor
idf.py -p /dev/cu.YOURPORT flash monitor
```

> To exit the monitor press **Ctrl + ]**

## How it works

### Distance → brightness mapping

| Distance | Brightness |
|----------|-----------|
| ≤ 0.5 m  | 100%      |
| 1.15 m   | ~50%      |
| ≥ 1.8 m  | 0%        |

When multiple distances are detected (e.g. paper lamp shade + person), the furthest reading is always used. The sensor start range is set to 0.30 m to exclude the paper shade (~10 cm away).

### Perceptual brightness (gamma correction)

A raw linear PWM sweep from 0→100% does not look linear to the human eye — it appears to jump quickly at low values and barely change at high values. This is because **eyes perceive brightness logarithmically**.

To fix this, a gamma curve (`γ = 2.2`) is applied to the PWM duty cycle:

```
PWM duty = (brightness%)^2.2
```

This makes the lamp appear to change evenly as you move toward or away from it.

### LED state machine

```
valid reading               valid reading
      │                           │
 ┌────▼──────┐  no reading   ┌────▼─────┐
 │ TRACKING  │ ────────────► │ HOLDING  │
 │           │ ◄──────────── │          │
 └───────────┘  reading back └────┬─────┘
                                  │ 3 s timeout
                                  ▼
                            ┌──────────┐
                            │  FADING  │ → slowly → 0%
                            └──────────┘
```

- **TRACKING**: smoothly follows distance (max 2%/50 ms tick ≈ 2.5 s full sweep)
- **HOLDING**: no reading detected, holds current brightness for 3 s
- **FADING**: slowly fades to off (0.5%/tick ≈ 16 s full sweep)

### Key hardware notes

- SPI speed is **10 MHz** — 40 MHz causes calibration failures over breadboard wires
- Sensor range: **0.30 m to 3.0 m** (balanced preset)

## License

Acconeer source files are licensed under BSD 3-Clause. See [LICENSES/license_acconeer.txt](LICENSES/license_acconeer.txt).
