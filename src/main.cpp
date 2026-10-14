#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <DHT.h>
#include <WiFi.h>
#include <Wire.h>
#include <math.h>
#include <time.h>

namespace config
{
  constexpr char wifiSsid[] = "YOUR_WIFI_SSID";
  constexpr char wifiPassword[] = "YOUR_WIFI_PASSWORD";
  constexpr char ntpServer[] = "ntp.time.ir";
  constexpr char timezone[] = "IRST-3:30";
  constexpr uint32_t serialBaudRate = 115200;

  constexpr uint8_t dhtPin = 7;
  constexpr uint8_t mq135Pin = 2;
  constexpr uint8_t soilPin = 1;
  constexpr uint8_t fanPin = 5;
  constexpr uint8_t lightPin = 6;
  constexpr uint8_t pumpPin = 4;
  constexpr uint8_t bh1750SdaPin = 8;
  constexpr uint8_t bh1750SclPin = 9;
  constexpr uint8_t indicatorPin = 48;
}

namespace indicator
{
  enum class Event : uint8_t
  {
    WifiDisconnected,
    TimeNotSynced,
    SoilDry,
    PumpActive,
    Critical
  };

  struct Pattern
  {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint32_t onMs;
    uint32_t offMs;
  };

  Adafruit_NeoPixel pixel(1, config::indicatorPin, NEO_GRB + NEO_KHZ800);
  bool events[5]{};
  int activePattern{-1};
  bool isOn{};
  uint32_t phaseStartedAt{};

  void setEvent(Event event, bool active)
  {
    events[static_cast<uint8_t>(event)] = active;
  }

  Pattern getPattern(int &patternId)
  {
    if (events[static_cast<uint8_t>(Event::Critical)])
    {
      patternId = 4;
      return {255, 0, 0, 100, 100};
    }
    if (events[static_cast<uint8_t>(Event::WifiDisconnected)])
    {
      patternId = 0;
      return {255, 0, 0, 250, 1750};
    }
    if (events[static_cast<uint8_t>(Event::TimeNotSynced)])
    {
      patternId = 1;
      return {255, 150, 0, 250, 1750};
    }
    if (events[static_cast<uint8_t>(Event::SoilDry)])
    {
      patternId = 2;
      return {255, 70, 0, 300, 700};
    }
    if (events[static_cast<uint8_t>(Event::PumpActive)])
    {
      patternId = 3;
      return {0, 0, 255, 100, 100};
    }
    patternId = -1;
    return {0, 0, 0, 100, 1900};
  }

  void show(const Pattern &pattern)
  {
    pixel.setPixelColor(0, pixel.Color(pattern.red, pattern.green, pattern.blue));
    pixel.show();
  }

  void init()
  {
    pixel.begin();
    pixel.setBrightness(255);
    pixel.clear();
    pixel.show();
  }

  void update()
  {
    int patternId;
    const Pattern pattern = getPattern(patternId);
    const uint32_t now = millis();
    if (patternId != activePattern)
    {
      activePattern = patternId;
      isOn = true;
      phaseStartedAt = now;
      show(pattern);
      return;
    }

    const uint32_t phaseDuration = isOn ? pattern.onMs : pattern.offMs;
    if (now - phaseStartedAt >= phaseDuration)
    {
      isOn = !isOn;
      phaseStartedAt = now;
      if (isOn)
      {
        show(pattern);
      }
      else
      {
        pixel.clear();
        pixel.show();
      }
    }
  }
}

namespace wifi
{
  constexpr uint32_t checkIntervalMs = 1000;
  constexpr time_t minimumValidEpoch = 1600000000;
  bool connected{};
  bool attemptStarted{};
  uint8_t backoffStep{};
  uint32_t nextAttemptAt{};
  uint32_t lastCheckAt{};

  void init()
  {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(false);
    WiFi.begin(config::wifiSsid, config::wifiPassword);
    attemptStarted = true;
    nextAttemptAt = millis() + 1000;
    backoffStep = 1;
  }

  bool hasValidTime()
  {
    return time(nullptr) >= minimumValidEpoch;
  }

