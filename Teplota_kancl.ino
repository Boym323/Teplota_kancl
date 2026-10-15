// Měření teploty a vlhkosti pomocí ESP8266 a čidla SHT4x.
// Při každém probuzení provedeme JEDNO měření, odešleme je do TMEP.cz
// a vždy přejdeme do hlubokého spánku, i když čidlo nebo síť selže.
// POZOR: Pro probuzení z deep sleep musí být zpravidla GPIO16 (D0) spojeno s RST.

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Adafruit_SHT4x.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "wifi_rtc_cache.h"

// Soukromá konfigurace patří do ignorovaného Teplota_kancl_local.h.
// Bez něj použijeme původní veřejné hodnoty z ukázkové konfigurace.
#if __has_include("Teplota_kancl_local.h")
#include "Teplota_kancl_local.h"
#else
#include "config.example.h"
#endif

// Kompatibilita se soukromými konfiguracemi vytvořenými před zavedením voleb.
#ifndef TMEP_FAST_WIFI
#define TMEP_FAST_WIFI 1
#endif
#ifndef TMEP_FAST_SLEEP
#define TMEP_FAST_SLEEP 1
#endif

namespace {

constexpr uint32_t kWifiTimeoutMs = 15000;
constexpr uint32_t kFastWifiTimeoutMs = 2500; // Poté zkusíme klasické vyhledání AP.
constexpr uint32_t kHttpTimeoutMs = 5000;
constexpr uint16_t kTmepPort = 80;
constexpr size_t kHostCapacity = 64;
constexpr size_t kRequestCapacity = 192;
constexpr size_t kStatusCapacity = 64;

// 32 * 4 = 128 B: nezasahujeme do první části RTC RAM používané OTA bootloaderem.
// V RTC uživatelské paměti je celkem 512 B a offset je v 4B slovech.
constexpr uint32_t kRtcCacheOffsetWords = 32;

static_assert(TMEP_SLEEP_SECONDS > 0, "Deep sleep must be at least 1 second");
static_assert(TMEP_FAST_WIFI == 0 || TMEP_FAST_WIFI == 1, "TMEP_FAST_WIFI must be 0 or 1");
static_assert(TMEP_FAST_SLEEP == 0 || TMEP_FAST_SLEEP == 1, "TMEP_FAST_SLEEP must be 0 or 1");

Adafruit_SHT4x sht4;

// Jedno místo pro uspání: žádná chybová větev nesmí vynechat deep sleep.
void sleepUntilNextReading() {
  Serial.print(F("Deep sleep (seconds): "));
  Serial.println(TMEP_SLEEP_SECONDS);
  Serial.flush(); // Neztratit poslední diagnostickou zprávu.

  uint64_t sleepUs = static_cast<uint64_t>(TMEP_SLEEP_SECONDS) * 1000000ULL;
  const uint64_t maxSleepUs = ESP.deepSleepMax();
  if (maxSleepUs > 0 && sleepUs > maxSleepUs) {
    // RTC časovač neumí neomezeně dlouhý interval.
    sleepUs = maxSleepUs;
  }

#if TMEP_FAST_SLEEP
  // Po ukončení HTTP přenosu nečekáme na běžné vypínání Wi-Fi rádia.
  // Významné pro zařízení napájené baterií; ověřit na konkrétní desce.
  ESP.deepSleepInstant(sleepUs, WAKE_RF_DEFAULT);
#else
  ESP.deepSleep(sleepUs, WAKE_RF_DEFAULT);
#endif
  delay(100); // Pojistka pro nečekaný návrat z volání uspání.
}

// Čidlo načteme ještě před zapnutím Wi-Fi: při chybě šetříme napájení.
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

  // Do TMEP se nesmí odeslat NaN, nekonečno ani údaj mimo rozsah SHT4x.
  if (!isfinite(temperature) || !isfinite(humidity) ||
      temperature < -40.0f || temperature > 125.0f ||
      humidity < 0.0f || humidity > 100.0f) {
    Serial.println(F("Invalid SHT4x values"));
    return false;
  }

  Serial.print(F("Temperature (C): "));
  Serial.println(temperature, 2);
  Serial.print(F("Humidity (% RH): "));
  Serial.println(humidity, 2);
  return true;
}

