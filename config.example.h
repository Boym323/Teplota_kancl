#pragma once

// Veřejná ukázková konfigurace. Před nasazením ji zkopírujte do
// Teplota_kancl_local.h a soukromé hodnoty upravte pouze tam.
// Soubor Teplota_kancl_local.h je v .gitignore a nesmí obsahovat veřejně sdílená hesla.

#define WIFI_SSID "NewSWSfree"       // Název Wi-Fi; původní veřejná hodnota projektu.
#define WIFI_PASSWORD ""             // Prázdné heslo = otevřená síť, jinak heslo WPA/WPA2.

#define TMEP_DOMAIN "mps"            // Jen subdoména, bez .tmep.cz.
#define TMEP_TEMPERATURE_GUID "temp" // Klíč parametru pro teplotu v URL.
#define TMEP_SLEEP_SECONDS 60UL      // Doba spánku po každém pokusu (sekundy).

// Rychlé spojení: kanál a BSSID z předchozího připojení v RTC paměti.
// Při změně AP se po 2,5 s automaticky přepne na běžné vyhledávání.
// Nastavte 0, pokud vaše Wi-Fi rychlé spojení nepodporuje nebo způsobuje potíže.
#define TMEP_FAST_WIFI 1

// Bez čekání na standardní vypínání rádia (ESP.deepSleepInstant).
// Nastavte 0, pokud se na konkrétní desce objeví problém s uspáváním.
#define TMEP_FAST_SLEEP 1
