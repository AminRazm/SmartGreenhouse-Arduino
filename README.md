# Smart Greenhouse — ESP-IDF

This is the first version of my ESP32-S3 smart greenhouse project. I developed it with Arduino and C++ and keep it as a C++ code sample and an earlier stage of the project.

## Features

- Climate control: Uses temperature, humidity, and gas readings to control fan speed.
- Smart lighting: Adjusts light output based on ambient light and a daily schedule.
- Automatic irrigation: Monitors soil moisture and controls the water pump.
- Wi-Fi and time sync: Uses Wi-Fi and SNTP for the lighting schedule.
- Indicator: Uses an RGB LED to show system events and errors.

## Architecture

The firmware uses a "millis()"-based superloop. Sensors, controllers, actuators, Wi-Fi, and the indicator are organized in separate code sections within one source file.

Each loop checks whether an operation needs an update. This keeps the code compact and easy to follow, but all application logic shares the same loop. A slow operation can delay other work, and events are handled through repeated checks.

As the project grew, I needed a better way to handle independent operations and respond to sensor readings. I therefore rewrote the firmware using ESP-IDF and C, with separate components, FreeRTOS tasks, and callbacks.

## Hardware

- ESP32-S3 development board
- DHT22 — temperature and humidity
- BH1750 — ambient light
- MQ135 — gas level
- Capacitive soil moisture sensor
- MOSFET-driven fan, grow light, and water pump
- RGB indicator LED

## Build

The project uses PlatformIO, the Arduino framework, and the pioarduino Espressif platform.

1. Install VS Code and PlatformIO.
2. Open the project and select the "esp32-s3-devkitc-1" environment.
3. Set "YOUR_WIFI_SSID" and "YOUR_WIFI_PASSWORD" in the configuration section of "src/main.cpp".
4. Build and upload the firmware.

CLI commands:

pio run
pio run -t upload

The project uses pioarduino to access the Arduino-ESP32 platform version specified by its PlatformIO configuration.

## Related Project

See "SmartGreenhouse-ESP-IDF" (https://github.com/AminRazm/SmartGreenhouse-ESP-IDF) for the rewritten firmware.

The two repositories show my work with both C++ and C, and how the firmware architecture evolved as the project grew.