#pragma once

// Default settings retained from the original sketch for compatibility.
// Copy this file to config.h and edit there before deploying. config.h is ignored by Git.
#define WIFI_SSID "NewSWSfree"
#define WIFI_PASSWORD ""  // Empty string = open Wi-Fi; never commit real passwords.

#define TMEP_DOMAIN "mps"             // Subdomain ONLY (without .tmep.cz).
#define TMEP_TEMPERATURE_GUID "temp"  // Temperature query parameter name.
#define TMEP_SLEEP_SECONDS 60UL      // Deep sleep AFTER a measurement attempt.
