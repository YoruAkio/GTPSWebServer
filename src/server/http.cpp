#include <config.h>
#include <future>
#include <server/http.h>
#include <fmt/core.h>

namespace GTWebServer {
    HTTPServer::HTTPServer(const std::string& host, const uint16_t& port) {
        m_config = std::make_pair(host, port);
        m_server = std::make_unique<httplib::SSLServer>("./cache/cert.pem", "./cache/key.pem");
    }

    HTTPServer::~HTTPServer() { this->stop(); }

    bool HTTPServer::listen() {
        fmt::print(" - binding HTTPServer to {}:{}.\n", m_config.first, m_config.second);
        if (!this->bind_to_port(m_config)) {
            fmt::print(" - failed to bind HTTPServer's port with {}.\n", m_config.second);
            return false;
        }
        std::thread(&HTTPServer::thread, this).detach();
        return true; 
    }

    void HTTPServer::stop() { 
        m_server->stop();
        m_server.release();
    }

    void HTTPServer::thread() {
        m_server->Post("/growtopia/server_data.php", [&](const httplib::Request& req, httplib::Response& res) {
            if (req.params.empty() || req.get_header_value("User-Agent").find("UbiServices_SDK") == std::string::npos) {
                res.status = 403;
                return;
            }
            std::string content = fmt::format("server={}\nport={}\ntype=1\n#maint=Server is under maintenance. We will be back online shortly. Thank you for your patience!\nmeta=DIKHEAD\nRTENDMARKERBS1001\n",
                config::http::gt::address, config::http::gt::port);
            res.set_content(std::move(content), "text/html");
            return;
        });

        m_server->listen_after_bind();
        while(true);
    }
} // namespace GTWebServer