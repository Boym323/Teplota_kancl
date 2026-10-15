# Zapojení a instalace

Tento návod se vztahuje ke sketchi [`Teplota_kancl.ino`](../Teplota_kancl.ino). Repozitář **neurčuje konkrétní vývojovou desku ani model breakout modulu SHT4x**, proto jsou označení vývodů orientační a je nutné je zkontrolovat proti dokumentaci skutečného hardwaru.

## 1. Co připravit

- Modul / vývojovou desku **ESP8266** s možností nahrávání přes USB–UART nebo externí převodník.
- Senzor řady **SHT4x** (teplota + relativní vlhkost) s I²C; konkrétní varianta není zdokumentována.
- Vodiče, vhodné stabilní napájení a případně USB kabel umožňující **přenos dat**.
- Počítač s Arduino IDE a ovladačem USB–UART, pokud jej vyžaduje použitá deska.
- Wi-Fi připojení a nastavený cíl v TMEP.cz.

**Bezpečnost napájení:** ESP8266 používá logické úrovně **3,3 V**. Nepřipojujte 5V I²C signály přímo k jeho GPIO. Napájecí rozsah čidla se liší podle konkrétního breakout modulu; neodvozujte jej jen podle nápisu `VIN`.

## 2. Typické I²C zapojení

| Vodič na senzoru | ESP8266 (GPIO) | Časté označení na NodeMCU / D1 mini |
| --- | --- | --- |
| VCC / VIN | 3V3 | 3V3 |
| GND | GND | G / GND |
| SDA | GPIO4 | D2 |
| SCL | GPIO5 | D1 |

Tyto piny odpovídají běžnému implicitnímu I²C zapojení desek ESP8266. **Sketch explicitně nevolá `Wire.begin(sda, scl)` ani piny nekonfiguruje**, takže používá inicializaci a výchozí mapování knihovny/core pro zvolenou desku. Pokud má vaše deska jiné mapování, bude potřeba upravit inicializaci I²C v programu a otestovat ji.

Zapojení pro automatické probuzení z hlubokého spánku:

- **GPIO16 ↔ RST** (často pin označený `D0` ↔ `RST`).
- Neprovádějte bezmyšlenkovitě na deskách s vlastním obvodem probouzení; nejdříve ověřte schéma a doporučení výrobce.
- ESP8266 po uplynutí doby spánku probouzí resetem, nikoli návratem na následující řádek `loop()`.