  void update()
  {
    const uint32_t now = millis();
    if (now - lastCheckAt < checkIntervalMs)
    {
      return;
    }
    lastCheckAt = now;

    if (WiFi.status() == WL_CONNECTED)
    {
      if (!connected)
      {
        connected = true;
        backoffStep = 0;
        Serial.print("Wi-Fi connected, IP: ");
        Serial.println(WiFi.localIP());
        indicator::setEvent(indicator::Event::WifiDisconnected, false);
        configTzTime(config::timezone, config::ntpServer);
      }
      if (hasValidTime())
      {
        indicator::setEvent(indicator::Event::TimeNotSynced, false);
      }
      else
      {
        indicator::setEvent(indicator::Event::TimeNotSynced, true);
      }
      return;
    }

    if (connected)
    {
      connected = false;
      attemptStarted = false;
      backoffStep = 0;
      nextAttemptAt = now + 1000;
      Serial.println("Wi-Fi disconnected");
      indicator::setEvent(indicator::Event::WifiDisconnected, true);
    }

    if (!attemptStarted || now >= nextAttemptAt)
    {
      const uint32_t delayMs = 1000U << (backoffStep < 5 ? backoffStep : 5);
      Serial.printf("Starting Wi-Fi reconnect; next retry in %lu ms\n",
                    static_cast<unsigned long>(delayMs));
      WiFi.disconnect();
      WiFi.begin(config::wifiSsid, config::wifiPassword);
      attemptStarted = true;
      nextAttemptAt = now + delayMs;
      if (backoffStep < 5)
      {
        backoffStep++;
      }
    }
  }
}

namespace mq135
{
  constexpr uint8_t sampleCount = 20;
  constexpr uint32_t samplePeriodMs = 100;
  uint16_t samples[sampleCount]{};
  uint32_t sum{};
  uint8_t sampleIndex{};
  uint8_t samplesCollected{};
  uint32_t lastSampleAt{};
  int averageRaw{};

  void init()
  {
    pinMode(config::mq135Pin, INPUT);
    analogReadResolution(12);
    analogSetPinAttenuation(config::mq135Pin, ADC_11db);
  }

  void update()
  {
    const uint32_t now = millis();
    if (now - lastSampleAt < samplePeriodMs)
    {
      return;
    }
    lastSampleAt = now;
    const uint16_t sample = static_cast<uint16_t>(analogRead(config::mq135Pin));
    if (samplesCollected == sampleCount)
    {
      sum -= samples[sampleIndex];
    }
    else
    {
      samplesCollected++;
    }
    samples[sampleIndex] = sample;
    sum += sample;
    sampleIndex = static_cast<uint8_t>((sampleIndex + 1) % sampleCount);
    averageRaw = static_cast<int>(sum / samplesCollected);
  }
}

namespace dht22
{
  constexpr uint32_t readIntervalMs = 3000;
  constexpr uint32_t powerUpDelayMs = 2000;
  constexpr uint8_t maximumRetries = 3;
  constexpr uint32_t retryGapMs = 100;
  constexpr uint32_t failureWarningInterval = 5;
  DHT sensor(config::dhtPin, DHT22);
  uint32_t nextReadAt{};
  uint32_t consecutiveFailures{};
  float temperature{NAN};
  float humidity{NAN};
  uint32_t lastSuccessfulReadAt{};
  bool hasReading{};

  void init()
  {
    sensor.begin(40);
    nextReadAt = millis() + powerUpDelayMs;
  }

  void recordFailure()
  {
    consecutiveFailures++;
    if (consecutiveFailures % failureWarningInterval == 0)
    {
      Serial.printf("DHT22 read failed %lu times in a row\n",
                    static_cast<unsigned long>(consecutiveFailures));
    }
  }

  void update()
  {
    if (static_cast<int32_t>(millis() - nextReadAt) < 0)
    {
      return;
    }

    bool readSucceeded = false;
    for (uint8_t attempt = 0; attempt < maximumRetries; ++attempt)
    {
      if (sensor.read(attempt > 0))
      {
        readSucceeded = true;
        break;
      }
      if (attempt + 1 < maximumRetries)
      {
        delay(retryGapMs);
      }
    }
    nextReadAt = millis() + readIntervalMs;
    if (!readSucceeded)
    {
      recordFailure();
      return;
    }

    const float nextHumidity = sensor.readHumidity();
    const float nextTemperature = sensor.readTemperature();
    if (!isfinite(nextTemperature) || !isfinite(nextHumidity) ||
        nextHumidity < 0.0f || nextHumidity > 100.0f ||
        nextTemperature < -40.0f || nextTemperature > 80.0f)
    {
      recordFailure();
      return;
    }

    temperature = nextTemperature;
    humidity = nextHumidity;
    lastSuccessfulReadAt = millis();
    hasReading = true;
    consecutiveFailures = 0;
    Serial.printf("DHT22: temperature=%.1f C, humidity=%.1f%%\n",
                  temperature, humidity);
  }
}

