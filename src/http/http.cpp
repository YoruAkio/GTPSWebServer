#include "http.h"

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <cstring>
#include <thread>

#include "../limiter/limiter.h"
#include "config.h"
#include "httplib.h"
#include "spdlog/spdlog.h"

namespace Ventura {
bool HTTPServer::listen(const std::string &ip) {
    m_servers = std::make_unique<httplib::SSLServer>("ssl/server.crt", "ssl/server.key");

    spdlog::info("HTTPServer Initialized, listening to all requests...");

    m_servers->bind_to_port("0.0.0.0", 443);
    m_thread = std::make_unique<std::thread>(&HTTPServer::thread, this);
    return true;
}

HTTPServer::~HTTPServer() {
    this->stop();
    if (m_thread && m_thread->joinable()) {
        m_thread->join();
    }
}

void HTTPServer::thread() {
    Limiter &m_limiter{Limiter::Get()};

    m_servers->Get(".*", [&](const httplib::Request &req, httplib::Response &res) { m_limiter.ListenRequest(req, res); });

    m_servers->Get("/", [](const httplib::Request &req, httplib::Response &res) { res.set_content("Hello World!", "text/plain"); });

    m_servers->Get("/config", [](const httplib::Request &req, httplib::Response &res) { res.set_content(Config::toJson().dump(4), "application/json"); });

    m_servers->Post("/growtopia/server_data.php", [&](const httplib::Request &req, httplib::Response &res) {
        if (req.params.empty() || req.get_header_value("User-Agent").find("UbiServices_SDK") == std::string::npos) {
            res.status = 403;
            return;
        }
        std::string meta = fmt::format("Ventura_{}", std::rand() % 9000 + 1000);
        std::string content = fmt::format(
            "server|{}\n"
            "port|{}\n"
            "type|1\n"
            "# maint|Server is currently down for maintenance. We will be back soon!\n"
            "loginurl|{}\n"
            "meta|{}\n",
            "RTENDMARKERBS1001", Config::ip, Config::port, Config::loginurl, meta);

        res.set_content(std::move(content), "text/html");
        return;
    });

    m_servers->Get(".*", [](const httplib::Request &req, httplib::Response &res) { res.status = 404; });
    m_servers->Post(".*", [](const httplib::Request &req, httplib::Response &res) { res.status = 404; });

    m_servers->set_logger([](const httplib::Request &req, const httplib::Response &res) { spdlog::info("{} [{}]: {}", req.method, res.status, req.path); });

    m_servers->listen_after_bind();
    while (true);
}

void HTTPServer::stop() {
    if (m_servers) {
        m_servers->stop();
        // m_servers.release();
    }
}
}  // namespace Ventura