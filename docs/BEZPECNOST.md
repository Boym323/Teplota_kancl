# Bezpečnost a známá omezení

Tento dokument uvádí **ověřené vlastnosti současného sketch** [`Teplota_kancl.ino`](../Teplota_kancl.ino) a **doporučená opatření**. Nejde o bezpečnostní audit hardwaru, Wi-Fi infrastruktury ani služby TMEP.

## 1. Potenciálně citlivé údaje v historii projektu

V současném souboru sketch se nachází **zakomentovaný původní název Wi-Fi sítě a řetězec označený jako heslo**. Zakomentování samo o sobě přístupový údaj nezneplatňuje ani neskrývá před návštěvníky veřejného GitHub repozitáře.

**Doporučení s nejvyšší prioritou:**

1. Pokud mohl být uvedený údaj někdy platný, **považujte jej za zveřejněný a změňte heslo v příslušné síti**.
2. Odstraňte zakomentované údaje z aktuálního zdrojového souboru samostatnou, cílenou změnou.
3. Posuďte, zda je nutné citlivý obsah odstranit i z git historie. Historické přepisy vyžadují koordinaci s uživateli klonů/forků a nenahrazují rotaci hesla.
4. Do dalších veřejných revizí nikdy neukládejte skutečná hesla, tokeny ani tajné identifikátory. Používejte lokálně generovanou neveřejnou konfiguraci nebo bezpečnější provisioning.

**Tento dokument tyto údaje neopakuje a nemění firmware ani historii repozitáře.**

## 2. Komunikace s Wi-Fi

Aktuální program používá `WiFi.begin(ssid)` **bez předaného hesla**. V praxi je proto zamýšlen pro síť, k níž je takto možné přistoupit; pro WPA/WPA2 síť je zapotřebí úprava programu s bezpečnou konfigurací připojení.

Rizika a doporučení:

- Otevřená Wi-Fi neposkytuje šifrování na rádiové vrstvě.
- Pokud zařízení nepotřebuje přístup do interních částí LAN, zvažte izolovanou síť pro IoT a omezená odchozí pravidla.
- Při nasazování zkontrolujte, že Wi-Fi je na frekvenci podporované zařízením (u ESP8266 běžně **2,4 GHz**) a že DNS i port 80 fungují.
- Změny síťového nastavení v dnešní podobě vyžadují novou kompilaci a nahrání firmware.

## 3. Přenos na TMEP není šifrovaný

Sketch používá `WiFiClient`, `client.connect(host, 80)` a textový `HTTP/1.1` požadavek. **Nepoužívá TLS (HTTPS)**.

Důsledky:

- Měření, cílové jméno hostitele a parametry URL mohou být na síťové trase čitelné.
- Přenos neposkytuje ochranu integrity na úrovni TLS ani ověření certifikátu serveru.
- URL může v některých síťových systémech skončit v protokolech; **nedoplňujte do ní hesla ani tajné tokeny**.

Možná modernizace je přechod na `WiFiClientSecure` / HTTPS **až po ověření podpory cílové služby**. Nutno doplnit ověřování důvěryhodnosti certifikátu; pouhé použití TLS bez ověření certifikátu není dostatečné.

## 4. Omezené vyhodnocení odpovědi serveru

Kód čeká nejvýše **5 sekund** na to, až `client.available()` vrátí nenulovou hodnotu. Nepřečte ani nezkontroluje status line (např. `200 OK`), hlavičky nebo obsah odpovědi. Až poté vstoupí do deep sleep.

Možný důsledek: i chybová odpověď může vést k uspání, přestože TMEP data odmítl. Vyhodnocení doručení je tedy pouze nepřímé a musí se ověřovat na straně TMEP.

Doporučení: implementovat časově omezené čtení odpovědi, validaci stavového kódu, omezené retry a bezpečné chování při výpadku.

## 5. Dostupnost, robustnost a spotřeba

Ověřené provozní limity:

- `sht4.begin()` při neúspěchu blokuje běh neomezeně dlouho.
- Připojování k Wi-Fi probíhá bez časového limitu.
- Síťová chyba nebo timeout přeruší `loop()` **před** plánovaným spánkem.
- Není watchdog na úrovni aplikace, fronta měření ani backoff.
- Konfigurace je zapsaná přímo v kódu.
- Sketch používá `char host[50]` s neomezeným spojováním řetězců pomocí `strcpy` a `strcat`; při dlouhé `domain` je tu riziko přepsání paměti. Při změně konfigurace omezte délku a ideálně používejte bezpečné formátování s kontrolou výsledku.

**Doporučené pořadí úprav:** rotace/odstranění zveřejněných údajů → bezpečný síťový přenos → časové limity a spánek při výpadku → kontrola HTTP odpovědi → validace konfigurace a sestavení.

## 6. Rozsah současné dokumentace

Dokumentace neznamená, že řešení prošlo funkčním, bezpečnostním nebo dlouhodobým testem. Repozitář nedokládá konkrétní model desky, způsob napájení, naměřenou spotřebu ani SLA služby TMEP. Všechny změny firmwaru je vhodné ověřovat s fyzickým zařízením.

Pro praktické zapojení viz [Instalace](INSTALACE.md), pro přesný tok programu [Technický popis](TECHNICKY_POPIS.md), při problému [Provoz a řešení problémů](PROVOZ_A_PROBLEMY.md).
