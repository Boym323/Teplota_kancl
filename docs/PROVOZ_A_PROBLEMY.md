# Provoz a řešení problémů

Průvodce pro firmware [`Teplota_kancl.ino`](../Teplota_kancl.ino), který měří SHT4x, odešle hodnoty do TMEP a **po každém pokusu vstoupí do deep sleep**. Firmware nemá OTA, vlastní dashboard ani dálkovou diagnostiku.

## Sériový monitor

Arduino IDE → **Tools → Serial Monitor**, rychlost **115200**. Úspěšný cyklus může vypadat přibližně takto:

```text
Temperature (C): 23.50
Humidity (% RH): 45.20
Wi-Fi connected; IP: 192.0.2.10
TMEP HTTP status: 200
TMEP accepted HTTP request
Deep sleep (seconds): 60
```

Jde o *ilustrační* výpis, nikoli o skutečně naměřené hodnoty. Pouze status 2xx **nedokazuje**, že jsou hodnoty uložené a zobrazené v TMEP; zkontrolujte je i ve službě.

Po dobu hlubokého spánku se nic nového nevypisuje. Pro pravidelné probouzení bývá u ESP8266 potřeba propojení **GPIO16 → RST**. Každé probuzení vyvolá reset, takže počáteční fáze sériového výpisu začne znovu.

## Problémy a postupy

| Hláška / příznak | Význam | Co ověřit |
| --- | --- | --- |
| `SHT4x not found` | Senzor nedostupný na I²C. | Napájení a GND, SDA/SCL, správné piny desky, funkčnost senzoru. |
| `SHT4x read failed` | Senzor nevrátil platný výsledek API. | Napájení, kontakt vodičů a elektromagnetické rušení. |
| `Invalid SHT4x values` | Načtené číslo není konečné nebo mimo fyzikální rozsah. | Čidlo a jeho umístění; porovnat s referenčním měřením. |
| `Wi-Fi connection timed out` | Do 15 sekund nenastal stav `WL_CONNECTED`. | SSID, heslo v `Teplota_kancl_local.h`, signál 2,4 GHz, DHCP, dostupnost AP. |
| `TMEP host name too long` | Název subdomény přesahuje povolený buffer. | Zkrátit `TMEP_DOMAIN`; nepsat celou doménu `.tmep.cz`. |
| `TMEP request too long` | URL/hlavičky překročily buffer požadavku. | Zkontrolovat neobvykle dlouhý `TMEP_TEMPERATURE_GUID`. |
| `TMEP connection failed` | DNS nebo TCP připojení na port 80 se nezdařilo. | DNS, internet, firewall, dostupnost cílové domény. |
| `Incomplete TMEP request` | TCP klient nepotvrdil odeslání celé zprávy. | Nestabilní Wi-Fi, vzdálený server, síťové chyby. |
| `TMEP reply missing or invalid` | Odpověď nemá úplný či rozpoznatelný HTTP/1.x stavový řádek. | Dostupnost serveru, proxy a smysluplnost odpovědi. |
| `TMEP status line too long` | Odpověď překročila malý buffer na HTTP status. | Nečekaná odpověď serveru / proxy. |
| `TMEP HTTP status malformed` | Odpověď nemá standardní třímístný stavový kód. | Server nebo síťový prostředník. |
| `TMEP HTTP status: 3xx/4xx/5xx` | Server vrátil přesměrování nebo chybu. | Parametry domény, konfiguraci TMEP, změny endpointu. |
| `Deep sleep (seconds): 60` a žádné další měření | Zařízení možná neusíná/probouzí se nesprávně. | GPIO16 ↔ RST, schéma desky, napájení. |

### Konfigurace

Aktuální firmware podporuje lokální `Teplota_kancl_local.h` a zveřejněný `config.example.h`. Pokud `Teplota_kancl_local.h` neexistuje, použijí se výchozí hodnoty z příkladu. Zkontrolujte, že máte vlastní nastavení a skutečně se kompiluje do výsledného obrazu. `Teplota_kancl_local.h` není trackován Gitem.

### Měření na TMEP chybí

1. Ověřte sériový výpis: došlo k připojení na Wi-Fi a HTTP status byl 2xx?
2. Ověřte nastavení `TMEP_DOMAIN` a `TMEP_TEMPERATURE_GUID` proti TMEP. Formát cesty je `/?<guid>=<teplota>&humV=<vlhkost>`.
3. Ověřte DNS / dostupnost cílové služby a zda nevznikla změna endpointu.
4. Zkontrolujte zobrazení na straně služby. Testovací otevření URL může **zapsat testovací měření**.
5. Pokud je požadavek přijatý, ale hodnota nezobrazená, diagnostika musí pokračovat na straně služby: status 2xx není potvrzení historie.

## Záměrné změny chování proti původní verzi

- Senzor se načítá **před připojením k Wi-Fi**.
- V případě chyby senzoru / Wi-Fi / HTTP se další měření neprovádí okamžitě, ale až **po spánku**.
- Wi-Fi má časový limit připojení 15 sekund.
- Firmware rozlišuje úspěšné HTTP odpovědi 2xx od 3xx–5xx.
- Hodnoty se kontrolují a požadavek je sestaven bez dočasných objektů Arduino `String`.
- Doba spánku je stále výchozích **60 sekund** a začíná po skončení pokusu, nikoli v přesné minutě.

## Limity a provozní doporučení

Není implementováno ukládání nedoručených hodnot, opakované odesílání, aplikací potvrzené zapsání na TMEP, HTTPS ani OTA. Pro bezpečný dlouhodobý provoz pravidelně kontrolujte skutečná data v TMEP, dostupnost senzoru a stav napájení. Není zde definována metodika ani naměřená hodnota spotřeby.

Pro hlášení závad přiložte model desky a senzoru, použitou verzi knihoven, zapojení GPIO16/RST a chybovou hlášku ze sériového monitoru. **Nikdy nepřikládejte hesla ani kompletní soukromou konfiguraci.**

Podrobnější popis programu je v [Technickém popisu](TECHNICKY_POPIS.md).
