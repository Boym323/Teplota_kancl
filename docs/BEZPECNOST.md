# Bezpečnost a provozní omezení

Upozornění vychází z firmware [`Teplota_kancl.ino`](../Teplota_kancl.ino). Nejde o penetrační test, certifikaci hardwaru nebo audit služby TMEP.

## Historicky zveřejněné heslo

Původní zdrojový soubor obsahoval **zakomentovaný řetězec označený jako Wi-Fi heslo**. V optimalizované verzi už v `Teplota_kancl.ino` není a nové lokální údaje mají být v souboru **`Teplota_kancl_local.h`**, který je ignorován Gitem.

**DŮLEŽITÉ:** Git historie může dřívější heslo stále obsahovat a pouhé smazání z nové verze **neznamená jeho zneplatnění ani vymazání historie**. Pokud heslo někdy platilo nebo stále platí, je potřeba ho **okamžitě změnit**. Případné odstranění historického obsahu vyžaduje zvláštní plán přepisu historie a koordinaci s uživateli forků/klonů.

Soubor `Teplota_kancl_local.h` do veřejného repozitáře neposílejte, a to ani při ručním nuceném `git add -f`. Před zveřejněním změn vždy ověřte jejich obsah.

## Wi-Fi

- Připojení nyní podporuje síť s heslem pomocí `WIFI_PASSWORD`. Prázdná hodnota znamená pokus o připojení k otevřené Wi-Fi.
- ESP8266 běžně používá **2,4GHz Wi-Fi**.
- `WiFi.persistent(false)` omezuje opakované zápisy síťové konfigurace do flash.
- Pro zařízení bez přístupu do interní sítě zvažte izolovanou IoT Wi-Fi a odchozí pravidla pouze k potřebným cílům.
- Připojení k síti čeká nejvýše **15 sekund**, pak zařízení přejde do spánku.

## HTTP bez TLS

Současný firmware odesílá naměřené hodnoty na `http://<domain>.tmep.cz` přes TCP **port 80**, nepoužívá HTTPS.

Důsledky:

- Cílový server a hodnoty v URL mohou být na trase čitelné nebo pozměnitelné.
- Není zaručena autentičnost protistrany na úrovni TLS.
- V URL nesmějí být hesla, tajné tokeny ani osobní údaje.

Přechod na `WiFiClientSecure` s ověřováním certifikátu je vhodná budoucí úprava **až po ověření, že cílový endpoint TMEP HTTPS podporuje**. TLS bez správného ověřování protistrany není dostačující.

## Ověřování doručení

Optimalizace nově parsuje první řádek HTTP odpovědi a rozlišuje **2xx** proti ostatním stavům. To je důležitý krok vpřed, ale stále nejde o transakční potvrzení zápisu.

- Tělo odpovědi se neparsuje.
- Neexistuje offline fronta ani mechanismus opakovaného odeslání neúspěšného měření.
- Rozhodnutí o úspěchu vzniká pouze ze stavového kódu HTTP, což nemusí odpovídat zobrazené historii v TMEP.

## Dostupnost a paměť

- Čidlo musí uspět při inicializaci a čtení; neplatné nebo nefyzikální hodnoty se neodesílají.
- Síťové chyby a HTTP timeouty vždy končí pokusem o hluboký spánek místo okamžité smyčky.
- `snprintf` s kontrolou délky nahrazuje nebezpečné `strcpy` / `strcat` při stavbě hostitele.
- Přenos vytváří požadavek do pevných lokálních bufferů místo opakovaného skládání objektů `String`.
- Současná verze nemá jeden společný deadline pokrývající všechny síťové operace včetně DNS a TCP, neobsahuje OTA, NTP ani lokální historii.
- Výrazně velké hodnoty `TMEP_SLEEP_SECONDS` nebyly hardwarově otestovány; respektujte omezení konkrétního ESP8266 / RTC spánku.

## Testování před provozem

GitHub CI ověřuje **kompilaci** pro NodeMCU ESP8266, nikoliv provozní vlastnosti. Před nasazením na fyzické zařízení je třeba ověřit:

- Komunikaci I²C a správnost hodnot.
- Probouzení z deep sleep (GPIO16–RST).
- Ošetření výpadku Wi-Fi i TMEP.
- Správné zobrazení a ukládání hodnot v TMEP.
- Reálnou spotřebu s konkrétním napájením a deskou.

Odkazy: [Instalace](INSTALACE.md), [Technický popis](TECHNICKY_POPIS.md), [Provoz a řešení problémů](PROVOZ_A_PROBLEMY.md).
