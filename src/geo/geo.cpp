#include "geo.h"
#include "../utils/logger.h"
#include "../config.h"

#include <algorithm>
#include <cctype>
#include <curl/curl.h>
#include <iostream>
#include <regex>
#include <sstream>

// For IP operations
#include <arpa/inet.h>

namespace Ventura {

// Helper function for CURL write callbacks
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

Geo::Geo() {
    // Initialize with default values
    initialize();
}

Geo::~Geo() {
    Logger::info("Geo object destroyed");
}

bool Geo::initialize() {
    // Initialize the geolocation system
    Logger::info("Initializing Geo service");
    
    // Load trusted regions from config
    loadTrustedRegions(Config::trustedRegion);
    
    // Initialize curl globally
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    return true;
}

void Geo::loadTrustedRegions(const std::vector<std::string>& regions) {
    trusted_regions_ = regions;
    
    // Convert all to uppercase for case-insensitive comparison
    for (auto& region : trusted_regions_) {
        std::transform(region.begin(), region.end(), region.begin(), 
            [](unsigned char c) { return std::toupper(c); });
    }
    
    Logger::info("Loaded {} trusted regions: {}", trusted_regions_.size(), 
        fmt::join(trusted_regions_, ", "));
}

void Geo::clearCache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    ip_cache_.clear();
    Logger::info("Geo cache cleared");
}

bool Geo::isAllowed(const std::string& ip) {
    // Handle special cases first
    if (ip.empty()) {
        Logger::warn("Empty IP address provided");
        return false;
    }
    
    // Check for localhost and private networks - typically allowed in development
    if (isPrivateIP(ip)) {
        Logger::debug("IP {} is a private/localhost address, allowing access", ip);
        return true;
    }
    
    // Try to get from cache first
    IPGeoData geo_data;
    if (getFromCache(ip, geo_data)) {
        Logger::debug("Cache hit for IP {}: {} ({}), trusted: {}", 
            ip, geo_data.country_code, geo_data.country_name, 
            geo_data.is_trusted ? "yes" : "no");
        return geo_data.is_trusted;
    }
    
    // Not in cache, perform lookup using multiple services with fallbacks
    bool lookup_success = false;
    
    // Try primary service first
    lookup_success = lookupWithPrimaryService(ip, geo_data);
    
    // Try secondary service if primary failed
    if (!lookup_success) {
        lookup_success = lookupWithSecondaryService(ip, geo_data);
    }
    
    // Try local database if online services failed
    if (!lookup_success) {
        lookup_success = lookupWithLocalDatabase(ip, geo_data);
    }
    
    // Last resort fallback
    if (!lookup_success) {
        lookup_success = fallbackLookup(ip, geo_data);
    }
    
    // If all lookups failed, default to deny for security
    if (!lookup_success) {
        Logger::warn("All geolocation lookups failed for IP {}, denying access", ip);
        return false;
    }
    
    // Check if the country is in our trusted list
    geo_data.is_trusted = isTrustedCountry(geo_data.country_code);
    
    // Add to cache
    addToCache(ip, geo_data);
    
    Logger::info("IP {} resolved to {} ({}), trusted: {}", 
        ip, geo_data.country_code, geo_data.country_name, 
        geo_data.is_trusted ? "yes" : "no");
    
    return geo_data.is_trusted;
}

bool Geo::lookupWithPrimaryService(const std::string& ip, IPGeoData& result) {
    Logger::debug("Looking up IP {} with primary service", ip);
    
    // Using ip-api.com as primary service (free for non-commercial use)
    std::string url = "http://ip-api.com/json/" + ip + "?fields=country,countryCode,regionName,city,status";
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        Logger::error("Failed to initialize CURL");
        return false;
    }
    
    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L); // 5 seconds timeout
    
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        Logger::error("CURL error: {}", curl_easy_strerror(res));
        return false;
    }
    
    try {
        // Parse JSON response
        auto json_response = nlohmann::json::parse(response);
        
        if (json_response["status"] == "success") {
            result.country_code = json_response["countryCode"];
            result.country_name = json_response["country"];
            result.region = json_response["regionName"];
            result.city = json_response["city"];
            
            // Convert country code to uppercase
            std::transform(result.country_code.begin(), result.country_code.end(), 
                result.country_code.begin(), ::toupper);
            
            return true;
        } else {
            Logger::warn("Primary service returned error for IP {}", ip);
        }
    } catch (const std::exception& e) {
        Logger::error("Error parsing response from primary service: {}", e.what());
    }
    
    return false;
}

