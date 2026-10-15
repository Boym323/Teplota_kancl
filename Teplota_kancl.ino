// ESP8266 + SHT4x telemetry for TMEP.cz.
// The device takes one reading per boot and ALWAYS returns to deep sleep.
// For timed wakeup, the hardware normally needs GPIO16 connected to RST.

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Adafruit_SHT4x.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

// Keep local Wi-Fi passwords in config.h, not in Git.
#if __has_include("config.h")
#include "config.h"
#else
#include "config.example.h"
#endif

namespace {

constexpr uint32_t kWifiTimeoutMs = 15000;
constexpr uint32_t kHttpTimeoutMs = 5000;
constexpr uint16_t kTmepPort = 80;
constexpr size_t kHostCapacity = 64;
constexpr size_t kRequestCapacity = 192;
constexpr size_t kStatusCapacity = 64;

Adafruit_SHT4x sht4;

void sleepUntilNextReading() {
  Serial.print(F("Deep sleep (seconds): "));
  Serial.println(TMEP_SLEEP_SECONDS);
  Serial.flush();

  // Use 64-bit multiplication to avoid overflow for larger valid intervals.
  ESP.deepSleep(static_cast<uint64_t>(TMEP_SLEEP_SECONDS) * 1000000ULL);
  delay(100); // Allow the ESP8266 to enter deep sleep.
}

bool readSensor(float &temperature, float &humidity) {
  if (!sht4.begin()) {
    Serial.println(F("SHT4x not found"));
    return false;
  }

  sht4.setPrecision(SHT4X_HIGH_PRECISION);
  sht4.setHeater(SHT4X_NO_HEATER);

  sensors_event_t humidityEvent = {};
  sensors_event_t temperatureEvent = {};
  if (!sht4.getEvent(&humidityEvent, &temperatureEvent)) {
    Serial.println(F("SHT4x read failed"));
    return false;
  }

  temperature = temperatureEvent.temperature;
  humidity = humidityEvent.relative_humidity;

  if (!isfinite(temperature) || !isfinite(humidity)) {
    Serial.println(F("Invalid SHT4x values"));
    return false;
  }

  Serial.print(F("Temperature (C): "));
  Serial.println(temperature, 2);
  Serial.print(F("Humidity (% RH): "));
  Serial.println(humidity, 2);
  return true;
}

bool connectWifi() {
  // Prevent Wi-Fi settings from being written to flash on every wakeup.
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);

  if (WIFI_PASSWORD[0] == '\0') {
    WiFi.begin(WIFI_SSID);
  } else {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED &&
         static_cast<uint32_t>(millis() - start) < kWifiTimeoutMs) {
    delay(250); // Yield to the Wi-Fi stack and watchdog.
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("Wi-Fi connection timed out"));
    return false;
  }

  Serial.print(F("Wi-Fi connected; IP: "));
  Serial.println(WiFi.localIP());
  return true;
}

bool sendReading(float temperature, float humidity) {
  char host[kHostCapacity];
  const int hostLength =
      snprintf(host, sizeof(host), "%s.tmep.cz", TMEP_DOMAIN);
  if (hostLength < 0 || static_cast<size_t>(hostLength) >= sizeof(host)) {
    Serial.println(F("TMEP host name too long"));
    return false;
  }

  // dtostrf avoids locale-dependent float formatting and heap-backed String.
  char temperatureText[20];
  char humidityText[20];
  dtostrf(temperature, 0, 2, temperatureText);
  dtostrf(humidity, 0, 2, humidityText);

  char request[kRequestCapacity];
  const int requestLength = snprintf(
      request, sizeof(request),
      "GET /?%s=%s&humV=%s HTTP/1.1\r\n"
      "Host: %s\r\n"
      "Connection: close\r\n\r\n",
      TMEP_TEMPERATURE_GUID, temperatureText, humidityText, host);

  if (requestLength < 0 ||
      static_cast<size_t>(requestLength) >= sizeof(request)) {
    Serial.println(F("TMEP request too long"));
    return false;
  }

  WiFiClient client;
  client.setTimeout(kHttpTimeoutMs);
  if (!client.connect(host, kTmepPort)) {
    Serial.println(F("TMEP connection failed"));
    return false;
  }

  const size_t written = client.write(
      reinterpret_cast<const uint8_t *>(request),
      static_cast<size_t>(requestLength));
  if (written != static_cast<size_t>(requestLength)) {
    Serial.println(F("Incomplete TMEP request"));
    client.stop();
    return false;
  }

  // Read only the HTTP status line. Any HTTP error must not count as success.
  char statusLine[kStatusCapacity];
  size_t used = 0;
  bool complete = false;
  const uint32_t start = millis();

  while (static_cast<uint32_t>(millis() - start) < kHttpTimeoutMs) {
    while (client.available() > 0) {
      const int byte = client.read();
      if (byte < 0) {
        break;
      }
      if (byte == '\n') {
        complete = true;
        break;
      }
      if (byte == '\r') {
        continue;
      }
      if (used + 1 >= sizeof(statusLine)) {
        Serial.println(F("TMEP status line too long"));
        client.stop();
        return false;
      }
      statusLine[used++] = static_cast<char>(byte);
    }
    if (complete || !client.connected()) {
      break;
    }
    delay(10);
  }

  statusLine[used] = '\0';
  client.stop();

  // Accept HTTP/1.x success responses (2xx), not redirects or errors.
  if (!complete || strncmp(statusLine, "HTTP/1.", 7) != 0) {
    Serial.println(F("TMEP reply missing or invalid"));
    return false;
  }
  const char *codeText = strchr(statusLine, ' ');
  if (codeText == nullptr || strlen(codeText) < 4 ||
      codeText[1] < '0' || codeText[1] > '9' ||
      codeText[2] < '0' || codeText[2] > '9' ||
      codeText[3] < '0' || codeText[3] > '9' ||
      (codeText[4] != ' ' && codeText[4] != '\0')) {
    Serial.println(F("TMEP HTTP status malformed"));
    return false;
  }

  const int status = (codeText[1] - '0') * 100 +
                     (codeText[2] - '0') * 10 + (codeText[3] - '0');
  Serial.print(F("TMEP HTTP status: "));
  Serial.println(status);
  return status >= 200 && status < 300;
}

} // namespace

void setup() {
  Serial.begin(115200);
  delay(10);

  float temperature = 0.0f;
  float humidity = 0.0f;

  if (readSensor(temperature, humidity) && connectWifi()) {
    if (sendReading(temperature, humidity)) {
      Serial.println(F("TMEP accepted HTTP request"));
    } else {
      Serial.println(F("TMEP send failed"));
    }
  }

  // All paths end in deep sleep, including sensor/network/server errors.
  sleepUntilNextReading();
}

void loop() {
  // A wakeup from ESP.deepSleep() restarts setup() after a hardware reset.
}
