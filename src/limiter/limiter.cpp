#include "limiter.h"

#include <algorithm>
#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include <unordered_map>

#include "../config.h"
#include "../database/database.h"
#include "httplib.h"
#include "../utils/logger.h"
#include "sqlite3.h"

namespace Ventura {

Limiter::Limiter() {
    // Initialize with config values
    window_size_seconds_ = Config::rateLimitTime > 0 ? Config::rateLimitTime : 300;  // Default 5 minutes
    
    // Start background maintenance thread
    should_terminate_ = false;
    m_thread = std::make_unique<std::thread>(&Limiter::MaintenanceThread, this);
    
    Logger::info("Rate limiter initialized with window size of {} seconds", window_size_seconds_);
}

Limiter::~Limiter() {
    this->Stop();
}

void Limiter::Stop() {
    should_terminate_ = true;
    if (m_thread && m_thread->joinable()) {
        m_thread->join();
    }
    
    // Save current rate limit data to DB before exit
    SaveLimiterData();
    
    Logger::info("Rate limiter stopped. Stats: {} allowed, {} blocked requests", 
        stats_.allowed_requests.load(), stats_.blocked_requests.load());
}

bool Limiter::ListenRequest(const httplib::Request& req, httplib::Response& res) {
    const std::string& ip = req.remote_addr;
    const int64_t time_now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Increment total request count
    stats_.total_requests++;
    
    // Lock while modifying data
    {
        std::lock_guard<std::mutex> lock(limit_mutex_);
        
        auto& data = ip_limit_data_[ip];  // Creates entry if it doesn't exist
        
        // Check if IP is currently limited
        if (data.is_limited) {
            // Check if limit has expired
            if (time_now >= data.cooldown_end_time) {
                // Limit has expired, reset
                data.is_limited = false;
                data.request_count = 1;
                data.window_start_time = time_now;
                data.last_request_time = time_now;
                data.max_requests = Config::rateLimit;
                
                // Update stats
                stats_.currently_limited_ips--;
                stats_.allowed_requests++;
                
                // Remove from database
                RemoveRateLimitFromDatabase(ip);
                
                Logger::debug("IP {} rate limit expired, allowing request", ip);
                return false; // Not rate limited anymore
            }
            
            // Still limited
            stats_.blocked_requests++;
            
            // Provide Retry-After header for better client experience
            res.set_header("Retry-After", std::to_string(data.cooldown_end_time - time_now));
            res.status = 429;
            
            // Return a more informative message with time remaining
            int mins = static_cast<int>((data.cooldown_end_time - time_now) / 60);
            int secs = static_cast<int>((data.cooldown_end_time - time_now) % 60);
            std::string time_left = fmt::format("{}m {}s", mins, secs);
            
            res.set_content(fmt::format(
                "{{\"error\": \"Rate limited\", \"retry_after\": {}, \"time_left\": \"{}\"}}",
                data.cooldown_end_time - time_now, time_left), "application/json");
            
            Logger::debug("IP {} is rate limited for another {} seconds", ip, data.cooldown_end_time - time_now);
            return true; // Rate limited
        }
        
        // Check for window reset (sliding window algorithm)
        if (time_now - data.window_start_time >= window_size_seconds_) {
            // Window has expired, reset counters
            data.request_count = 1;
            data.window_start_time = time_now;
        } else {
            // Increment request count in current window
            data.request_count++;
        }
        
        data.last_request_time = time_now;
        
        // Initialize max_requests if not set
        if (data.max_requests == 0) {
            data.max_requests = Config::rateLimit;
        }
        
        // Check if limit exceeded
        if (data.request_count > data.max_requests) {
            // Rate limit exceeded, apply cooldown
            data.is_limited = true;
            data.cooldown_end_time = time_now + Config::rateLimitTime;
            
            // Update stats
            stats_.currently_limited_ips++;
            stats_.blocked_requests++;
            
            // Save to database
            UpdateRateLimitInDatabase(ip, time_now, data.cooldown_end_time);
            
            // Set response
            res.set_header("Retry-After", std::to_string(Config::rateLimitTime));
            res.status = 429;
            res.set_content(fmt::format(
                "{{\"error\": \"Rate limited\", \"retry_after\": {}, \"time_left\": \"{}m {}s\"}}",
                Config::rateLimitTime, Config::rateLimitTime / 60, Config::rateLimitTime % 60), 
                "application/json");
            
            Logger::warn("Rate limit applied to IP {} (exceeded {} requests in {} seconds), cooldown: {} seconds", 
                ip, data.request_count, time_now - data.window_start_time, Config::rateLimitTime);
            
            return true; // Rate limited
        }
    }
    
    // Request is allowed
    stats_.allowed_requests++;
    
    // Only log detailed rate information for IPs approaching their limit
    auto& data = ip_limit_data_[ip];
    if (data.request_count > (data.max_requests * 0.7)) {  // Log when over 70% of limit
        Logger::debug("IP {} at {}/{} requests ({:.1f}%)", 
            ip, data.request_count, data.max_requests, 
            (static_cast<float>(data.request_count) / data.max_requests) * 100.0f);
    }
    
    return false; // Not rate limited
}

bool Limiter::LoadLimiterData() {
    Database& m_db = Database::Get();
    std::string sql = "SELECT * FROM rate_limiter;";
    sqlite3_stmt* stmt;
    
    int rc = sqlite3_prepare_v2(m_db.get(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        Logger::error("Failed to prepare statement: {}", sqlite3_errmsg(m_db.get()));
        return false;
    }
    
    int count = 0;
    int64_t time_now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const unsigned char* ips = sqlite3_column_text(stmt, 0);
        int64_t time_added = sqlite3_column_int64(stmt, 1);
        int64_t cooldown_end = sqlite3_column_int64(stmt, 2);
        
        // Skip expired entries
        if (cooldown_end <= time_now) {
            continue;
        }
        
        std::string str_ip(reinterpret_cast<const char*>(ips));
        
        // Create entry with rate limit info
        IPLimitData data;
        data.request_count = Config::rateLimit;
        data.max_requests = Config::rateLimit;
        data.last_request_time = time_added;
        data.window_start_time = time_added;
        data.cooldown_end_time = cooldown_end;
        data.is_limited = true;
        
        // Add to our map
        ip_limit_data_[str_ip] = data;
        count++;
    }
    
    sqlite3_finalize(stmt);
    
    stats_.currently_limited_ips = count;
    Logger::info("Loaded {} active rate limits from database", count);
    
    return true;
}

bool Limiter::SaveLimiterData() {
    Database& m_db = Database::Get();
    int64_t time_now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    std::lock_guard<std::mutex> lock(limit_mutex_);
    
    // First delete all existing records to avoid duplicates
    std::string delete_sql = "DELETE FROM rate_limiter;";
    char* err_msg = nullptr;
    int rc = sqlite3_exec(m_db.get(), delete_sql.c_str(), nullptr, nullptr, &err_msg);
    
    if (rc != SQLITE_OK) {
        Logger::error("SQL error during delete: {}", err_msg);
        sqlite3_free(err_msg);
        return false;
    }
    
    // Begin transaction for faster inserts
    rc = sqlite3_exec(m_db.get(), "BEGIN TRANSACTION", nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        Logger::error("SQL error starting transaction: {}", err_msg);
        sqlite3_free(err_msg);
        return false;
    }
    
    int saved_count = 0;
    
    // Prepare the insert statement outside the loop for efficiency
    sqlite3_stmt* stmt;
    rc = sqlite3_prepare_v2(m_db.get(), "INSERT INTO rate_limiter VALUES (?, ?, ?)", -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        Logger::error("Failed to prepare statement: {}", sqlite3_errmsg(m_db.get()));
        sqlite3_exec(m_db.get(), "ROLLBACK", nullptr, nullptr, nullptr);
        return false;
    }
    
    // Insert all active rate limits
    for (const auto& pair : ip_limit_data_) {
        const auto& ip = pair.first;
        const auto& data = pair.second;
        
        // Only save active limits that haven't expired
        if (data.is_limited && data.cooldown_end_time > time_now) {
            sqlite3_bind_text(stmt, 1, ip.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int64(stmt, 2, data.last_request_time);
            sqlite3_bind_int64(stmt, 3, data.cooldown_end_time);
            
            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) {
                Logger::error("Failed to insert rate limit for IP {}: {}", ip, sqlite3_errmsg(m_db.get()));
            } else {
                saved_count++;
            }
            
            sqlite3_reset(stmt);
        }
    }
    
    sqlite3_finalize(stmt);
    
    // Commit the transaction
    rc = sqlite3_exec(m_db.get(), "COMMIT", nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        Logger::error("SQL error committing transaction: {}", err_msg);
        sqlite3_free(err_msg);
        sqlite3_exec(m_db.get(), "ROLLBACK", nullptr, nullptr, nullptr);
        return false;
    }
    
    Logger::info("Saved {} active rate limits to database", saved_count);
    return true;
}

void Limiter::CleanupExpiredLimits() {
    int64_t time_now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    int removed = 0;
    int inactive = 0;
    
    std::lock_guard<std::mutex> lock(limit_mutex_);
    
    // Use erase-remove idiom with a lambda
    auto it = ip_limit_data_.begin();
    while (it != ip_limit_data_.end()) {
        const auto& data = it->second;
        
        // Remove expired limits
        if (data.is_limited && data.cooldown_end_time <= time_now) {
            it = ip_limit_data_.erase(it);
            removed++;
            stats_.currently_limited_ips--;
        }
        // Remove inactive IPs (no activity for 2x window size)
        else if (time_now - data.last_request_time > (window_size_seconds_ * 2) && !data.is_limited) {
            it = ip_limit_data_.erase(it);
            inactive++;
        }
        else {
            ++it;
        }
    }
    
    if (removed > 0 || inactive > 0) {
        Logger::debug("Rate limit cleanup: removed {} expired limits, {} inactive IPs", 
            removed, inactive);
    }
}

void Limiter::UpdateRateLimitInDatabase(const std::string& ip, int64_t time_added, int64_t cooldown_end) {
    Database& m_db = Database::Get();
    
    // First delete any existing record for this IP
    std::string delete_sql = "DELETE FROM rate_limiter WHERE ip = ?;";
    sqlite3_stmt* delete_stmt;
    int rc = sqlite3_prepare_v2(m_db.get(), delete_sql.c_str(), -1, &delete_stmt, nullptr);
    
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(delete_stmt, 1, ip.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(delete_stmt);
        sqlite3_finalize(delete_stmt);
    }
    
    // Insert the new record
    std::string insert_sql = "INSERT INTO rate_limiter VALUES (?, ?, ?);";
    sqlite3_stmt* insert_stmt;
    rc = sqlite3_prepare_v2(m_db.get(), insert_sql.c_str(), -1, &insert_stmt, nullptr);
    
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(insert_stmt, 1, ip.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(insert_stmt, 2, time_added);
        sqlite3_bind_int64(insert_stmt, 3, cooldown_end);
        sqlite3_step(insert_stmt);
        sqlite3_finalize(insert_stmt);
    }
}

void Limiter::RemoveRateLimitFromDatabase(const std::string& ip) {
    Database& m_db = Database::Get();
    
    std::string delete_sql = "DELETE FROM rate_limiter WHERE ip = ?;";
    sqlite3_stmt* delete_stmt;
    int rc = sqlite3_prepare_v2(m_db.get(), delete_sql.c_str(), -1, &delete_stmt, nullptr);
    
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(delete_stmt, 1, ip.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(delete_stmt);
        sqlite3_finalize(delete_stmt);
    }
}

void Limiter::ClearAllLimits() {
    std::lock_guard<std::mutex> lock(limit_mutex_);
    
    // Clear in-memory limits
    ip_limit_data_.clear();
    
    // Update stats
    stats_.currently_limited_ips = 0;
    
    // Clear database
    Database& m_db = Database::Get();
    std::string delete_sql = "DELETE FROM rate_limiter;";
    char* err_msg = nullptr;
    int rc = sqlite3_exec(m_db.get(), delete_sql.c_str(), nullptr, nullptr, &err_msg);
    
    if (rc != SQLITE_OK) {
        Logger::error("SQL error during clear: {}", err_msg);
        sqlite3_free(err_msg);
    }
    
    Logger::info("All rate limits cleared");
}

void Limiter::MaintenanceThread() {
    Logger::info("Rate limiter maintenance thread started");
    
    // Run every 30 seconds
    const int cleanup_interval_seconds = 30;
    
    while (!should_terminate_) {
        // Sleep first to avoid immediate cleanup on startup
        for (int i = 0; i < cleanup_interval_seconds && !should_terminate_; i++) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        if (should_terminate_) break;
        
        // Perform cleanup tasks
        CleanupExpiredLimits();
        
        // Save to database periodically (every 5 cleanup cycles ~ 2.5 minutes)
        static int save_counter = 0;
        if (++save_counter >= 5) {
            SaveLimiterData();
            save_counter = 0;
            
            // Log stats periodically
            Logger::info("Rate limiter stats: {} total requests, {} allowed, {} blocked, {} currently limited IPs",
                stats_.total_requests.load(), stats_.allowed_requests.load(), 
                stats_.blocked_requests.load(), stats_.currently_limited_ips.load());
        }
    }
    
    Logger::info("Rate limiter maintenance thread stopped");
}

}  // namespace Ventura