namespace soil
{
  constexpr uint8_t averageCount = 10;
  constexpr uint32_t samplePeriodMs = 1000;
  uint16_t samples[averageCount]{};
  uint32_t sum{};
  uint8_t sampleIndex{};
  uint8_t samplesCollected{};
  uint32_t lastSampleAt{};
  int averageRaw{};
  bool hasAverage{};

  void init()
  {
    pinMode(config::soilPin, INPUT_PULLDOWN);
    analogReadResolution(12);
    analogSetPinAttenuation(config::soilPin, ADC_11db);
  }

  void update()
  {
    const uint32_t now = millis();
    if (now - lastSampleAt < samplePeriodMs)
    {
      return;
    }
    lastSampleAt = now;
    const uint16_t sample = static_cast<uint16_t>(analogRead(config::soilPin));
    if (samplesCollected == averageCount)
    {
      sum -= samples[sampleIndex];
    }
    else
    {
      samplesCollected++;
    }
    samples[sampleIndex] = sample;
    sum += sample;
    sampleIndex = static_cast<uint8_t>((sampleIndex + 1) % averageCount);
    averageRaw = static_cast<int>(sum / samplesCollected);
    if (samplesCollected == averageCount)
    {
      hasAverage = true;
    }
  }
}

namespace fan
{
  enum class Speed : uint8_t
  {
    Off,
    Low,
    Medium,
    High
  };

  constexpr uint32_t pwmFrequencyHz = 25000;
  constexpr uint8_t pwmResolutionBits = 10;
  constexpr uint16_t dutyOff = 0;
  constexpr uint16_t dutyLow = 400;
  constexpr uint16_t dutyMedium = 700;
  constexpr uint16_t dutyHigh = 1023;
  constexpr int temperatureThresholds[3] = {260, 270, 280};
  constexpr int humidityThresholds[3] = {55, 60, 65};
  constexpr int gasThresholds[3] = {1800, 2400, 3000};
  constexpr int temperatureHysteresis = 5;
  constexpr int humidityHysteresis = 2;
  constexpr int gasHysteresis = 150;
  constexpr uint32_t staleWarningMs = 15000;

  int temperatureLevel{};
  int humidityLevel{};
  int gasLevel{};
  Speed appliedSpeed{Speed::Off};
  uint32_t lastValidDhtAt{};
  uint32_t lastStaleWarningAt{};
  bool initialized{};

  int levelOf(int current, const int (&thresholds)[3], int hysteresis, int heldLevel)
  {
    int level = 0;
    for (int index = 0; index < 3; ++index)
    {
      if (current >= thresholds[index])
      {
        level = index + 1;
      }
    }
    if (level < heldLevel && current >= thresholds[heldLevel - 1] - hysteresis)
    {
      level = heldLevel;
    }
    return level;
  }

  uint16_t dutyFor(Speed speed)
  {
    switch (speed)
    {
    case Speed::Low:
      return dutyLow;
    case Speed::Medium:
      return dutyMedium;
    case Speed::High:
      return dutyHigh;
    case Speed::Off:
    default:
      return dutyOff;
    }
  }

  void init()
  {
    pinMode(config::fanPin, OUTPUT);
    if (!ledcAttach(config::fanPin, pwmFrequencyHz, pwmResolutionBits))
    {
      indicator::setEvent(indicator::Event::Critical, true);
      Serial.println("Fan PWM initialization failed");
      return;
    }
    initialized = true;
    ledcWrite(config::fanPin, dutyOff);
  }

  void update()
  {
    if (!initialized)
    {
      return;
    }
    if (dht22::hasReading)
    {
      lastValidDhtAt = dht22::lastSuccessfulReadAt;
    }
    if (!dht22::hasReading || mq135::samplesCollected == 0)
    {
      return;
    }

    const uint32_t now = millis();
    if (now - lastValidDhtAt > staleWarningMs &&
        now - lastStaleWarningAt >= staleWarningMs)
    {
      lastStaleWarningAt = now;
      Serial.println("Warning: no valid DHT22 reading recently; holding fan control from last valid values");
    }

    temperatureLevel = levelOf(static_cast<int>(dht22::temperature * 10.0f),
                               temperatureThresholds, temperatureHysteresis, temperatureLevel);
    humidityLevel = levelOf(static_cast<int>(dht22::humidity + 0.5f),
                            humidityThresholds, humidityHysteresis, humidityLevel);
    gasLevel = levelOf(mq135::averageRaw, gasThresholds, gasHysteresis, gasLevel);
    const int temperatureHumidityLevel = temperatureLevel > humidityLevel
                         ? temperatureLevel
                         : humidityLevel;
    const int highestLevel = temperatureHumidityLevel > gasLevel
                   ? temperatureHumidityLevel
                   : gasLevel;
    const Speed requestedSpeed = static_cast<Speed>(highestLevel);
    if (requestedSpeed != appliedSpeed)
    {
      ledcWrite(config::fanPin, dutyFor(requestedSpeed));
      appliedSpeed = requestedSpeed;
      Serial.printf("Climate: T=%.1f C, H=%.1f%%, gas=%d -> fan=%u\n",
                    dht22::temperature, dht22::humidity, mq135::averageRaw,
                    static_cast<unsigned>(requestedSpeed));
    }
  }
}