bool Geo::lookupWithSecondaryService(const std::string& ip, IPGeoData& result) {
    Logger::debug("Looking up IP {} with secondary service", ip);
    
    // Using ipapi.co as secondary service
    std::string url = "https://ipapi.co/" + ip + "/json/";
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        Logger::error("Failed to initialize CURL for secondary service");
        return false;
    }
    
    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        Logger::error("CURL error with secondary service: {}", curl_easy_strerror(res));
        return false;
    }
    
    try {
        // Parse JSON response
        auto json_response = nlohmann::json::parse(response);
        
        if (!json_response.contains("error")) {
            result.country_code = json_response["country_code"];
            result.country_name = json_response["country_name"];
            result.region = json_response["region"];
            result.city = json_response["city"];
            
            // Convert country code to uppercase
            std::transform(result.country_code.begin(), result.country_code.end(), 
                result.country_code.begin(), ::toupper);
            
            return true;
        } else {
            Logger::warn("Secondary service returned error for IP {}", ip);
        }
    } catch (const std::exception& e) {
        Logger::error("Error parsing response from secondary service: {}", e.what());
    }
    
    return false;
}

bool Geo::lookupWithLocalDatabase(const std::string& ip, IPGeoData& result) {
    // This would use a local GeoIP database like MaxMind's GeoLite2
    // For simplicity in this example, we're implementing a very basic
    // check based on IP ranges
    
    Logger::debug("Looking up IP {} with local database", ip);
    
    // Convert IP to integer for range checking
    uint32_t ip_int = ipToInt(ip);
    
    // Define some basic IP ranges for common countries
    // This is a simplified example - in production you would use a proper GeoIP database
    struct IPRange {
        uint32_t start;
        uint32_t end;
        std::string country_code;
        std::string country_name;
    };
    
    std::vector<IPRange> basic_ranges = {
        // Some example ranges - these are NOT accurate and for illustration only
        {ipToInt("1.0.0.0"), ipToInt("1.255.255.255"), "AU", "Australia"},
        {ipToInt("27.0.0.0"), ipToInt("27.255.255.255"), "JP", "Japan"},
        {ipToInt("36.0.0.0"), ipToInt("36.255.255.255"), "CN", "China"},
        {ipToInt("39.0.0.0"), ipToInt("39.255.255.255"), "KR", "South Korea"},
        {ipToInt("42.0.0.0"), ipToInt("42.255.255.255"), "KR", "South Korea"},
        {ipToInt("43.0.0.0"), ipToInt("43.255.255.255"), "JP", "Japan"},
        {ipToInt("49.0.0.0"), ipToInt("49.255.255.255"), "TH", "Thailand"},
        {ipToInt("58.0.0.0"), ipToInt("58.255.255.255"), "JP", "Japan"},
        {ipToInt("59.0.0.0"), ipToInt("59.255.255.255"), "KR", "South Korea"},
        {ipToInt("60.0.0.0"), ipToInt("60.255.255.255"), "CN", "China"},
        {ipToInt("101.0.0.0"), ipToInt("101.255.255.255"), "TW", "Taiwan"},
        {ipToInt("103.0.0.0"), ipToInt("103.255.255.255"), "ID", "Indonesia"},
        {ipToInt("110.0.0.0"), ipToInt("110.255.255.255"), "MY", "Malaysia"},
        {ipToInt("111.0.0.0"), ipToInt("111.255.255.255"), "SG", "Singapore"},
        {ipToInt("112.0.0.0"), ipToInt("112.255.255.255"), "ID", "Indonesia"},
        {ipToInt("113.0.0.0"), ipToInt("113.255.255.255"), "SG", "Singapore"},
        {ipToInt("114.0.0.0"), ipToInt("114.255.255.255"), "ID", "Indonesia"},
        {ipToInt("115.0.0.0"), ipToInt("115.255.255.255"), "TH", "Thailand"},
        {ipToInt("116.0.0.0"), ipToInt("116.255.255.255"), "MY", "Malaysia"},
        {ipToInt("117.0.0.0"), ipToInt("117.255.255.255"), "ID", "Indonesia"},
        {ipToInt("118.0.0.0"), ipToInt("118.255.255.255"), "JP", "Japan"},
        {ipToInt("119.0.0.0"), ipToInt("119.255.255.255"), "SG", "Singapore"},
        {ipToInt("120.0.0.0"), ipToInt("120.255.255.255"), "CN", "China"},
        {ipToInt("121.0.0.0"), ipToInt("121.255.255.255"), "KR", "South Korea"},
        {ipToInt("122.0.0.0"), ipToInt("122.255.255.255"), "TW", "Taiwan"},
        {ipToInt("180.0.0.0"), ipToInt("180.255.255.255"), "TH", "Thailand"},
        {ipToInt("182.0.0.0"), ipToInt("182.255.255.255"), "MY", "Malaysia"},
        {ipToInt("183.0.0.0"), ipToInt("183.255.255.255"), "ID", "Indonesia"}
    };
    
    for (const auto& range : basic_ranges) {
        if (ip_int >= range.start && ip_int <= range.end) {
            result.country_code = range.country_code;
            result.country_name = range.country_name;
            result.region = "Unknown";
            result.city = "Unknown";
            return true;
        }
    }
    
    return false;
}