// AP určíme kombinací SSID/hesla a případně uloženého kanálu + BSSID.
void beginWifi(const WifiRtcCache* hint) {
  const char* password = (WIFI_PASSWORD[0] == '\0') ? nullptr : WIFI_PASSWORD;
  if (hint != nullptr) {
    WiFi.begin(WIFI_SSID, password, hint->channel, hint->bssid);
  } else {
    WiFi.begin(WIFI_SSID, password);
  }
}

bool connectWifi() {
  // Opakované probuzení nesmí opotřebovávat flash zápisem Wi-Fi konfigurace.
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);

  WifiRtcCache cache = {};
  bool cachedAp = false;
#if TMEP_FAST_WIFI
  // RTC RAM vydrží deep sleep, při výpadku napájení může být neplatná.
  cachedAp = ESP.rtcUserMemoryRead(
                 kRtcCacheOffsetWords,
                 reinterpret_cast<uint32_t*>(&cache), sizeof(cache)) &&
             wifiCacheValid(cache, WIFI_SSID);
  if (cachedAp) {
    Serial.println(F("Wi-Fi: cached AP"));
  }
#endif

  beginWifi(cachedAp ? &cache : nullptr);
  const uint32_t start = millis();

  // Omezený celkový čas pro připojení včetně případného přepnutí na běžný scan.
  while (WiFi.status() != WL_CONNECTED &&
         static_cast<uint32_t>(millis() - start) < kWifiTimeoutMs) {
    if (cachedAp &&
        static_cast<uint32_t>(millis() - start) >= kFastWifiTimeoutMs) {
      // AP mohl změnit kanál či BSSID. Nezůstaneme na starých údajích.
      Serial.println(F("Wi-Fi: cached AP failed, scanning"));
      WiFi.disconnect(false);
      beginWifi(nullptr);
      cachedAp = false;
    }
    delay(50); // Uvolnění CPU pro síťový stack a watchdog.
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("Wi-Fi connection timed out"));
    return false;
  }

  Serial.print(F("Wi-Fi connected; IP: "));
  Serial.println(WiFi.localIP());

#if TMEP_FAST_WIFI
  // Nový kanál a BSSID uložíme do RTC RAM pouze po úspěšném připojení.
  // Nezapisujeme do flash a nikdy zde neukládáme Wi-Fi heslo.
  const int channel = WiFi.channel();
  const uint8_t* bssid = WiFi.BSSID();
  if (bssid != nullptr && channel >= 1 && channel <= 13) {
    cache = wifiCacheBuild(WIFI_SSID, static_cast<uint8_t>(channel), bssid);
    ESP.rtcUserMemoryWrite(kRtcCacheOffsetWords,
                           reinterpret_cast<uint32_t*>(&cache), sizeof(cache));
  }
#endif
  return true;
}

// HTTP používá jen krátké pevné buffery; žádné dynamicky rostoucí String.
bool sendReading(float temperature, float humidity) {
  char host[kHostCapacity];
  const int hostLength =
      snprintf(host, sizeof(host), "%s.tmep.cz", TMEP_DOMAIN);
  if (hostLength < 0 || static_cast<size_t>(hostLength) >= sizeof(host)) {
    Serial.println(F("TMEP host name too long"));
    return false;
  }

  // dtostrf zformátuje desetinná čísla bez dočasných String objektů.
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
  // Platí pro DNS, TCP a operace klienta; není to jeden společný deadline.
  client.setTimeout(kHttpTimeoutMs);
  if (!client.connect(host, kTmepPort)) {
    Serial.println(F("TMEP connection failed"));
    return false;
  }

  const size_t written = client.write(
      reinterpret_cast<const uint8_t*>(request),
      static_cast<size_t>(requestLength));
  if (written != static_cast<size_t>(requestLength)) {
    Serial.println(F("Incomplete TMEP request"));
    client.stop();
    return false;
  }

  // Kontrolujeme pouze stavový řádek HTTP, nikoli celé tělo odpovědi.
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

  // Za úspěch počítáme 2xx; přesměrování či chyba serveru se nesmí skrýt.
  if (!complete || strncmp(statusLine, "HTTP/1.", 7) != 0) {
    Serial.println(F("TMEP reply missing or invalid"));
    return false;
  }
  const char* codeText = strchr(statusLine, ' ');
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

} // anonymní jmenný prostor

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

  // Každý běh končí spánkem, také po chybě senzoru, Wi-Fi či serveru.
  sleepUntilNextReading();
}

void loop() {
  // Deep sleep probouzí ESP8266 resetem, proto se další cyklus vrátí do setup().
}
