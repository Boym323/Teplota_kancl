# Provoz, diagnostika a řešení problémů

Návod vychází ze stávajícího [`Teplota_kancl.ino`](../Teplota_kancl.ino). Předpokládá přístup k zařízení, Arduino IDE a sériovému monitoru. V repozitáři není monitoring, dashboard ani automatická diagnostika.

## Jak poznat správný provoz

Po restartu nebo probuzení ze spánku by měly následovat následující fáze:

1. Inicializace a nalezení senzoru (`Found SHT4x sensor`).
2. Výpis sériového čísla a režimu měření (`High precision`).
3. Připojování k nastavené Wi-Fi, následně `WiFi connected` a IP adresa.
4. Informace o režimu ohřevu (`No heater`).
5. Změřená teplota, relativní vlhkost a doba načítání.
6. Připojení k `<domain>.tmep.cz`, případně `Client connected`.
7. Výpis `Requesting URL: ...`.
8. Při dostupnosti dat od serveru hluboký spánek a po probuzení nový start.

**Výstup do sériového monitoru není důkazem doručení dat.** Po úspěšném odeslání firmware nevyhodnocuje HTTP status ani tělo odpovědi. Přijaté hodnoty kontrolujte přímo na straně služby TMEP.

Otevřete Arduino IDE → **Tools → Serial Monitor** a nastavte **115200 baudů**. Během hlubokého spánku není očekáván průběžný výpis; některé běžné desky také používají odlišnou rychlost pro startovací hlášení bootloaderu.

## Diagnostika podle příznaku

| Příznak nebo hláška | Pravděpodobná příčina / ověření | Doporučený postup |
| --- | --- | --- |
| `Couldn't find SHT4x` | Senzor není na I²C dostupný. | Zkontrolovat napájení, společnou GND, SDA/SCL, konektory a adresaci I²C; ověřit správnou desku. |
| Zařízení skončí po `Couldn't find SHT4x` | V programu následuje nekonečné čekání. | Opravit zapojení a zařízení restartovat; tento stav sám nepokračuje. |
| Nekonečné tečky po `Connecting to` | `WiFi.status()` nedosáhl `WL_CONNECTED`. | Ověřit SSID, otevřenou/chráněnou síť, dostupnost 2,4GHz Wi-Fi, signál a DHCP; aktuální sketch volá `WiFi.begin(ssid)` bez hesla. |
| `Connection failed` | `WiFiClient::connect()` nenavázal TCP spojení. | Ověřit DNS, doménu, internet, TCP port 80, firewall, dostupnost TMEP. |
| `>>> Client Timeout !` | Do 5 sekund po odeslání nebyla dostupná žádná data odpovědi. | Prověřit dostupnost serveru a síť; další `loop()` může běžet bez prodlevy. |
| Hodnoty se vypisují, ale na TMEP chybí | Chybná subdoména, GUID, očekávané parametry, případně chyba HTTP aplikace. | Porovnat URL s nastavením TMEP; testovat mimo zařízení, sledovat dostupnost služby. Kód HTTP status nekontroluje. |
| Objeví se pouze první měření | ESP8266 se neprobudilo z deep sleep. | Ověřit podporu a propojení GPIO16 ↔ RST podle konkrétní desky. |
| Měření se opakuje v jiném intervalu než 60 s | 60 s je jen doba spánku, nikoliv celý cyklus. | Započítat boot, hledání Wi-Fi, měření a přenos. |
| Velmi rychlé opakování měření při výpadku TMEP | Předčasné `return` z `loop()` přeskočí deep sleep. | Zajistit dostupnost serveru; robustní náprava vyžaduje změnu programu (spánek / backoff i při selhání). |
| Data jsou nepravděpodobná | Okolní teplota, umístění senzoru, napájení, rušení nebo měření u tepelného zdroje. | Porovnat s referenčním měřidlem a zajistit správné umístění; sketch nefiltruje hodnoty. |
| Nelze firmware nahrát | Chybný port, kabel, USB–UART ovladač či režim bootloaderu. | Použít datový kabel, správný profil desky a pokyny výrobce pro přechod do upload režimu. |

## Postup při nedostupnosti TMEP

1. Na sériové lince zjistěte, zda nastalo `Connection failed`, `Client Timeout`, nebo se pouze nevykreslila data v TMEP.
2. Ověřte hodnotu `domain` a sestavený `host` (`.tmep.cz` se připojuje automaticky).
3. Ověřte výpis `Requesting URL`: musí obsahovat očekávaný parametr teploty (`guid`) a `humV`.
4. Na jiném zařízení ze stejné sítě ověřte dostupnost cílového hostitele a konkrétního požadavku. Pozor: přímé otevření měřicí URL může samo odeslat testovací hodnotu.
5. Zkontrolujte dashboard / historii na TMEP. Samotné navázání spojení **neprokazuje přijetí měření**.
6. Pokud problém trvá, ukládejte pouze diagnostiku bez hesel, interních síťových detailů nebo jiných citlivých údajů.

## Proč není režim vhodný pro bezobslužnou kritickou telemetrii

V nynějším stavu existují tyto ověřené mezery:

- Bez časového limitu připojování k Wi-Fi.
- Chybí spánek po síťové chybě, takže zařízení může zvýšit spotřebu při výpadku.
- Chybí potvrzení úspěšného zápisu dat a opakované doručení.
- Chybí lokální fronta měření, časové značky a kontrola kvality dat.
- Po selhání senzoru program čeká neomezeně.
- Není definována správa vzdálených aktualizací.
- Chybí sestavovací/testovací pipeline a záznam konkrétních verzí knihoven.

Firmware je proto vhodné nejprve otestovat v cílové síti a na konkrétním hardwaru. Pro přehled plánovaných technických změn viz [Technický popis](TECHNICKY_POPIS.md).

## Záznam při řešení incidentu

Pro opakovatelné hlášení se hodí shromáždit:

- Model ESP8266 desky a SHT4x senzoru.
- Verzi nahraného sketch a použitých knihoven.
- Verzi ESP8266 Arduino core.
- Popis zapojení včetně GPIO16/RST.
- Přibližný čas problému a související chybovou hlášku na sériové lince.
- Informaci, zda se problém týká všech cyklů nebo jen části.
- Potvrzení, zda byly hodnoty viditelné přímo na TMEP.

**Nezveřejňujte skutečné Wi-Fi heslo ani jiná přístupová tajemství.**