Oficiální vysvětlení: [ESP8266 core – deep sleep](https://github.com/esp8266/Arduino/blob/master/doc/libraries.rst).

## 3. Arduino IDE a knihovny

1. Stáhněte a spusťte [Arduino IDE](https://www.arduino.cc/en/software).
2. V **File / Preferences** (nebo odpovídající české lokalizaci) otevřete pole **Additional boards manager URLs**.
3. Přidejte:
   
   ```text
   https://arduino.esp8266.com/stable/package_esp8266com_index.json
   ```
4. Ve **Správci desek** vyhledejte `esp8266` a nainstalujte balíček **esp8266 by ESP8266 Community**.
5. Ve **Správci knihoven** vyhledejte `Adafruit SHT4x Library` a nainstalujte ji. Pokud nabídne instalaci závislostí, zvolte **Install all**. Sketch potřebuje také typy z **Adafruit Unified Sensor** a obvykle knihovnu **Adafruit BusIO**; správce knihoven může přidat i další závislosti deklarované Adafruitem.
6. Otevřete soubor `Teplota_kancl.ino` v Arduino IDE.

**Použité hlavičky v tomto projektu:**

```cpp
#include <ESP8266WiFi.h>
#include "Adafruit_SHT4x.h"
```

První poskytuje ESP8266 Arduino core, druhou externí Adafruit knihovna. Projekt nemá `platformio.ini` ani `arduino-cli.yaml` a nezamyká verze závislostí.

## 4. Nastavení Wi-Fi a TMEP

Firmware čte nastavení z **`config.h`**, pokud v adresáři sketch existuje. Jinak použije [`config.example.h`](../config.example.h) s původními veřejnými výchozími hodnotami. Tím je zachována kompatibilita, ale doporučujeme vytvořit lokální nastavení vždy před nasazením.

1. Zkopírujte `config.example.h` na `config.h` ve stejné složce jako `Teplota_kancl.ino`.
2. V souboru `config.h` upravte:

```cpp
#define WIFI_SSID "NAZEV_SITE"
#define WIFI_PASSWORD "HESLO_SITE"  // Pro otevřenou síť použijte ""
#define TMEP_DOMAIN "MOJE_SUBDOMENA"
#define TMEP_TEMPERATURE_GUID "NAZEV_MERENI"
#define TMEP_SLEEP_SECONDS 60UL
```

3. `config.h` je v `.gitignore`: v běžné práci nebude omylem přidán do nového commitu. **Před každým zveřejněním změn přesto ověřte, že neobsahují hesla.**
4. Pokud si v původní verzi upravujete síť přímo v `.ino`, přeneste tyto hodnoty do `config.h`. Od této verze se přímé staré proměnné `ssid` / `domain` / `guid` / `sleep` nepoužívají.

Makra znamenají: `WIFI_SSID` název Wi-Fi; `WIFI_PASSWORD` heslo (prázdné = otevřená síť); `TMEP_DOMAIN` subdoména bez `.tmep.cz`; `TMEP_TEMPERATURE_GUID` parametr teploty; `TMEP_SLEEP_SECONDS` dobu spánku v sekundách **po** každém pokusu o měření a odeslání. Po změně nastavení je nutné firmware znovu nahrát.

Případné starší heslo viditelné v historii repozitáře je nutné považovat za prozrazené a v případě platnosti ho změnit.

### Příklad adresy pro TMEP

Pokud `TMEP_DOMAIN` = `priklad` a `TMEP_TEMPERATURE_GUID` = `teplota`, firmware odešle:

```text
http://priklad.tmep.cz/?teplota=22.4&humV=41.7
```

Čísla jsou **ilustrační**. Název `humV` je přímo zapsaný do programu. Tvar parametrů musí odpovídat vašemu měření na TMEP; konfiguraci cílové služby tento repozitář neobsahuje.

## 5. Nahrání

1. Připojte ESP8266 datovým USB kabelem.
2. V **Tools / Board** zvolte konkrétní model desky.
3. V **Tools / Port** zvolte sériový port zařízení.
4. Proveďte kompilaci pomocí **Verify**.
5. Spusťte **Upload**. Některé desky vyžadují manuální uvedení do režimu nahrávání; řiďte se jejich dokumentací.
6. Otevřete **Serial Monitor** s rychlostí **115200**.
7. Očekávejte výpis teploty a vlhkosti, připojení Wi-Fi a v případě úspěchu `TMEP HTTP status: 200` (nebo jiný status 2xx). Při chybě se zařízení rovněž uspí.
8. Ověřte, že se hodnoty skutečně zobrazují na TMEP.cz. Ani HTTP status 2xx sám o sobě jednoznačně neprokazuje uložení historie měření.
9. Ověřte i další cyklus po probuzení z deep sleep.

Pro ladění probouzení lze **dočasně** vynechat nebo upravit deep sleep, ale to už je změna funkčnosti sketch, nikoliv jeho původní chování.

## 6. Před uvedením do provozu

- [ ] Napětí a I²C signály odpovídají specifikacím senzoru a desky.
- [ ] SHT4x je detekován a měří věrohodné hodnoty.
- [ ] Wi-Fi má internetový přístup a DNS přeloží nastavenou doménu TMEP.
- [ ] Parametry domény / měření odpovídají účtu TMEP.
- [ ] Přijímané hodnoty jsou viditelné v TMEP.
- [ ] ESP8266 se pravidelně probouzí ze spánku.
- [ ] Veřejný repozitář neobsahuje přihlašovací údaje (viz [Bezpečnost](BEZPECNOST.md)).

Další postup při potížích: [Provoz a řešení problémů](PROVOZ_A_PROBLEMY.md).

## Odkazy

- [ESP8266 Arduino core – instalace](https://arduino-esp8266.readthedocs.io/en/latest/installing.html)
- [Adafruit – Arduino pro SHT4x](https://learn.adafruit.com/adafruit-sht40-temperature-humidity-sensor/arduino)
- [Adafruit SHT4x – repozitář knihovny](https://github.com/adafruit/Adafruit_SHT4X)
