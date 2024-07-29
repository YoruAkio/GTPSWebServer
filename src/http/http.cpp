#include <fmt/core.h>
#include <fmt/ranges.h>
#include <spdlog/spdlog.h>

#include <cstring>
#include <thread>

#include "config.h"
#include "http.h"
#include "httplib.h"

namespace Ventura {
    bool HTTPServer::listen(const std::string& ip, uint16_t port) {
        m_server = std::make_unique<httplib::SSLServer>("ssl/server.crt", "ssl/server.key");

        spdlog::info("HTTPServer Initialized, listening on {}:{}", ip, port);

        m_server->bind_to_port(ip.c_str(), port);
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
        m_server->Get("/", [](const httplib::Request &req, httplib::Response &res) {
            res.set_content("Hello World!", "text/plain");
        });

        m_server->Get("/config", [](const httplib::Request &req, httplib::Response &res) {
            res.set_content(Config::toJson().dump(), "application/json");
        });

        m_server->listen_after_bind();
        while (true);
    }

    void HTTPServer::stop() {
        if (m_server) {
            m_server->stop();
            // m_server.release();
        }
    }
}  // namespace Ventura