namespace pump
{
  constexpr uint32_t pulseDurationMs = 10000;
  bool running{};
  uint32_t startedAt{};

  void init()
  {
    pinMode(config::pumpPin, OUTPUT);
    digitalWrite(config::pumpPin, LOW);
  }

  bool startPulse()
  {
    if (running)
    {
      return false;
    }
    digitalWrite(config::pumpPin, HIGH);
    running = true;
    startedAt = millis();
    indicator::setEvent(indicator::Event::PumpActive, true);
    Serial.println("Irrigation started (10 seconds)");
    return true;
  }

  bool update()
  {
    if (running && millis() - startedAt >= pulseDurationMs)
    {
      digitalWrite(config::pumpPin, LOW);
      running = false;
      indicator::setEvent(indicator::Event::PumpActive, false);
      Serial.println("Irrigation pulse finished");
      return true;
    }
    return false;
  }
}

namespace irrigation
{
  constexpr int dryThreshold = 800;
  constexpr uint8_t requiredDryReadings = 3;
  constexpr uint32_t cooldownMs = 15UL * 60 * 1000;
  uint8_t consecutiveDryReadings{};
  uint32_t cooldownStartedAt{};
  bool cooldownActive{};

  void update()
  {
    if (pump::update())
    {
      cooldownActive = true;
      cooldownStartedAt = millis();
      Serial.println("Irrigation cooldown started");
    }
    if (!soil::hasAverage)
    {
      return;
    }

    static int lastProcessedAverage = -1;
    static uint32_t lastSoilUpdateAt{};
    if (soil::averageRaw == lastProcessedAverage &&
        millis() - lastSoilUpdateAt < 1000)
    {
      return;
    }
    lastProcessedAverage = soil::averageRaw;
    lastSoilUpdateAt = millis();
    Serial.printf("Soil ADC average=%d\n",
            soil::averageRaw);

    if (soil::averageRaw >= dryThreshold)
    {
      if (consecutiveDryReadings < requiredDryReadings)
      {
        consecutiveDryReadings++;
      }
    }
    else
    {
      consecutiveDryReadings = 0;
    }

    const bool soilIsDry = consecutiveDryReadings >= requiredDryReadings;
    static bool lastReportedDry{};
    if (soilIsDry != lastReportedDry)
    {
      Serial.printf("Soil %s: raw=%d, dry threshold=%d\n",
                    soilIsDry ? "dry" : "moist", soil::averageRaw, dryThreshold);
      lastReportedDry = soilIsDry;
    }
    indicator::setEvent(indicator::Event::SoilDry, soilIsDry);
    if (cooldownActive && millis() - cooldownStartedAt >= cooldownMs)
    {
      cooldownActive = false;
      Serial.println("Irrigation cooldown finished");
    }
    if (soilIsDry && !pump::running && !cooldownActive)
    {
      pump::startPulse();
    }
  }
}

namespace bh1750
{
  constexpr uint8_t address = 0x23;
  constexpr uint8_t powerOnCommand = 0x01;
  constexpr uint8_t continuousHighResolutionCommand = 0x10;
  constexpr uint32_t readIntervalMs = 2000;
  uint32_t lastReadAt{};
  bool initialized{};
  bool hasReading{};
  uint32_t readingCount{};
  float lux{};

  bool sendCommand(uint8_t command)
  {
    Wire.beginTransmission(address);
    Wire.write(command);
    return Wire.endTransmission() == 0;
  }

