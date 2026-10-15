# Teplota_kancl

Jednoduchý Arduino sketch pro **ESP8266** se senzorem **SHT4x**, který změří teplotu a relativní vlhkost a odešle obě hodnoty na [TMEP.cz](https://www.tmep.cz/).

> **Stav projektu:** Dokumentace popisuje chování aktuálního souboru [`Teplota_kancl.ino`](Teplota_kancl.ino). Nejde o hotový, otestovaný výrobek: repozitář neobsahuje automatické testy, sestavovací konfiguraci ani definici konkrétní vývojové desky. Některé kroky zapojení je nutné ověřit podle použitého modulu.

## Co firmware dělá

1. Při startu otevře sériovou linku na **115 200 baudů** a inicializuje SHT4x.
2. Zvolí **vysokou přesnost** měření a **vypnuté vyhřívání** senzoru.
3. Připojí ESP8266 k Wi-Fi.
4. Změří teplotu (°C) a relativní vlhkost (% RH).
5. Pomocí TCP klienta odešle **HTTP GET na port 80** do domény `<domain>.tmep.cz` s parametry `<guid>` a `humV`.
6. Pokud od serveru dorazí nějaká data, uspí čip pomocí `ESP.deepSleep()` na nastavenou dobu; probuzení vyvolá nový start sketch.

**Důležité:** nastavených `60` sekund označuje **dobu spánku po odeslání**, nikoli pevný interval mezi měřeními. Skutečný odstup zahrnuje ještě start, připojení k Wi-Fi a měření. Sketch neověřuje HTTP stavový kód ani obsah odpovědi.

## Hardware a zapojení

- Vývojová deska nebo modul s čipem **ESP8266** (konkrétní model není v repozitáři uveden).
- **SHT4x** senzor teploty a vlhkosti s rozhraním I²C, vhodný pro napěťové úrovně ESP8266.
- Napájení a kabeláž podle zvolené desky.
- Wi-Fi síť s přístupem na službu TMEP.

Typické propojení na deskách, které používají obvyklé piny ESP8266:

| SHT4x | ESP8266 | Poznámka |
| --- | --- | --- |
| VCC / VIN | 3V3 | Řiďte se parametry **konkrétního modulu** senzoru. |
| GND | GND | Společná zem. |
| SDA | GPIO4 | Na mnoha deskách NodeMCU / D1 mini označeno `D2`. |
| SCL | GPIO5 | Na mnoha deskách NodeMCU / D1 mini označeno `D1`. |

Pro automatické probuzení po `ESP.deepSleep()` bývá na ESP8266 nutné propojit **GPIO16 (často D0) s RST**. U některých desek může být propojení již řešeno jinak. Bez funkčního propojení se zařízení může uspat po prvním měření a samo se neprobudí. Před zapojením vždy ověřte schéma své desky.

Podrobnosti: [Zapojení a nahrání firmware](docs/INSTALACE.md).

## Rychlý start (Arduino IDE)

1. Nainstalujte [Arduino IDE](https://www.arduino.cc/en/software).
2. V **Předvolbách** přidejte URL správce desek ESP8266:
   `https://arduino.esp8266.com/stable/package_esp8266com_index.json`
3. Ve **Správci desek** nainstalujte **esp8266 by ESP8266 Community** a vyberte přesný model své desky.
4. Ve **Správci knihoven** nainstalujte **Adafruit SHT4x Library** a nabídnuté závislosti (zejména **Adafruit BusIO** a **Adafruit Unified Sensor**).
5. Zapojte senzor, otevřete `Teplota_kancl.ino` a upravte síť a parametry TMEP podle následující kapitoly.
6. Vyberte port, nahrajte firmware a otevřete **Sériový monitor** na 115 200 baudů.
7. Ověřte detekci senzoru, připojení k Wi-Fi, hodnoty měření a příjem dat na TMEP. Poté ověřte probuzení po spánku.

## Konfigurace

V souboru `Teplota_kancl.ino` jsou přímo v kódu definovány:

| Proměnná | Účel | Aktuální význam |
| --- | --- | --- |
| `ssid` | Název Wi-Fi sítě | Firmware se připojuje pomocí `WiFi.begin(ssid)` **bez hesla**. |
| `domain` | Subdoména TMEP | K hodnotě se automaticky připojí `.tmep.cz`. |
| `guid` | Název parametru teploty | Použije se jako klíč prvního parametru dotazu. |
| `sleep` | Doba hlubokého spánku | Výchozí hodnota ve sketchi je **60 sekund**. |

Příklad očekávaného tvaru odesílaného požadavku (ilustrační hodnoty):

```text
GET /?temp=23.5&humV=45.2 HTTP/1.1
Host: mps.tmep.cz
Connection: close
```

V tomto příkladu je `temp` konfigurované `guid`, zatímco `humV` je **pevně zapsaný název parametru vlhkosti** v kódu. Vlastní hodnoty domény, GUID a názvy veličin musí souhlasit s nastavením měření na TMEP.

### Wi-Fi s heslem

Aktuální sketch volá pouze `WiFi.begin(ssid)`, tedy neobsahuje aktivní předání hesla. U chráněné sítě musí být zdrojový kód **upraven**, například takto:

```cpp
const char* ssid = "NAZEV_SITE";
const char* wifiPassword = "HESLO_SITE";

// v setup():
WiFi.begin(ssid, wifiPassword);
```

Nezadávejte skutečná přístupová hesla do veřejně verzovaného souboru. Bezpečnější správa konfigurace je námětem pro další úpravu projektu, **není součástí současné implementace**.

## Diagnostika a známá omezení

- Pokud se SHT4x nenajde, `setup()` zůstane v nekonečné smyčce; firmware dál neměří.
- Pokud se nepodaří připojit k Wi-Fi, čeká `setup()` **bez časového limitu**.
- Při selhání TCP spojení nebo při čekání na první data déle než **5 sekund** se provede `return` z `loop()` **před deep sleep**. Další průchod `loop()` tak může začít bez plánované pauzy a opakovat pokusy rychle za sebou.
- Kód neověřuje status `200 OK`, nenačítá/parsu je tělo odpovědi a nepotvrzuje uložení měření v TMEP.
- Data odcházejí přes **nešifrované HTTP**; citlivé informace se proto nesmějí přidávat do URL.
- Konfigurace je v kódu a firmware neobsahuje OTA aktualizace, frontu offline měření ani mechanismus pro správu času.

Podrobné vysvětlení toku programu: [Technický popis](docs/TECHNICKY_POPIS.md). Praktické kroky při problémech: [Provoz a řešení problémů](docs/PROVOZ_A_PROBLEMY.md). Bezpečnostní a provozní poznámky: [Bezpečnost](docs/BEZPECNOST.md).

## Struktura repozitáře

```text
.
├── README.md                       # Tento přehled
├── Teplota_kancl.ino               # Vlastní firmware ESP8266
└── docs/
    ├── INSTALACE.md                # Zapojení a Arduino IDE
    ├── TECHNICKY_POPIS.md          # Tok programu, protokol a časování
    ├── PROVOZ_A_PROBLEMY.md        # Diagnostika a troubleshooting
    └── BEZPECNOST.md               # Rizika a doporučená opatření
```

## Zdroje a související návody

- [Arduino core pro ESP8266 – instalace](https://arduino-esp8266.readthedocs.io/en/latest/installing.html)
- [Adafruit – použití SHT4x s Arduino](https://learn.adafruit.com/adafruit-sht40-temperature-humidity-sensor/arduino)
- [ESP8266 core – API a deep sleep](https://github.com/esp8266/Arduino/blob/master/doc/libraries.rst)
- [TMEP.cz](https://www.tmep.cz/) a [wiki TMEP](http://wiki.tmep.cz/)

**Licence:** V repozitáři není uveden licenční soubor. Bez výslovně přidělené licence nelze předpokládat oprávnění k dalšímu šíření nebo úpravám nad rámec zákonných výjimek.