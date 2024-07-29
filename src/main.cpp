#include <spdlog/spdlog.h>

#include <fstream>

#include "http/http.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "config.h"
#include "http/http.h"

using json = nlohmann::json;
using namespace Ventura;

int main() {
    spdlog::info("Initializing WebServer");

    if (!std::filesystem::exists("ssl/server.crt") || !std::filesystem::exists("ssl/server.key")) {
        spdlog::error("Failed to find ssl/server.crt or ssl/server.key");
        return 1;
    } else if (!std::filesystem::exists("config.json")) {
        spdlog::error("Failed to find config.json");
        return 1;
    }

    if (!Config::loadConfig()) {
        spdlog::error("Failed to load config");
        return 1;
    } else {
        spdlog::info("HTTPServer config loaded");
    }

    HTTPServer& m_servers{ HTTPServer::Get() };
    if (!m_servers.listen(Config::ip)) {
        spdlog::error("Failed to initialize HTTPServer");
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return 1;
    }
    
    return 0;
}
