#include <filesystem>

#include "config.h"
#include "database/database.h"
#include "http/http.h"
#include "httplib.h"
#include "limiter/limiter.h"
#include "nlohmann/json.hpp"
#include "utils/logger.h"
#include "geo/geo.h"

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
        
        // Start config file monitoring
        Logger::info("Starting config file monitor...");
        Config::startConfigMonitor();
    }

    Logger::info("Initializing Geolocation service...");
    Geo& geo_service = Geo::Get();
    if (!geo_service.initialize()) {
        Logger::warn("Geolocation service initialization had issues but will continue");
    } else {
        Logger::info("Geolocation service initialized successfully");
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

    Logger::info("Initializing HTTPServer...");
    HTTPServer& m_servers{HTTPServer::Get()};
    if (!m_servers.listen(Config::ip)) {
        // Stop the config monitor before exiting
        Config::stopConfigMonitor();
        Logger::error("Failed to initialize HTTPServer");
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return 1;
    }

    return 0;
}