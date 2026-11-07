# LoRaWeather Link
# PicoSky Station

A wireless weather station built with the **Raspberry Pi Pico 2**, **LoRa RA-02 (SX1278)** and **BME280** sensor. The transmitter reads temperature, humidity and air pressure and sends them over LoRa. The receiver shows the data on a **0.96" OLED** with a smooth animated interface.

```
 [BME280] --I2C--> [Pico 2 + RA-02]  ~~ LoRa 433 MHz ~~>  [RA-02 + Pico 2] --I2C--> [OLED]
        TRANSMITTER                                              RECEIVER
```

## Features

- Temperature (°C), humidity (%) and pressure (hPa) over LoRa
- Smooth animated OLED UI: values glide to new readings at about 25 FPS
- Icons and progress bars for each measurement
- Signal strength bars, RSSI and packet counter
- "No signal" warning if the transmitter stops
- Serial Monitor output on both boards for debugging

## Hardware

| Qty (per side) | Part |
|---|---|
| 2 total | Raspberry Pi Pico 2 |
| 2 total | LoRa RA-02 (SX1278, 433 MHz) module + antenna |
| 1 | BME280 sensor module (I2C) - transmitter |
| 1 | 0.96" SSD1306 OLED, 128x64, I2C - receiver |
| - | Breadboards, jumper wires |

> The RA-02 has a 2 mm pin pitch, so it needs a breakout adapter for a normal breadboard.
> It is a **3.3V-only** device. Never connect it to 5V.
> Always attach the antenna before powering on.

## Wiring

### RA-02 LoRa module (both boards)

| RA-02 | Pico 2 |
|---|---|
| 3.3V | 3V3 |
| GND | GND |
| SCK | GP18 |
| MISO | GP16 |
| MOSI | GP19 |
| NSS | GP17 |
| RST | GP20 |
| DIO0 | GP21 |

### BME280 (transmitter only)

| BME280 | Pico 2 |
|---|---|
| VIN | 3V3 |
| GND | GND |
| SDA | GP4 |
| SCL | GP5 |

### OLED SSD1306 (receiver only)

| OLED | Pico 2 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | GP4 |
| SCL | GP5 |

## Software setup

1. Install the [Arduino IDE](https://www.arduino.cc/en/software).
2. Open **File -> Preferences** and add this URL to *Additional Boards Manager URLs*:
   ```
   https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
   ```
3. Open **Tools -> Board -> Boards Manager**, install **Raspberry Pi Pico/RP2040/RP2350** by Earle Philhower.
4. Select the board **Raspberry Pi Pico 2**.
5. Install these libraries from **Library Manager**:

| Library | Author | Used by |
|---|---|---|
| LoRa | Sandeep Mistry | both |
| Adafruit BME280 Library | Adafruit | transmitter |
| Adafruit Unified Sensor | Adafruit | transmitter |
| Adafruit SSD1306 | Adafruit | receiver (OLED) |
| Adafruit GFX Library | Adafruit | receiver (OLED) |

## Project structure

```
PicoSky-LoRa-Weather-Station/
├── lora_tx/                   Transmitter (Pico 2 + RA-02 + BME280)
│   └── lora_tx.ino
├── lora_rx/                   Receiver, Serial Monitor output only
│   └── lora_rx.ino
├── lora_rx_oled/              Receiver, simple OLED display
│   └── lora_rx_oled.ino
├── lora_rx_oled_smooth/       Receiver, animated OLED UI (recommended)
│   └── lora_rx_oled_smooth.ino
└── README.md
```

## Usage

1. Upload `lora_tx/lora_tx.ino` to the transmitter Pico 2.
2. Upload `lora_rx_oled_smooth/lora_rx_oled_smooth.ino` to the receiver Pico 2.
3. Power both boards. The OLED shows a splash screen, then "Waiting for data...", then the live readings.
4. Open the Serial Monitor at **115200 baud** to see the raw values.

## LoRa settings

Both sketches must use identical settings:

| Setting | Value |
|---|---|
| Frequency | 433 MHz |
| Spreading factor | 9 |
| Bandwidth | 125 kHz |
| Coding rate | 4/5 |
| Sync word | 0x12 |
| CRC | enabled |
| TX power (transmitter) | 17 dBm |

> **Regulations:** check which frequency bands and power levels are legal in your country before transmitting. The RA-02 covers roughly 410-525 MHz. In India, 865-867 MHz is the usual license-free band, so make sure 433 MHz is permitted for your use.

## Packet format

The transmitter sends a comma-separated text packet every 5 seconds:

```
counter,temperature,humidity,pressure
```

Example: `12,25.30,60.10,1005.20`

## Customization

| What | Where | Setting |
|---|---|---|
| Send interval | `lora_tx.ino` | `SEND_INTERVAL` |
| Frequency | both | `LORA_FREQ` |
| Animation speed | `lora_rx_oled_smooth.ino` | `EASE` (0.1 slower, 0.3 faster) |
| Frame rate | `lora_rx_oled_smooth.ino` | `FRAME_MS` |
| No-signal timeout | `lora_rx_oled_smooth.ino` | `SIGNAL_TIMEOUT` |
| OLED I2C address | receiver OLED sketches | `OLED_ADDR` (`0x3C` or `0x3D`) |
| Bar ranges | `lora_rx_oled_smooth.ino` | `mapf(...)` calls in `loop()` |

## Troubleshooting

| Problem | Fix |
|---|---|
| `LoRa init failed` | Check all 6 SPI wires and 3.3V power; make sure the module is on a proper breakout |
| `BME280 not found` | Check SDA/SCL; the code tries addresses `0x76` and `0x77` |
| OLED blank | Change `OLED_ADDR` to `0x3D`; check SDA/SCL |
| No packets received | Confirm both boards use the same frequency, SF, bandwidth and sync word; attach antennas |
| Short range | Use proper 433 MHz antennas, keep line of sight, try a higher spreading factor (e.g. 10-12) |

## Ideas for the future

- Battery operation with deep sleep on the transmitter
- Data logging to SD card or sending to a cloud dashboard (MQTT, ThingSpeak)
- Multiple transmitter nodes with unique IDs
- Altitude estimate from pressure

## License

MIT License. Free to use, modify and share.

