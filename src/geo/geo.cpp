#include "geo.h"

#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
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

// Function to check if IP is in private range
bool Geo::isPrivateIP(const std::string& ip) {
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

bool Geo::isAllowed(const std::string& ip) {
    // Allow private IPs for local development
    if (isPrivateIP(ip)) {
        return true;
    }
    
    // Check if IP exists in cache
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = ip_country_cache_.find(ip);
        
        if (it != ip_country_cache_.end()) {
            // Check cache entry expiration (24 hours)
            auto now = std::chrono::system_clock::now();
            auto diff = std::chrono::duration_cast<std::chrono::hours>(now - it->second.timestamp).count();
            
            if (diff < 24) {
                // Cache hit and still valid
                return it->second.allowed;
            } else {
                // Cache expired, remove it
                ip_country_cache_.erase(it);
            }
        }
    }
    
    // Try multiple services to resolve country
    std::string country = resolveCountry(ip);
    
    // Check if country is in trusted list
    bool allowed = false;
    for (const auto& region : Config::trustedRegion) {
        if (country == region) {
            allowed = true;
            break;
        }
    }
    
    // Store result in cache
    CacheEntry entry;
    entry.country = country;
    entry.allowed = allowed;
    entry.timestamp = std::chrono::system_clock::now();
    
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        ip_country_cache_[ip] = entry;
        
        // Log cache size occasionally
        if (ip_country_cache_.size() % 100 == 0) {
            Logger::debug("Geo cache size: {}", ip_country_cache_.size());
        }
    }
    
    if (!allowed) {
        Logger::warn("IP {} from region {} not allowed (not in trusted regions)", ip, country);
    }
    
    return allowed;
}

std::string Geo::resolveCountry(const std::string& ip) {
    // Try multiple geo services in order
    std::string country;
    
    country = tryIpApiService(ip);
    if (!country.empty()) return country;
    
    country = tryFreeGeoIpService(ip);
    if (!country.empty()) return country;
    
    country = tryIpInfoService(ip);
    if (!country.empty()) return country;
    
    // If all services fail, log error and default to empty string
    Logger::error("Failed to resolve country for IP {}", ip);
    return "XX";  // Unknown country code
}

std::string Geo::performCurlRequest(const std::string& url) {
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

std::string Geo::tryIpApiService(const std::string& ip) {
    try {
        std::string url = "http://ip-api.com/json/" + ip + "?fields=countryCode";
        std::string response = performCurlRequest(url);
        
        if (response.empty()) return "";
        
        auto j = json::parse(response);
        if (j.contains("countryCode") && !j["countryCode"].is_null()) {
            std::string country = j["countryCode"].get<std::string>();
            Logger::debug("Resolved IP {} to country {} via ip-api.com", ip, country);
            return country;
        }
    } catch (const std::exception& e) {
        Logger::error("Error with ip-api.com service: {}", e.what());
    }
    
    return "";
}

std::string Geo::tryFreeGeoIpService(const std::string& ip) {
    try {
        std::string url = "https://freegeoip.app/json/" + ip;
        std::string response = performCurlRequest(url);
        
        if (response.empty()) return "";
        
        auto j = json::parse(response);
        if (j.contains("country_code") && !j["country_code"].is_null()) {
            std::string country = j["country_code"].get<std::string>();
            Logger::debug("Resolved IP {} to country {} via freegeoip.app", ip, country);
            return country;
        }
    } catch (const std::exception& e) {
        Logger::error("Error with freegeoip.app service: {}", e.what());
    }
    
    return "";
}

std::string Geo::tryIpInfoService(const std::string& ip) {
    try {
        std::string url = "https://ipinfo.io/" + ip + "/json";
        std::string response = performCurlRequest(url);
        
        if (response.empty()) return "";
        
        auto j = json::parse(response);
        if (j.contains("country") && !j["country"].is_null()) {
            std::string country = j["country"].get<std::string>();
            Logger::debug("Resolved IP {} to country {} via ipinfo.io", ip, country);
            return country;
        }
    } catch (const std::exception& e) {
        Logger::error("Error with ipinfo.io service: {}", e.what());
    }
    
    return "";
}

}  // namespace Ventura