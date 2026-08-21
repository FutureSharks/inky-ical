# inky-ical

Do you have a busy life and fail to check your calendar app in the morning? Or sometimes forget an important event as you are trying to reduce screen time? This project could help bringing some of your online life to a distraction free display in your living space.

This project can display your iCAL calendars on an e-ink display. For example your Google, Apple or Microsoft 365 calendars.

Features:

-  Battery powered, lasts months
-  Daily cycle at configurable time
-  Can fetch and merge multiple calendars
-  Retries on errors, gives up if too many

Hardware:

- **Device:** [LilyGo T5 e-Paper S3](https://lilygo.cc/en-de/products/t5-4-7-inch-e-paper-v2-3) (960×540 px, 4.7")
- **Chipset:** ESP32-S3R8 / ED047TC1
- **Power:** Uses a LiPo battery
- **Language:** Arduino C++

More photos can be found in [images](images) and a CAD model for the enclosure is [here](inky-ical.f3d)

## Quick Start

1. Copy the template: `cp src/secrets_template.h src/secrets.h`
2. Edit `src/secrets.h` with your own values for `WIFI_SSID` `WIFI_PASSWORD` and `CALENDAR_URLS`
3. Install the [Arduino CLI](https://arduino.github.io/arduino-cli/latest/installation)
4. With the provided [sketch.yaml](sketch.yaml) just run `arduino-cli compile` to from the root of this repo
5. To upload run `arduino-cli upload --port <you port device>`

## References

- [LilyGo EPD47 Library](https://github.com/Xinyuan-LilyGO/LilyGo-EPD47)
- [uICAL](https://github.com/sourcesimian/uICAL)
- [ArduinoLog](https://github.com/thijse/Arduino-Log)
- [Arduino ESP32 Documentation](https://docs.espressif.com/projects/arduino-esp32/)
- [ED047TC1 Datasheet](./datasheets/ED047TC1.pdf)
