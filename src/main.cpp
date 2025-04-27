#include <filesystem>

#include "config.h"
#include "database/database.h"
#include "http/http.h"
#include "httplib.h"
#include "limiter/limiter.h"
#include "nlohmann/json.hpp"
#include "utils/logger.h"
#include "geo/geo.h"  // Include the geo header file

using json = nlohmann::json;
using namespace Ventura;

int main() {
    Logger::info("Initializing WebServer...");

    if (!std::filesystem::exists("ssl/server.crt") || !std::filesystem::exists("ssl/server.key")) {
        Logger::error("Failed to find ssl/server.crt or ssl/server.key");
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return 1;
    }

    Logger::info("Loading WebServer config...");
    if (!Config::loadConfig()) {
        Logger::error("Failed to load config");
        return 1;
    } else {
        Logger::info("WebServer config loaded");
        Config::printConfig();
    }

    Logger::info("Initializing Geolocation service...");
    Geo& geo_service = Geo::Get();
    if (!geo_service.initialize()) {
        Logger::warn("Geolocation service initialization had issues but will continue");
    } else {
        Logger::info("Geolocation service initialized successfully");
        
        // Test IP geolocation with some example IPs
        std::vector<std::string> test_ips = {"8.8.8.8", "103.10.10.10", "111.22.33.44"};
        for (const auto& ip : test_ips) {
            bool allowed = geo_service.isAllowed(ip);
            Logger::debug("Test IP {} is{} allowed", ip, allowed ? "" : " not");
        }
    }

    Logger::info("Initializing Database...");
    Database& m_db(Database::Get());
    if (!m_db.open_db("database.db")) {
        Logger::error("Failed to initialize Database");
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return 1;
    } else if (!Limiter::Get().LoadLimiterData()) {
        Logger::error("Failed to load Limiter Data");
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return 1;
    }

    int time_now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    int cooldown_end = time_now + 300;
    Logger::info("time_now: {}, cooldown_end: {}", time_now, cooldown_end);
    m_db.insert_rate_limiter("1.2.1.2", time_now, cooldown_end);
    m_db.print_all_table_value(Database::eTable::RATE_LIMITER);

    Logger::info("Initializing HTTPServer...");
    HTTPServer& m_servers{HTTPServer::Get()};
    if (!m_servers.listen(Config::ip)) {
        Logger::error("Failed to initialize HTTPServer");
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return 1;
    }

    return 0;
}