  void init()
  {
    Wire.begin(config::bh1750SdaPin, config::bh1750SclPin);
    Wire.setClock(100000);
    initialized = sendCommand(powerOnCommand) && sendCommand(continuousHighResolutionCommand);
    if (!initialized)
    {
      indicator::setEvent(indicator::Event::Critical, true);
      Serial.println("BH1750 initialization failed");
    }
    else
    {
      lastReadAt = millis() - 1800;
    }
  }

  bool read(float &reading)
  {
    if (Wire.requestFrom(address, static_cast<uint8_t>(2)) != 2)
    {
      return false;
    }
    const uint16_t rawValue = (static_cast<uint16_t>(Wire.read()) << 8) |
                              static_cast<uint16_t>(Wire.read());
    reading = rawValue / 1.2f;
    return true;
  }

  void update()
  {
    const uint32_t now = millis();
    if (!initialized || now - lastReadAt < readIntervalMs)
    {
      return;
    }
    lastReadAt = now;
    if (read(lux))
    {
      hasReading = true;
      readingCount++;
    }
    else
    {
      Serial.println("BH1750 read failed");
    }
  }
}

namespace lighting
{
  constexpr float targetLux = 4000.0f;
  constexpr int activeHourStart = 6;
  constexpr int activeHourEnd = 21;
  constexpr float deadbandRatio = 0.05f;
  constexpr float emaAlpha = 0.5f;
  constexpr float stepDivisor = 10.0f;
  constexpr int stepMinimum = 1;
  constexpr int stepMaximum = 204;
  constexpr int maximumLevel = 1023;
  constexpr uint32_t pwmFrequencyHz = 5000;
  constexpr uint8_t pwmResolutionBits = 10;
  int level{};
  float luxEma{-1.0f};
  bool hasTime{};
  bool initialized{};
  uint32_t lastProcessedReading{};

  void init()
  {
    pinMode(config::lightPin, OUTPUT);
    if (!ledcAttach(config::lightPin, pwmFrequencyHz, pwmResolutionBits))
    {
      indicator::setEvent(indicator::Event::Critical, true);
      Serial.println("Light PWM initialization failed");
      return;
    }
    initialized = true;
    ledcWrite(config::lightPin, 0);
  }

  void setLevel(int requestedLevel, float smoothedLux)
  {
    if (!initialized || requestedLevel == level)
    {
      return;
    }
    level = constrain(requestedLevel, 0, maximumLevel);
    ledcWrite(config::lightPin, static_cast<uint32_t>(level));
    Serial.printf("Lux=%.0f, light level=%d\n", smoothedLux, level);
  }

  void update()
  {
    if (!initialized)
    {
      return;
    }
    if (!bh1750::hasReading || lastProcessedReading == bh1750::readingCount)
    {
      return;
    }
    lastProcessedReading = bh1750::readingCount;
    const float lux = bh1750::lux;
    if (luxEma < 0.0f)
    {
      luxEma = lux;
    }
    else
    {
      luxEma += emaAlpha * (lux - luxEma);
    }

    struct tm localTime{};
    const bool timeAvailable = wifi::hasValidTime() && getLocalTime(&localTime, 0);
    if (timeAvailable != hasTime)
    {
      hasTime = timeAvailable;
      if (hasTime)
      {
        Serial.printf("Clock available; light schedule is active (%02d:00-%02d:00)\n",
                      activeHourStart, activeHourEnd);
      }
    }
    if (timeAvailable && (localTime.tm_hour < activeHourStart || localTime.tm_hour >= activeHourEnd))
    {
      setLevel(0, luxEma);
      return;
    }

    const float deadband = targetLux * deadbandRatio;
    if (luxEma > targetLux - deadband && luxEma < targetLux + deadband)
    {
      return;
    }
    int step = static_cast<int>(fabsf(luxEma - targetLux) / stepDivisor);
    step = constrain(step, stepMinimum, stepMaximum);
    int requestedLevel = level;
    if (luxEma < targetLux)
    {
      requestedLevel += step;
    }
    else
    {
      requestedLevel -= step;
    }
    setLevel(requestedLevel, luxEma);
  }
}

void setup()
{
  Serial.begin(config::serialBaudRate);
  indicator::init();
  indicator::setEvent(indicator::Event::WifiDisconnected, true);
  indicator::setEvent(indicator::Event::TimeNotSynced, true);
  wifi::init();
  mq135::init();
  dht22::init();
  soil::init();
  fan::init();
  pump::init();
  bh1750::init();
  lighting::init();
}

void loop()
{
  wifi::update();
  mq135::update();
  dht22::update();
  soil::update();
  irrigation::update();
  fan::update();
  bh1750::update();
  lighting::update();
  indicator::update();
}