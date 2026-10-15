// Test struktury RTC cache bez ESP8266 nebo fyzické Wi-Fi.
#include "../wifi_rtc_cache.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

int main() {
  const uint8_t bssid[] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
  WifiRtcCache cache = wifiCacheBuild("moje-wifi", 6, bssid);

  assert(wifiCacheValid(cache, "moje-wifi"));
  assert(!wifiCacheValid(cache, "jina-wifi"));

  // Změna kanálu, BSSID i kontrolního součtu se musí odhalit.
  cache.channel = 14;
  assert(!wifiCacheValid(cache, "moje-wifi"));
  cache = wifiCacheBuild("moje-wifi", 6, bssid);
  cache.bssid[5] ^= 1;
  assert(!wifiCacheValid(cache, "moje-wifi"));
  cache = wifiCacheBuild("moje-wifi", 6, bssid);
  cache.checksum ^= 1;
  assert(!wifiCacheValid(cache, "moje-wifi"));

  // Data jiné verze a multicast MAC se nepoužijí.
  cache = wifiCacheBuild("moje-wifi", 6, bssid);
  cache.magic = 0;
  assert(!wifiCacheValid(cache, "moje-wifi"));
  const uint8_t invalidBssid[] = {0xFF, 0, 0, 0, 0, 0};
  cache = wifiCacheBuild("moje-wifi", 1, invalidBssid);
  assert(!wifiCacheValid(cache, "moje-wifi"));

  // Test nezávisí na velikosti pointeru hostitelského systému.
  assert(sizeof(WifiRtcCache) == 20);
  return 0;
}
