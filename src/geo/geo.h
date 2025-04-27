#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <mutex>
#include <memory>

namespace Ventura {
class Geo {
public:
    Geo();
    ~Geo();

    // Main API: Check if an IP is allowed based on country
    bool isAllowed(const std::string& ip);

    // Initialize the geolocation system
    bool initialize();

    // Load trusted regions from config
    void loadTrustedRegions(const std::vector<std::string>& regions);

    // Clear the cache
    void clearCache();
    
private:
    // IP geocoding result
    struct IPGeoData {
        std::string country_code;
        std::string country_name;
        std::string city;
        std::string region;
        bool is_trusted;
        std::chrono::system_clock::time_point cache_time;
    };
    
    // Lookup services enumeration
    enum class LookupService {
        PRIMARY,
        SECONDARY,
        LOCAL_DB,
        FALLBACK
    };
    
    // Cache entry with TTL
    std::unordered_map<std::string, IPGeoData> ip_cache_;
    
    // Config settings
    std::vector<std::string> trusted_regions_;
    
    // Cache settings
    unsigned int cache_ttl_seconds_ = 3600; // 1 hour by default
    
    // Thread safety
    mutable std::mutex cache_mutex_;
    
    // Lookup methods
    bool lookupWithPrimaryService(const std::string& ip, IPGeoData& result);
    bool lookupWithSecondaryService(const std::string& ip, IPGeoData& result);
    bool lookupWithLocalDatabase(const std::string& ip, IPGeoData& result);
    bool fallbackLookup(const std::string& ip, IPGeoData& result);
    
    // Helper methods
    bool isPrivateIP(const std::string& ip) const;
    bool isTrustedCountry(const std::string& country_code) const;
    void addToCache(const std::string& ip, const IPGeoData& data);
    bool getFromCache(const std::string& ip, IPGeoData& data);
    
    // Convert IP string to integer for faster comparisons
    uint32_t ipToInt(const std::string& ip) const;

public:
    static Geo& Get() {
        static Geo ret;
        return ret;
    }
};
}