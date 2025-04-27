#pragma once

#include "../src/utils/logger.h"

#include <fstream>
#include <string>

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
        std::ofstream file("config.json");
        file << j.dump(4);
        file.close();
    } catch (const std::exception& e) {
        Logger::error("Failed to create config.json: {}", e.what());
        return false;
    }

    return true;
}

inline bool loadConfig() {
    std::ifstream file("config.json");
    // if file is not found, create it
    if (!file.is_open()) {
        if (!makeConfig()) {
            Logger::error("Failed to create config.json");
            return false;
        }
        file.open("config.json");
    }

    json j;
    file >> j;

    try {
        ip = j["ip"].get<std::string>();
        port = j["port"].get<int>();
        loginurl = j["loginurl"].get<std::string>();
        rateLimit = j["rateLimit"].get<int>();
        rateLimitTime = j["rateLimitTime"].get<int>();
        trustedRegion = j["trustedRegion"].get<std::vector<std::string>>();
    } catch (const std::exception& e) {
        Logger::error("Failed to parse config: {}", e.what());
        return false;
    }

    return true;
}

inline json toJson() {
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
    Logger::info("Config:");
    Logger::info("  ip: {}", ip);
    Logger::info("  port: {}", port);
    Logger::info("  loginurl: {}", loginurl);
    Logger::info("  rateLimit: {}", rateLimit);
    Logger::info("  rateLimitTime: {}", rateLimitTime);
    Logger::info("  trustedRegion: {}", fmt::join(trustedRegion, ", "));
}
}  // namespace Config
}  // namespace Ventura
