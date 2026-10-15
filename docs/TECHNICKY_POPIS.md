# Technický popis a tok programu

Platí pro firmware [`Teplota_kancl.ino`](../Teplota_kancl.ino) ve větvi s optimalizacemi. Sketch používá **ESP8266 Arduino core**, senzor **SHT4x** přes I²C a službu **TMEP.cz** přes nešifrované HTTP.

## Architektura

| Vrstva | Komponenta | Úloha |
| --- | --- | --- |
| ESP8266 | Arduino sketch | Spuštění, snímání hodnot, Wi-Fi, deep sleep |
| SHT4x | Adafruit SHT4x Library | Změření teploty a relativní vlhkosti |
| Síť | ESP8266WiFi + WiFiClient | TCP spojení a HTTP GET |
| Cíl | TMEP.cz | Přijímá hodnoty pod nastavenými parametry |
| Konfigurace | `Teplota_kancl_local.h` / `config.example.h` | Wi-Fi, subdoména TMEP, GUID, spánek |
| Diagnostika | `Serial` | Výpis měření, chyb a HTTP statusu |

Zařízení neposkytuje webový server, vlastní API, OTA ani lokální historii.

## Průběh jednoho cyklu

```text
RESET / napájení / probuzení
  |
  v
setup()
  |- Serial.begin(115200)
  |- readSensor()
  |   |- sht4.begin()
  |   |- high precision, no heater
  |   |- getEvent() + kontrola návratové hodnoty
  |   \- kontrola konečnosti a rozsahu [-40;125] °C, [0;100] % RH
  |       \- chyba: přeskočit Wi-Fi a HTTP
  |- connectWifi()
  |   |- WiFi.persistent(false) a WIFI_STA
  |   |- WiFi.begin(SSID[, heslo])
  |   \- max. 15 sekund do WL_CONNECTED
  |       \- chyba: přeskočit HTTP
  |- sendReading()
  |   |- bezpečné vytvoření hostitele a GET zprávy (pevné buffery)
  |   |- TCP spojení s <subdomain>.tmep.cz:80
  |   |- kompletní odeslání GET
  |   |- max. 5 sekund pro první řádek HTTP odpovědi
  |   \- kontrola stavového kódu 2xx
  \- sleepUntilNextReading()
      \- ESP.deepSleep(seconds * 1 000 000)
            |
            \- probuzení provede reset, následuje znovu setup()
```

`loop()` je prázdný. Všechny větve programu se po jediném pokusu vrací do hlubokého spánku. To zabraňuje opakovanému zahlcování serveru a zbytečnému aktivnímu běhu při běžných selháních.

## SHT4x

`sht4.begin()` musí uspět. Program nastaví `SHT4X_HIGH_PRECISION` a `SHT4X_NO_HEATER`. Pak zavolá `getEvent(&humidity, &temperature)`, který podle Adafruit API vrací `bool`. Při selhání čtení nebo neplatných hodnotách se odeslání vynechá.

Naměřená teplota musí být v rozsahu **−40 až 125 °C**, relativní vlhkost **0 až 100 % RH**, obě hodnoty musí být konečná čísla. Firmware neprovádí statistické vyhlazování, offset kalibraci ani časové značkování.

## Připojení k Wi-Fi

Konfigurace používá makra z lokálního `Teplota_kancl_local.h` nebo veřejného `config.example.h`. Při neprázdném `WIFI_PASSWORD` se používá `WiFi.begin(ssid, password)`, jinak `WiFi.begin(ssid)`. Funkce `WiFi.persistent(false)` brání opakovaným zápisům Wi-Fi nastavení do flash. Připojení má timeout **15 000 ms** s `delay(50)` ve smyčce.

Tento limit se týká čekání na stav `WL_CONNECTED`. Nevytváří celkovou horní mez pro kompletní HTTP síťový cyklus ani pro DNS.

### Rychlé opětovné připojení pomocí RTC RAM

Je-li `TMEP_FAST_WIFI=1`, uloží se po úspěšném spojení do RTC paměti (od offsetu 32, tedy za prvními 128 bajty) **BSSID, kanál a kontrolní otisk SSID** s kontrolním součtem. RTC záznam lze znovu použít po probuzení z deep sleep. Neobsahuje heslo Wi-Fi.

Při platném záznamu začne `WiFi.begin()` přímo s kanálem a BSSID; po **2 500 ms** bez připojení následuje `WiFi.disconnect(false)` a nové spojení bez nápovědy. I s fallbackem zůstává celkový limit Wi-Fi **15 000 ms**. Změna SSID, chybné BSSID či poškozený záznam znamenají standardní vyhledávání. Konfigurace `TMEP_FAST_WIFI=0` cache zcela vypne.

