# SmartGreenhouse-Arduino

An ESP32-S3 smart greenhouse built with Arduino and C++. This is the original version of the project, kept as a C++ code sample and a record of how the firmware architecture evolved.

## Features

- **Climate control:** Uses temperature, humidity, and gas readings to control fan speed.
- **Smart lighting:** Adjusts light output based on ambient light and a daily schedule.
- **Automatic irrigation:** Monitors soil moisture and controls the water pump.
- **Wi-Fi and time sync:** Uses Wi-Fi and SNTP to keep the lighting schedule accurate.
- **RGB indicator:** Shows system events and errors.

## Architecture

This version uses an Arduino `loop()` as a superloop. Timers use `millis()`, and each pass checks whether a sensor or output needs an update. The application logic is kept in one source file, and the program does not create its own FreeRTOS tasks.

This approach is compact and easy to follow when operations are simple and finish quickly. However, each part takes its turn in the same loop, so a slow operation can delay other work. Events are also handled through repeated checks.

As the project grew, I needed a clearer way to handle independent operations and respond to sensor readings. I rewrote the firmware using ESP-IDF and C, with separate components, FreeRTOS tasks, and callbacks.

| Arduino version | ESP-IDF version |
| --- | --- |
| One main source file and a superloop | Separate components and FreeRTOS tasks |
| Operations are checked in sequence | Independent work is organized into separate tasks |
| Compact C++ code sample | Modular design that is easier to extend |

The superloop approach is a valid embedded design when the workload and timing fit one main loop. This repository preserves the earlier C++ implementation for comparison with the [ESP-IDF version](https://github.com/AminRazm/SmartGreenhouse-ESP-IDF).

## Hardware

- ESP32-S3 development board
- DHT22 — temperature and humidity
- BH1750 — ambient light level
- MQ135 — gas level
- Capacitive soil moisture sensor
- MOSFET-driven fan, grow light, and water pump
- RGB indicator LED

## Build

The project uses PlatformIO, the Arduino framework, and the community-maintained [pioarduino Espressif platform](https://github.com/pioarduino/platform-espressif32).

1. Install VS Code and the pioarduino PlatformIO extension.
2. Open the project and select the `esp32-s3-devkitc-1` environment.
3. Set `YOUR_WIFI_SSID` and `YOUR_WIFI_PASSWORD` in the `config` section near the top of `src/main.cpp`.
4. Build and upload the firmware.

CLI commands:

```bash
pio run
pio run -t upload
```

Do not publish real Wi-Fi credentials.
 
## Related Project

See [SmartGreenhouse-ESP-IDF](https://github.com/AminRazm/SmartGreenhouse-ESP-IDF) for the modular C implementation using ESP-IDF and FreeRTOS.
