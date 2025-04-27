#pragma once

#include "../src/utils/logger.h"

#include <fstream>
#include <string>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>

#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace Ventura {
namespace Config {
inline std::string ip;
inline int port;
inline std::string loginurl;
inline int rateLimit;
inline int rateLimitTime;
inline std::vector<std::string> trustedRegion;

// File monitoring variables
inline std::string configPath = "config.json";
inline std::atomic<bool> configMonitorRunning{false};
inline std::mutex configMutex;
inline std::chrono::system_clock::time_point lastModifiedTime;

// Get the last modified time of a file
inline std::chrono::system_clock::time_point getFileModifiedTime(const std::string& filePath) {
    try {
        auto fileTime = std::filesystem::last_write_time(filePath);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            fileTime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
        return sctp;
    } catch (const std::exception& e) {
        Logger::error("Failed to get file modification time: {}", e.what());
        return std::chrono::system_clock::now();
    }
}

inline bool makeConfig() {
    json j;
    j["ip"] = "127.0.0.1";
    j["port"] = 17091;
    j["loginurl"] = "gtbackend-login.vercel.app";
    j["rateLimit"] = 50;
    j["rateLimitTime"] = 60 * 5;  // seconds
    // array of trusted region
    j["trustedRegion"] = {"ID", "SG", "MY"};  // only accept request from this region only

    try {
        std::ofstream file(configPath);
        file << j.dump(4);
        file.close();
    } catch (const std::exception& e) {
        Logger::error("Failed to create config.json: {}", e.what());
        return false;
    }

    return true;
}

inline json toJson() {
    std::lock_guard<std::mutex> lock(configMutex);
    json j;
    j["ip"] = ip;
    j["port"] = port;
    j["loginurl"] = loginurl;
    j["rateLimit"] = rateLimit;
    j["rateLimitTime"] = rateLimitTime;
    j["trustedRegion"] = trustedRegion;
    return j;
}

inline void printConfig() {
    std::lock_guard<std::mutex> lock(configMutex);
    Logger::info("Config:");
    Logger::info("  ip: {}", ip);
    Logger::info("  port: {}", port);
    Logger::info("  loginurl: {}", loginurl);
    Logger::info("  rateLimit: {}", rateLimit);
    Logger::info("  rateLimitTime: {}", rateLimitTime);
    Logger::info("  trustedRegion: {}", fmt::join(trustedRegion, ", "));
}

inline bool loadConfig() {
    std::ifstream file(configPath);
    // if file is not found, create it
    if (!file.is_open()) {
        if (!makeConfig()) {
            Logger::error("Failed to create config.json");
            return false;
        }
        file.open(configPath);
    }

    json j;
    file >> j;

    try {
        std::lock_guard<std::mutex> lock(configMutex);
        ip = j["ip"].get<std::string>();
        port = j["port"].get<int>();
        loginurl = j["loginurl"].get<std::string>();
        rateLimit = j["rateLimit"].get<int>();
        rateLimitTime = j["rateLimitTime"].get<int>();
        trustedRegion = j["trustedRegion"].get<std::vector<std::string>>();
        
        // Update last modified time
        lastModifiedTime = getFileModifiedTime(configPath);
    } catch (const std::exception& e) {
        Logger::error("Failed to parse config: {}", e.what());
        return false;
    }

    return true;
}

inline bool checkAndReloadConfig() {
    try {
        auto currentModTime = getFileModifiedTime(configPath);
        
        // Check if file was modified
        if (currentModTime > lastModifiedTime) {
            Logger::info("Config file change detected. Reloading configuration...");
            if (loadConfig()) {
                Logger::info("Configuration reloaded successfully.");
                printConfig();
                return true;
            } else {
                Logger::error("Failed to reload configuration.");
                return false;
            }
        }
        return true; // No changes needed
    } catch (const std::exception& e) {
        Logger::error("Error checking config file: {}", e.what());
        return false;
    }
}

inline void startConfigMonitor(int checkIntervalMs = 2000) {
    if (configMonitorRunning) {
        return; // Already running
    }
    
    configMonitorRunning = true;
    
    // Start monitoring thread
    std::thread([checkIntervalMs]() {
        Logger::info("Config file monitor started. Checking for changes every {} ms", checkIntervalMs);
        while (configMonitorRunning) {
            checkAndReloadConfig();
            std::this_thread::sleep_for(std::chrono::milliseconds(checkIntervalMs));
        }
        Logger::info("Config file monitor stopped.");
    }).detach();
}

inline void stopConfigMonitor() {
    configMonitorRunning = false;
}

}  // namespace Config
}  // namespace Ventura