Validace je definována v [`wifi_rtc_cache.h`](../wifi_rtc_cache.h) a ověřena hostitelskými testy. FNV kontrolní otisk **není kryptografická ochrana**. Zrychlení je očekávané zejména na stabilním přístupovém bodu, zatím však nebylo změřeno na fyzickém zařízení.

## Formát HTTP

Tvar požadavku:

```http
GET /?<guid>=<teplota>&humV=<vlhkost> HTTP/1.1
Host: <domain>.tmep.cz
Connection: close

```

Pro výchozí konfiguraci a *ilustrační* naměřené hodnoty:

```text
http://mps.tmep.cz/?temp=23.50&humV=45.20
```

- `TMEP_TEMPERATURE_GUID` představuje **název** parametru pro teplotu (např. `temp`).
- `humV` je přímo zapsaný název parametru pro vlhkost.
- Teplota i vlhkost se formátují na **dvě desetinná místa**.
- Pro vytvoření hostitele a celé HTTP hlavičky se používá `snprintf` s kontrolou délky; pro float `dtostrf`. Nevznikají pomocné dynamické objekty Arduino `String`.
- Pro komunikaci slouží `WiFiClient` na **portu 80**, nikoliv TLS.
- Po odeslání se načítá pouze první řádek odpovědi; kontroluje se tvar `HTTP/1.x` a tříciferný status. **2xx** je vyhodnoceno jako úspěšná HTTP odpověď.
- Ověření celého těla odpovědi ani potvrzení uložení historie na straně TMEP není implementováno.
- Délka první HTTP řádky je omezená fixním bufferem; příliš dlouhá odpověď je chyba.

## Deep sleep a časování

Konstanta `TMEP_SLEEP_SECONDS` má v ukázkové konfiguraci hodnotu **60UL**. Při úspěchu i chybě proběhne:

```cpp
ESP.deepSleep(static_cast<uint64_t>(TMEP_SLEEP_SECONDS) * 1000000ULL);
```

64bitové násobení brání přetečení při převodu sekund na mikrosekundy. Firmware navíc omezuje požadovanou dobu podle `ESP.deepSleepMax()`. Při zapnuté volbě `TMEP_FAST_SLEEP=1` používá `ESP.deepSleepInstant()`, který zkracuje čekání při vypínání rádia; vypnutím volby se vrátí na běžný `ESP.deepSleep()`. Pro probuzení je typicky nutné propojení GPIO16–RST.

Po každém resetu začíná nové měření. Čas mezi měřeními je:

```text
doba startu + čtení senzoru + připojování k Wi-Fi + HTTP komunikace + spánek
```

Nejde o přesný periodický scheduler.

## Chybové scénáře

| Stav | Chování |
| --- | --- |
| Senzor nedetekován / čtení selhalo | Nevysílá data, přejde do deep sleep. |
| Neplatná teplota nebo vlhkost | Nevysílá data, přejde do deep sleep. |
| Wi-Fi není připojená do 15 s | Nevysílá data, přejde do deep sleep. |
| DNS / TCP spojení selže | Loguje chybu a přejde do deep sleep. |
| HTTP požadavek není celý odeslán | Loguje chybu a přejde do deep sleep. |
| Nedojde kompletní stavový řádek do 5 s | Loguje chybu a přejde do deep sleep. |
| HTTP status 3xx / 4xx / 5xx | Loguje HTTP status a přejde do deep sleep. |
| HTTP status 2xx | Zaznamená úspěch HTTP a přejde do deep sleep. |

## Co firmware zatím neumí

- Neprovádí okamžité opakované pokusy ani exponenciální backoff.
- Nemá lokální zásobník neodeslaných měření.
- Neumí HTTPS/TLS, OTA, konfiguraci přes web ani NTP.
- Nepotvrzuje aplikací TMEP uložené hodnoty.
- Přenos přes HTTP zůstává čitelný pro prostředníky na síti.
- Limit délky DNS/TCP připojení nelze zaměňovat s jedním celkovým hard deadlinem.

## Automatické ověření

[Workflow Arduino CLI](../.github/workflows/firmware-compile.yml) testuje validaci RTC cache v C++ na hostitelském systému a kompiluje pro `esp8266:esp8266:nodemcuv2`. Bez fyzického senzoru a Wi-Fi to **neověřuje spotřebu, probouzení, správnost I²C ani komunikaci s TMEP**.
