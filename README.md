# Teplota_kancl

Firmware pro **ESP8266** a teplotně-vlhkostní senzor **SHT4x**, který po probuzení změří obě veličiny, odešle je do [TMEP.cz](https://www.tmep.cz/) a přejde do hlubokého spánku. Projekt používá Arduino framework, komunikaci I²C a HTTP GET.

> **Stav:** Firmware je určen pro konkrétní zapojení ESP8266 a senzoru SHT4x. Automatický sestavovací test ověřuje pouze kompilaci pro NodeMCU ESP8266, **ne funkci na reálném zařízení**. Konkrétní desku a správné zapojení je třeba ověřit.

## Jak to funguje

1. ESP8266 inicializuje SHT4x (vysoká přesnost, bez ohřevu).
2. Jednou změří teplotu (°C) a relativní vlhkost (% RH). Neplatné nebo chybové hodnoty neodesílá.
3. Připojí se k Wi-Fi. Po **15 sekundách** bez úspěchu pokus ukončí.
4. Odešle na TMEP.cz HTTP GET přes port **80**.
5. Maximálně **5 sekund** čeká na první řádek HTTP odpovědi. Za úspěšnou odpověď považuje kód **2xx** (přijetí a uložení měření je nutné ještě ověřit v TMEP).
6. **Po úspěchu i chybě** přejde do deep sleep na výchozích **60 sekund**.

Nastavená hodnota 60 sekund představuje **dobu spánku**, ne přesný interval mezi dvěma měřeními. Skutečný interval zahrnuje i start ESP8266 a síťovou komunikaci. Při automatickém probouzení bývá nutné propojit **GPIO16 (D0) s RST**; záleží na desce.

## Hardware

- Deska s **ESP8266** a možností nahrání firmware.
- Senzor řady **SHT4x** s I²C rozhraním.
- Napájení, vodiče a Wi-Fi připojení.

Typické I²C zapojení NodeMCU / D1 mini (ověřte schéma vlastní desky):

| SHT4x | ESP8266 |
| --- | --- |
| VCC / VIN | 3V3 (pokud to podporuje konkrétní modul) |
| GND | GND |
| SDA | GPIO4 / D2 |
| SCL | GPIO5 / D1 |

Nepřipojujte 5V I²C signály přímo na GPIO ESP8266.

[Podrobný návod k zapojení a instalaci](docs/INSTALACE.md)

## Rychlý start

1. Nainstalujte [Arduino IDE](https://www.arduino.cc/en/software).
2. Přidejte URL správce desek ESP8266:
   `https://arduino.esp8266.com/stable/package_esp8266com_index.json`
3. Nainstalujte balíček **esp8266 by ESP8266 Community** a knihovnu **Adafruit SHT4x Library** včetně požadovaných závislostí.
4. Zkopírujte [`config.example.h`](config.example.h) do **`config.h`** ve složce projektu. Upravený `config.h` je v [`.gitignore`](.gitignore), aby nedocházelo k náhodnému zveřejnění hesla.
5. Upravte SSID, volitelné heslo Wi-Fi a parametry TMEP v `config.h`.
6. Otevřete `Teplota_kancl.ino`, zvolte odpovídající desku a port a klikněte na **Upload**.
7. Otevřete **Serial Monitor** na 115 200 baudů a ověřte výsledky na TMEP i další probuzení po spánku.

**Zpětná kompatibilita:** Bez lokálního `config.h` firmware použije výchozí hodnoty v `config.example.h`, které odpovídají původní veřejné konfiguraci repozitáře. Před nasazením doporučujeme vytvořit vlastní `config.h`.

### Konfigurace

| Makro v `config.h` | Účel | Výchozí hodnota |
| --- | --- | --- |
| `WIFI_SSID` | Název Wi-Fi | Původní veřejná síť projektu |
| `WIFI_PASSWORD` | Wi-Fi heslo; prázdný řetězec pro otevřenou síť | Prázdný řetězec |
| `TMEP_DOMAIN` | Subdoména bez `.tmep.cz` | `mps` |
| `TMEP_TEMPERATURE_GUID` | Název parametru teploty | `temp` |
| `TMEP_SLEEP_SECONDS` | Doba hlubokého spánku v sekundách | `60UL` |

Firmware odešle požadavek v tomto tvaru (ilustrační hodnoty):

```http
GET /?temp=23.50&humV=45.20 HTTP/1.1
Host: mps.tmep.cz
Connection: close
```

Název `humV` je pevně zakódovaný parametr pro relativní vlhkost. Konfiguraci subdomény, parametru teploty a přijetí dat ověřte v účtu TMEP.

## Spolehlivost a omezení

- Při nedostupném senzoru, Wi-Fi nebo HTTP serveru firmware **přejde do spánku** a pokusí se o nové měření až při dalším probuzení.
- SHT4x kontroluje návratovou hodnotu měření a konečnost i fyzikální rozsah hodnot.
- Přenos nepoužívá dynamické `String` pro sestavení HTTP požadavku. Pevné buffery kontrolují délku.
- HTTP status **2xx** potvrzuje pouze úspěšnou HTTP odpověď; **nezaručuje** ověření výsledné historie v TMEP.
- Přenos je stále **nešifrované HTTP**. Neposílejte tajné údaje v URL; přechod na HTTPS vyžaduje ověření kompatibility TMEP.
- Chybí lokální ukládání měření, opakované doručování, OTA a časová synchronizace. Po chybném cyklu se naměřená hodnota neuchovává.
- Heslo odstraněné ze zdrojového kódu může zůstat v git historii; pokud bylo platné, musí být **změněno**.

## Kompilace a CI

Workflow [`Firmware compile`](.github/workflows/firmware-compile.yml) sestavuje sketch pro `esp8266:esp8266:nodemcuv2` pomocí Arduino CLI. Spouští se při změnách firmware / hlaviček na PR a při push do `main` (lze také ručně). Nejde o test funkčnosti čidla, HTTP odezvy TMEP nebo skutečné spotřeby.

## Dokumentace

- [Instalace a zapojení](docs/INSTALACE.md)
- [Technický popis](docs/TECHNICKY_POPIS.md)
- [Provoz a řešení problémů](docs/PROVOZ_A_PROBLEMY.md)
- [Bezpečnost](docs/BEZPECNOST.md)

## Struktura

```text
.
├── .github/workflows/firmware-compile.yml
├── .gitignore
├── config.example.h
├── README.md
├── Teplota_kancl.ino
└── docs/
    ├── BEZPECNOST.md
    ├── INSTALACE.md
    ├── PROVOZ_A_PROBLEMY.md
    └── TECHNICKY_POPIS.md
```

## Užitečné odkazy

- [Instalace Arduino core pro ESP8266](https://arduino-esp8266.readthedocs.io/en/latest/installing.html)
- [Adafruit SHT4x – Arduino](https://learn.adafruit.com/adafruit-sht40-temperature-humidity-sensor/arduino)
- [TMEP.cz](https://www.tmep.cz/)

**Licence:** Repozitář zatím neobsahuje licenční soubor. Pravidla užití mimo zákonné výjimky tedy nejsou výslovně stanovena.
