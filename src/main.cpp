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

    if (!Config::loadConfig()) {
        spdlog::error("Failed to load config");
        return 1;
    } else {
        spdlog::info("HTTPServer config loaded");
    }

    HTTPServer& m_server{ HTTPServer::Get() };
    if (!m_server.listen(Config::ip, Config::port)) {
        spdlog::error("Failed to listen on {}:{}", Config::ip, Config::port);
        return 1;
    }
    
    return 0;
}
