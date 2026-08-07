# gcal-on-eink

Display your Google Calendars on an ESP32-S3 e-ink display. Battery-powered, 6-hour refresh cycles, configurable wake times.

- **Device:** [LilyGo T5 e-Paper S3](https://lilygo.cc/en-de/products/t5-4-7-inch-e-paper-v2-3) (960×540 px, 4.7")
- **Chipset:** ESP32-S3R8 / ED047TC1
- **Power:** ~380µA sleep, WiFi on demand
- **Language:** Arduino C++

## Quick Start

### Configure WiFi & Calendar

1. Copy the template: `cp src/secrets_template.h src/secrets.h`
2. Edit `src/secrets.h` with your own values for `WIFI_SSID` `WIFI_PASSWORD` and `CALENDAR_URLS`

### Build & Upload

1. Install the [Arduino CLI](https://arduino.github.io/arduino-cli/latest/installation)
2. With the provided [sketch.yaml](sketch.yaml) just run `arduino-cli compile` to from the root of this repo
3. To upload run `arduino-cli upload --port <you port device>`

## References

- [LilyGo EPD47 Library](https://github.com/Xinyuan-LilyGO/LilyGo-EPD47)
- [uICAL](https://github.com/sourcesimian/uICAL)
- [ArduinoLog](https://github.com/thijse/Arduino-Log)
- [Arduino ESP32 Documentation](https://docs.espressif.com/projects/arduino-esp32/)
- [ED047TC1 Datasheet](./datasheets/ED047TC1.pdf)
