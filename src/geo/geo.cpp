#include "geo.h"

#include <chrono>
#include <cstring>
#include <memory>
#include <string>

#include "../config.h"
#include "../utils/logger.h"
#include "httplib.h"
#include <curl/curl.h>

// Platform-specific includes
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#endif

#include "nlohmann/json.hpp"
using json = nlohmann::json;

namespace Ventura {

// Helper function to write CURL response data to a string
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
    size_t newLength = size * nmemb;
    try {
        s->append((char*)contents, newLength);
        return newLength;
    } catch (std::bad_alloc& e) {
        // Handle memory problem
        return 0;
    }
}

Geo::Geo() {
    // Initialize curl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    Logger::info("Geo service initialized");
}

Geo::~Geo() {
    curl_global_cleanup();
}

bool Geo::initialize() {
    Logger::info("Initializing geo location service");
    // Nothing special to initialize here as the constructor already does the work
    return true;
}

void Geo::loadTrustedRegions(const std::vector<std::string>& regions) {
    trusted_regions_ = regions;
    Logger::info("Loaded trusted regions: {}", fmt::join(trusted_regions_, ", "));
}

void Geo::clearCache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    ip_cache_.clear();
    Logger::info("Geo cache cleared");
}

// Function to check if IP is in private range
bool Geo::isPrivateIP(const std::string& ip) const {
    // Convert string IP to numeric format
    struct in_addr addr;
    
#ifdef _WIN32
    // Windows implementation
    addr.s_addr = 0;
    if (inet_pton(AF_INET, ip.c_str(), &addr) != 1) {
        return false;  // Invalid IP
    }
    unsigned long ipNum = ntohl(addr.s_addr);
#else
    // Linux implementation
    if (inet_pton(AF_INET, ip.c_str(), &addr) != 1) {
        return false;  // Invalid IP
    }
    uint32_t ipNum = ntohl(addr.s_addr);
#endif

    // Check private IP ranges
    // 10.0.0.0 - 10.255.255.255
    // 172.16.0.0 - 172.31.255.255
    // 192.168.0.0 - 192.168.255.255
    // 127.0.0.0 - 127.255.255.255 (localhost)
    if ((ipNum >= 0x0A000000 && ipNum <= 0x0AFFFFFF) || // 10.0.0.0/8
        (ipNum >= 0xAC100000 && ipNum <= 0xAC1FFFFF) || // 172.16.0.0/12
        (ipNum >= 0xC0A80000 && ipNum <= 0xC0A8FFFF) || // 192.168.0.0/16
        (ipNum >= 0x7F000000 && ipNum <= 0x7FFFFFFF)) { // 127.0.0.0/8
        return true;
    }
    
    return false;
}

bool Geo::isTrustedCountry(const std::string& country_code) const {
    for (const auto& region : trusted_regions_) {
        if (country_code == region) {
            return true;
        }
    }
    return false;
}

void Geo::addToCache(const std::string& ip, const IPGeoData& data) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    ip_cache_[ip] = data;
    
    // Log cache size occasionally
    if (ip_cache_.size() % 100 == 0) {
        Logger::debug("Geo cache size: {}", ip_cache_.size());
    }
}

bool Geo::getFromCache(const std::string& ip, IPGeoData& data) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = ip_cache_.find(ip);
    
    if (it != ip_cache_.end()) {
        // Check cache entry expiration
        auto now = std::chrono::system_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.cache_time).count();
        
        if (diff < cache_ttl_seconds_) {
            // Cache hit and still valid
            data = it->second;
            return true;
        } else {
            // Cache expired, remove it
            ip_cache_.erase(it);
        }
    }
    
    return false;
}

uint32_t Geo::ipToInt(const std::string& ip) const {
    struct in_addr addr;
    if (inet_pton(AF_INET, ip.c_str(), &addr) != 1) {
        return 0;  // Invalid IP
    }
    return ntohl(addr.s_addr);
}

// Helper for making CURL requests
std::string performCurlRequest(const std::string& url) {
    std::string response_string;
    CURL* curl = curl_easy_init();
    
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);  // 5 seconds timeout
        
        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            Logger::error("CURL request failed: {}", curl_easy_strerror(res));
            return "";
        }
    }
    
    return response_string;
}

