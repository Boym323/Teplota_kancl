# Technický popis a tok programu

Tento dokument popisuje **aktuální implementaci** v [`Teplota_kancl.ino`](../Teplota_kancl.ino), nikoli zamýšlené budoucí funkce. Firmware je jeden Arduino sketch bez samostatných modulů, externího backendu v repozitáři nebo automatizovaného sestavení.

## Komponenty

| Vrstva | Komponenta | Úloha |
| --- | --- | --- |
| Hardware | ESP8266 | Běh sketch, připojení k Wi-Fi, deep sleep. |
| Hardware | SHT4x | Teplota a relativní vlhkost přes I²C. |
| Knihovna | `ESP8266WiFi.h` | Wi-Fi a TCP klient `WiFiClient`. |
| Knihovna | `Adafruit_SHT4x.h` | Inicializace čidla a `getEvent()`. |
| Externí služba | TMEP.cz | Příjem hodnot přes HTTP endpoint. |
| Diagnostika | `Serial` | Sériový výstup s rychlostí 115200. |

Přenos probíhá směrem **ESP8266 → TMEP.cz**. Sketch nemá serverovou část a neposkytuje API ani webové rozhraní.

## Sekvence při startu

```text
RESET / napájení / probuzení z deep sleep
  |
  v
setup()
  |- Serial.begin(115200)
  |- sht4.begin()
  |    \- pokud selže: nekonečné čekání
  |- výpis sériového čísla senzoru
  |- sht4.setPrecision(SHT4X_HIGH_PRECISION)
  |- WiFi.begin(ssid)                 [bez hesla]
  |    \- čekání bez časového limitu na WL_CONNECTED
  |- výpis získané IP adresy
  \- sht4.setHeater(SHT4X_NO_HEATER)
  |
  v
loop()
  |- sht4.getEvent(&humidity, &temp)
  |- výpis teploty, vlhkosti a délky čtení
  |- host = domain + ".tmep.cz"
  |- client.connect(host, 80)
  |    \- selhání -> return z loop() bez deep sleep
  |- sestavení URL s guid=<teplota>&humV=<vlhkost>
  |- odeslání HTTP GET přes client.print()
  |- čekání na client.available() > 0
  |    \- timeout po 5000 ms -> client.stop(); return bez deep sleep
  \- ESP.deepSleep(sleep * 1000000)
          |
          \- nové spuštění zařízení po probuzení
```

Výše uvedený diagram vyjadřuje pořadí a větvení kódu. Vstupní veličiny a časové prodlevy se mohou lišit podle hardwaru a sítě.

## Inicializace senzoru

1. `sht4.begin()` – pokus o nalezení senzoru. Při neúspěchu následuje `while (1) delay(1)`.
2. `sht4.readSerial()` – výpis sériového čísla do diagnostiky; **není odesíláno do TMEP**.
3. `sht4.setPrecision(SHT4X_HIGH_PRECISION)` – režim vysoké přesnosti.
4. `sht4.setHeater(SHT4X_NO_HEATER)` – vypnutí ohřevu senzoru.
5. `sht4.getEvent(&humidity, &temp)` – naplní dvě události s čerstvými daty.

Hodnoty se v programu čtou z `temp.temperature` a `humidity.relative_humidity`. Sketch nezahrnuje kalibraci, filtrování, kontrolu rozsahu ani zvláštní ošetření chybových hodnot.

## Přenos dat

Hostitel se vytváří spojováním `domain` a přípony `.tmep.cz`. TCP klient komunikuje přes **port 80** bez TLS.

Tvar požadavku:

```http
GET /?<guid>=<teplota>&humV=<vlhkost> HTTP/1.1
Host: <domain>.tmep.cz
Connection: close

```

Například při doméně `mps`, GUID `temp` a ilustračních měřeních:

```text
http://mps.tmep.cz/?temp=23.4&humV=48.1
```

Důležité podrobnosti implementace:

- `guid` je v URL použit jako **název** parametru teploty, nikoli jako samostatný parametr `guid=...`.
- `humV` je pevný název parametru relativní vlhkosti.
- Čísla jsou převáděna do řetězce pomocí operátorů Arduino `String`; není zde explicitní řízení počtu desetinných míst, zaokrouhlování ani URL encoding.
- `client.connect(host, 80)` pouze zjišťuje možnost navázání spojení.
- `client.print(...)` odešle HTTP hlavičky a URL, **neověří však výsledek na aplikační vrstvě**.
- `client.available() > 0` znamená, že jsou dostupná data ze spojení, **ne** že server odpověděl `200 OK` nebo zapsal údaje.
- HTTP odpověď se vůbec nečte ani neparsuje. Kód explicitně nevolá `client.stop()` na úspěšné větvi, následně požádá o hluboký spánek.

## Časování a deep sleep

Aktuální konstanta `sleep` má hodnotu `60` a používá se jako `ESP.deepSleep(sleep*1000000)`, tedy **60 000 000 µs = 60 s**.

V přirozeném úspěšném cyklu platí zhruba:

```text
start + inicializace + Wi-Fi + měření + přenos + 60 s spánku
```

**Nejde o periodický časovač přesně po 60 s.** Wi-Fi může přidat proměnlivou prodlevu. Každé probuzení z deep sleep znamená nový start programu. Pro automatické probuzení je na ESP8266 typicky potřeba propojení **GPIO16–RST**.

### Chování při chybách

| Stav | Skutečné chování |
| --- | --- |
| SHT4x není detekován | Nekonečná smyčka v `setup()`. |
| Wi-Fi není dosažitelná | Nekonečné čekání v `setup()` po 500ms intervalech. |
| TCP spojení selže | `loop()` končí pomocí `return`, **bez** deep sleep. |
| Server nevrátí data do 5 s | `client.stop()`, `return` z `loop()`, **bez** deep sleep. |
| Ze serveru dorazí libovolná data | Následuje deep sleep, i když HTTP status neznáme. |

Proto může výpadek vzdálené služby vést k mnoha rychlým opakovaným připojením a vyšší spotřebě. Aktuální firmware neobsahuje exponenciální backoff ani vytrvalou frontu neodeslaných měření.

## Paměť, síť a hranice řešení

- Každý cyklus vytváří místní `WiFiClient` a sestavuje cestu pomocí `String`.
- Mezi jednotlivými starty po deep sleep se nepřenáší aplikační stav.
- Zařízení si neukládá historii lokálně a při ztrátě spojení se měření **nezařadí k pozdějšímu odeslání**.
- Nejsou implementovány watchdog / retry strategie na aplikační úrovni (mimo běžné chování knihoven), MQTT, OTA, webová správa nebo lokální API.
- Kód používá `char host[50]` a funkce `strcpy/strcat`; při nevhodně dlouhé hodnotě `domain` by mohlo dojít k překročení pole. Současná krátká hodnota tuto situaci sama o sobě nevyvolává, ale budoucí změny musí ověřit délku.
- `sleep*1000000` není obaleno kontrolou rozsahu; u výrazně delších intervalů je potřeba zohlednit limity datových typů a `ESP.deepSleepMax()`.

## Případné rozšíření (není implementováno)

- Konečný timeout připojování k Wi-Fi.
- Odstupňované opakování odesílání a spánek i při chybě.
- Parsování HTTP statusu a bezpečné ošetření odpovědi.
- Oddělená neveřejná konfigurace a práce s tajemstvími.
- Validace měření, laditelné časování, úspora energie.
- Automatické sestavení a test kompatibility Arduino core / knihoven.

Žádný z těchto návrhů není třeba zaměňovat za vlastnost současného firmware.