bool Geo::fallbackLookup(const std::string& ip, IPGeoData& result) {
    Logger::debug("Using fallback lookup for IP {}", ip);
    
    // As a last resort, try to guess the region from the IP structure or format
    // This is very unreliable and should only be used when all other methods fail
    
    // As a simple example, we'll check some common IP prefixes
    // This is NOT reliable and is just for demonstration purposes
    if (ip.find("103.") == 0 || ip.find("111.") == 0 || ip.find("180.") == 0) {
        result.country_code = "ID";
        result.country_name = "Indonesia";
    } 
    else if (ip.find("111.") == 0 || ip.find("113.") == 0 || ip.find("119.") == 0) {
        result.country_code = "SG";
        result.country_name = "Singapore";
    }
    else if (ip.find("110.") == 0 || ip.find("116.") == 0 || ip.find("182.") == 0) {
        result.country_code = "MY";
        result.country_name = "Malaysia";
    }
    else {
        // If we can't determine, default to a non-trusted country
        result.country_code = "XX";
        result.country_name = "Unknown";
        return false;
    }
    
    result.region = "Unknown";
    result.city = "Unknown";
    
    return true;
}

bool Geo::isPrivateIP(const std::string& ip) const {
    // Check if IP is private/local
    
    // Check special cases
    if (ip == "127.0.0.1" || ip == "::1" || ip == "localhost") {
        return true;
    }
    
    // Convert to int for range checking
    uint32_t ip_int = ipToInt(ip);
    
    // Check private IP ranges
    return
        // 10.0.0.0/8
        (ip_int >= ipToInt("10.0.0.0") && ip_int <= ipToInt("10.255.255.255")) ||
        // 172.16.0.0/12
        (ip_int >= ipToInt("172.16.0.0") && ip_int <= ipToInt("172.31.255.255")) ||
        // 192.168.0.0/16
        (ip_int >= ipToInt("192.168.0.0") && ip_int <= ipToInt("192.168.255.255")) ||
        // 169.254.0.0/16 (link-local)
        (ip_int >= ipToInt("169.254.0.0") && ip_int <= ipToInt("169.254.255.255"));
}

bool Geo::isTrustedCountry(const std::string& country_code) const {
    // Convert to uppercase for comparison
    std::string code = country_code;
    std::transform(code.begin(), code.end(), code.begin(), ::toupper);
    
    // Check if in trusted regions list
    return std::find(trusted_regions_.begin(), trusted_regions_.end(), code) != trusted_regions_.end();
}

void Geo::addToCache(const std::string& ip, const IPGeoData& data) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    // Create a copy with current timestamp
    IPGeoData cache_data = data;
    cache_data.cache_time = std::chrono::system_clock::now();
    
    // Add or update cache
    ip_cache_[ip] = cache_data;
    
    Logger::debug("Added IP {} to geolocation cache", ip);
}

bool Geo::getFromCache(const std::string& ip, IPGeoData& data) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    // Check if IP is in cache
    auto it = ip_cache_.find(ip);
    if (it == ip_cache_.end()) {
        return false;
    }
    
    // Check if cache entry is expired
    auto now = std::chrono::system_clock::now();
    auto cache_time = it->second.cache_time;
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - cache_time).count();
    
    if (elapsed > cache_ttl_seconds_) {
        // Cache entry is expired, remove it
        ip_cache_.erase(it);
        return false;
    }
    
    // Return cached data
    data = it->second;
    return true;
}

uint32_t Geo::ipToInt(const std::string& ip) const {
    struct sockaddr_in sa;
    uint32_t result = 0;
    
    // Try to convert the IP to a binary format
    if (inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr)) != 1) {
        Logger::error("Invalid IP address format: {}", ip);
        return 0;  // Return 0 for invalid IPs
    }
    
    // Get as network byte order integer
    result = ntohl(sa.sin_addr.s_addr);
    return result;
}

} // namespace Ventura