bool Geo::lookupWithPrimaryService(const std::string& ip, IPGeoData& result) {
    try {
        std::string url = "http://ip-api.com/json/" + ip + "?fields=country,countryCode,regionName,city";
        std::string response = performCurlRequest(url);
        
        if (response.empty()) return false;
        
        auto j = json::parse(response);
        if (j.contains("countryCode") && !j["countryCode"].is_null()) {
            result.country_code = j["countryCode"].get<std::string>();
            result.country_name = j.value("country", "Unknown");
            result.region = j.value("regionName", "Unknown");
            result.city = j.value("city", "Unknown");
            result.is_trusted = isTrustedCountry(result.country_code);
            result.cache_time = std::chrono::system_clock::now();
            
            Logger::debug("Resolved IP {} to country {} via primary service", ip, result.country_code);
            return true;
        }
    } catch (const std::exception& e) {
        Logger::error("Error with primary geo service: {}", e.what());
    }
    
    return false;
}

bool Geo::lookupWithSecondaryService(const std::string& ip, IPGeoData& result) {
    try {
        std::string url = "https://freegeoip.app/json/" + ip;
        std::string response = performCurlRequest(url);
        
        if (response.empty()) return false;
        
        auto j = json::parse(response);
        if (j.contains("country_code") && !j["country_code"].is_null()) {
            result.country_code = j["country_code"].get<std::string>();
            result.country_name = j.value("country_name", "Unknown");
            result.region = j.value("region_name", "Unknown");
            result.city = j.value("city", "Unknown");
            result.is_trusted = isTrustedCountry(result.country_code);
            result.cache_time = std::chrono::system_clock::now();
            
            Logger::debug("Resolved IP {} to country {} via secondary service", ip, result.country_code);
            return true;
        }
    } catch (const std::exception& e) {
        Logger::error("Error with secondary geo service: {}", e.what());
    }
    
    return false;
}

bool Geo::lookupWithLocalDatabase(const std::string& ip, IPGeoData& result) {
    // Not implemented - would use a local GeoIP database
    return false;
}

bool Geo::fallbackLookup(const std::string& ip, IPGeoData& result) {
    try {
        std::string url = "https://ipinfo.io/" + ip + "/json";
        std::string response = performCurlRequest(url);
        
        if (response.empty()) return false;
        
        auto j = json::parse(response);
        if (j.contains("country") && !j["country"].is_null()) {
            result.country_code = j["country"].get<std::string>();
            result.country_name = "Unknown"; // Not provided by ipinfo.io
            result.region = j.value("region", "Unknown");
            result.city = j.value("city", "Unknown");
            result.is_trusted = isTrustedCountry(result.country_code);
            result.cache_time = std::chrono::system_clock::now();
            
            Logger::debug("Resolved IP {} to country {} via fallback service", ip, result.country_code);
            return true;
        }
    } catch (const std::exception& e) {
        Logger::error("Error with fallback geo service: {}", e.what());
    }
    
    return false;
}

bool Geo::isAllowed(const std::string& ip) {
    // Allow private IPs for local development
    if (isPrivateIP(ip)) {
        return true;
    }
    
    // Check if IP exists in cache
    IPGeoData geoData;
    if (getFromCache(ip, geoData)) {
        return geoData.is_trusted;
    }
    
    // Try multiple services in sequence
    if (!lookupWithPrimaryService(ip, geoData) &&
        !lookupWithSecondaryService(ip, geoData) &&
        !lookupWithLocalDatabase(ip, geoData) &&
        !fallbackLookup(ip, geoData)) {
        
        // If all services fail, log error and default to denying access
        Logger::error("Failed to resolve country for IP {}", ip);
        return false;
    }
    
    // Add result to cache
    addToCache(ip, geoData);
    
    if (!geoData.is_trusted) {
        Logger::warn("IP {} from region {} not allowed (not in trusted regions)", 
                    ip, geoData.country_code);
    }
    
    return geoData.is_trusted;
}

}  // namespace Ventura