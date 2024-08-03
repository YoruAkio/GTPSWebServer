#include <spdlog/spdlog.h>

#include <fstream>

#include "http/http.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "config.h"
#include "http/http.h"
#include "database/database.h"

using json = nlohmann::json;
using namespace Ventura;

int main() {
    spdlog::info("Initializing WebServer...");

    if (!std::filesystem::exists("ssl/server.crt") || !std::filesystem::exists("ssl/server.key")) {
        spdlog::error("Failed to find ssl/server.crt or ssl/server.key");
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return 1;
    } else if (!std::filesystem::exists("config.json")) {
        spdlog::error("Failed to find config.json");
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return 1;
    }

    spdlog::info("Loading WebServer config...");
    if (!Config::loadConfig()) {
        spdlog::error("Failed to load config");
        return 1;
    } else {
        spdlog::info("WebServer config loaded");
        spdlog::info(
            "Config loaded: \n"
            "ip: {}\n"
            "port: {}\n"
            "loginurl: {}\n",
            Config::ip, Config::port, Config::loginurl
        );
    }

    spdlog::info("Initializing Database...");
    Database& m_db( Database::Get() );
    if (!m_db.open_db("database.db")) {
        spdlog::error("Failed to initialize Database");
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return 1;
    }
    m_db.loop_db();

    // testing

    int time_now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    int cooldown_end = time_now + 60;
    spdlog::info("time_now: {}, cooldown_end: {}", time_now, cooldown_end);
    m_db.insert_rate_limiter("1.2.1.2", time_now, cooldown_end);
    m_db.remove_rate_limiter("1.2.1.2");
    m_db.print_all_table_value(Database::eTable::RATE_LIMITER);

    spdlog::info("Initializing HTTPServer...");
    HTTPServer& m_servers{ HTTPServer::Get() };
    if (!m_servers.listen(Config::ip)) {
        spdlog::error("Failed to initialize HTTPServer");
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return 1;
    }
    
    return 0;
}
