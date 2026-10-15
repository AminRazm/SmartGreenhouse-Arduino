# Smart Greenhouse (Arduino)

This is the first version of my ESP32-S3 smart greenhouse project. I developed it with Arduino and C++ and keep it as a C++ code sample and an earlier stage of the project.

## Features

- Climate control: Uses temperature, humidity, and gas readings to control fan speed.
- Smart lighting: Adjusts light output based on ambient light and a daily schedule.
- Automatic irrigation: Monitors soil moisture and controls the water pump.
- Wi-Fi and time sync: Uses Wi-Fi and SNTP for the lighting schedule.
- Indicator: Uses an RGB LED to show system events and errors.

## Architecture

The firmware uses a `millis()`-based superloop. Sensors, controllers, actuators, Wi-Fi, and the indicator are organized in separate code sections within one source file.

Each loop checks whether an operation needs an update. This keeps the code compact and easy to follow, but all application logic shares the same loop. A slow operation can delay other work, and events are handled through repeated checks.

As the project grew, I needed a better way to handle independent operations and respond to sensor readings. I therefore rewrote the firmware using ESP-IDF and C, with separate components, FreeRTOS tasks, and callbacks.

## Hardware

- ESP32-S3 development board
- DHT22 - temperature and humidity
- BH1750 - ambient light
- MQ135 - gas level
- Capacitive soil moisture sensor
- MOSFET-driven fan, grow light, and water pump
- RGB indicator LED

## Build

The project uses PlatformIO with the Arduino framework and the [pioarduino](https://github.com/pioarduino/platform-espressif32) platform for ESP32 development.

1. Install [Visual Studio Code](https://code.visualstudio.com/) and the [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode).
2. Set `YOUR_WIFI_SSID` and `YOUR_WIFI_PASSWORD` in the configuration section of `src/main.cpp`.
3. Build and upload the firmware.

**CLI command:**
```bash
pio run -t upload
```
## Related Project

See [SmartGreenhouse-ESP-IDF](https://github.com/AminRazm/SmartGreenhouse-ESP-IDF) for the rewritten firmware.

The two repositories show my work with both C++ and C, and how the firmware architecture evolved as the project grew.
