#include <fmt/core.h>
#include <fmt/ranges.h>
#include <spdlog/spdlog.h>

#include <cstring>
#include <thread>

#include "config.h"
#include "http.h"
#include "httplib.h"

namespace Ventura {
    bool HTTPServer::listen(const std::string& ip) {
        m_servers = std::make_unique<httplib::SSLServer>("ssl/server.crt", "ssl/server.key");

        spdlog::info("HTTPServer Initialized, listening on port 443");

        m_servers->bind_to_port(ip.c_str(), 443);
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
        m_servers->Get("/", [](const httplib::Request &req, httplib::Response &res) {
            res.set_content("Hello World!", "text/plain");
        });

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