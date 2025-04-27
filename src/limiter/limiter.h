#pragma once

#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <unordered_map>

#include "../database/database.h"
#include "httplib.h"

namespace Ventura {
class Limiter {
public:
    // Statistics structure for better logging and monitoring
    struct RateLimitStats {
        std::atomic<uint64_t> total_requests{0};          // Total requests processed
        std::atomic<uint64_t> allowed_requests{0};        // Requests allowed through
        std::atomic<uint64_t> blocked_requests{0};        // Requests blocked due to rate limiting
        std::atomic<uint64_t> currently_limited_ips{0};   // Number of IPs currently rate limited
    };

    struct IPLimitData {
        int request_count;             // Current count of requests
        int max_requests;              // Maximum allowed requests
        int64_t last_request_time;     // Timestamp of last request
        int64_t window_start_time;     // Start of the current counting window
        int64_t cooldown_end_time;     // When the rate limit expires
        bool is_limited;               // Whether the IP is currently limited
    };

public:
    Limiter();
    ~Limiter();

    // Rate limit check function - returns true if request is rate limited
    bool ListenRequest(const httplib::Request& req, httplib::Response& res);
    
    // Load previously rate-limited IPs from database
    bool LoadLimiterData();
    
    // Save current rate limits to database for persistence
    bool SaveLimiterData();
    
    // Clean up expired rate limits to free memory
    void CleanupExpiredLimits();
    
    // Get current statistics
    const RateLimitStats& GetStats() const { return stats_; }
    
    // Stop the background processing thread
    void Stop();
    
    // Clear all rate limits (for admin purposes)
    void ClearAllLimits();

public:
    static Limiter& Get() {
        static Limiter ret;
        return ret;
    }

private:
    // Helper method to update database
    void UpdateRateLimitInDatabase(const std::string& ip, int64_t time_added, int64_t cooldown_end);
    
    // Helper method to remove from database
    void RemoveRateLimitFromDatabase(const std::string& ip);
    
    // Execute periodic maintenance tasks
    void MaintenanceThread();

private:
    // Protect concurrent access to the maps
    std::mutex limit_mutex_;
    
    // Map of IP addresses to their request data
    std::unordered_map<std::string, IPLimitData> ip_limit_data_;
    
    // Background thread for cleanup operations
    std::unique_ptr<std::thread> m_thread{};
    
    // Flag to signal thread termination
    std::atomic<bool> should_terminate_{false};
    
    // Statistics tracking
    RateLimitStats stats_;
    
    // Time window for rate limiting (in seconds)
    int64_t window_size_seconds_{60}; // Default 1 minute, will be set from config
};
}  // namespace Ventura