#pragma once

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

inline bool loadConfig() {
    std::ifstream file("config.json");
    if (!file.is_open()) {
        return false;
    }

    json j;
    file >> j;

    try {
        ip = j["ip"].get<std::string>();
        port = j["port"].get<int>();
        loginurl = j["loginurl"].get<std::string>();
        rateLimit = j["rateLimit"].get<int>();
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse config: {}", e.what());
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
    return j;
}
}  // namespace Config
}  // namespace Ventura
