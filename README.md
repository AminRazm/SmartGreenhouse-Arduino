# Smart Greenhouse

This is my first version of the smart greenhouse firmware. I keep it as a small C++ project and as a sample of my C++ work. The program is in one source file and runs on an ESP32-S3.

It reads temperature, humidity, soil moisture, gas level, and light level. It uses these readings to control a fan, a grow light, a water pump, and an RGB status LED. Wi-Fi and SNTP provide the time for the light schedule.

## Two Designs

This version uses an Arduino `loop()` as a superloop. Each pass checks whether a sensor or output needs an update. Timers use `millis()`, and simple state checks control the main actions. The program does not create its own FreeRTOS tasks.

This style is useful when the work is small and each operation finishes quickly. It keeps the code compact and makes the control flow easy to follow. It is not true parallel work: every part takes its turn, and a slow operation can delay the rest. Waiting for events is also less direct because the loop keeps checking for them.

As the greenhouse needed more independent and responsive work, I moved the main project to [ESP-IDF](https://github.com/AminRazm/SmartGreenhouse-ESP-IDF). That version uses separate C components, FreeRTOS tasks, and callbacks. It handles independent work more clearly and is easier to extend. This project keeps the earlier, simpler C++ design for comparison and as a C++ code sample; the ESP-IDF version is the more complete design for the current system.

| This project | ESP-IDF project |
| --- | --- |
| One C++ source file and one superloop | Separate C components and FreeRTOS tasks |
| Work is checked and handled in sequence | Independent work can run in separate tasks |
| Compact and easy to follow | More responsive and easier to extend |

The superloop approach is used in embedded systems where the work and timing are simple enough for one main loop. This firmware uses ESP32-S3 and Arduino features, so another board may need different APIs and libraries.

## Tools

The project uses PlatformIO settings in `platformio.ini` and the pioarduino Espressif platform. That platform is a community fork of [PlatformIO's Espressif 32 platform](https://github.com/pioarduino/platform-espressif32), with support for newer Arduino-ESP32 releases used by this project. I use the pioarduino VS Code extension to work with this setup.

Select the `esp32-s3-devkitc-1` environment and use Build or Upload in the extension. The matching CLI commands are `pio run` and `pio run -t upload`.

Before building, set `YOUR_WIFI_SSID` and `YOUR_WIFI_PASSWORD` in the `config` section near the top of `src/main.cpp`. Do not publish real Wi-Fi credentials.