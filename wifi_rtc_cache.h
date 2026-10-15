#pragma once

// Pomocné funkce pro rychlé opětovné připojení Wi-Fi po hlubokém spánku.
// Hlavička nezávisí na Arduino API, takže ji lze testovat i běžným C++ překladačem.
// V RTC RAM se ukládá pouze BSSID, kanál a otisk SSID, NIKDY heslo.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct alignas(4) WifiRtcCache {
  uint32_t magic;
  uint32_t ssidFingerprint;
  uint8_t channel;
  uint8_t bssid[6];
  uint8_t reserved;
  uint32_t checksum;
};

static_assert(sizeof(WifiRtcCache) == 20, "RTC cache must be 20 bytes");
static_assert(alignof(WifiRtcCache) == 4, "RTC cache needs 4-byte alignment");

constexpr uint32_t kWifiCacheMagic = 0x544B3031UL; // "TK01"

// FNV-1a: krátký kontrolní otisk, nikoli kryptografické zabezpečení.
inline uint32_t wifiFingerprint(const uint8_t* data, size_t length) {
  uint32_t hash = 2166136261UL;
  for (size_t i = 0; i < length; ++i) {
    hash = (hash ^ data[i]) * 16777619UL;
  }
  return hash;
}

inline uint32_t wifiCacheChecksum(const WifiRtcCache& cache) {
  return wifiFingerprint(reinterpret_cast<const uint8_t*>(&cache),
                         offsetof(WifiRtcCache, checksum));
}

// Změna SSID nebo poškozená RTC paměť zneplatní starý záznam.
inline bool wifiCacheValid(const WifiRtcCache& cache, const char* ssid) {
  return cache.magic == kWifiCacheMagic &&
         cache.ssidFingerprint ==
             wifiFingerprint(reinterpret_cast<const uint8_t*>(ssid), strlen(ssid)) &&
         cache.channel >= 1 && cache.channel <= 13 &&
         (cache.bssid[0] & 1U) == 0 && // BSSID musí být unicast adresa.
         cache.checksum == wifiCacheChecksum(cache);
}

inline WifiRtcCache wifiCacheBuild(const char* ssid, uint8_t channel,
                                 const uint8_t* bssid) {
  WifiRtcCache cache = {};
  cache.magic = kWifiCacheMagic;
  cache.ssidFingerprint =
      wifiFingerprint(reinterpret_cast<const uint8_t*>(ssid), strlen(ssid));
  cache.channel = channel;
  memcpy(cache.bssid, bssid, sizeof(cache.bssid));
  cache.checksum = wifiCacheChecksum(cache);
  return cache;
}
