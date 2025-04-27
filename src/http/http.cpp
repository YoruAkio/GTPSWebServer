#include "http.h"

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <cstring>
#include <thread>
#include <chrono>
#include <fstream>
#include <vector>

#include "../limiter/limiter.h"
#include "../config.h"
#include "../geo/geo.h"
#include "httplib.h"
#include "../utils/logger.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace Ventura {
bool HTTPServer::listen(const std::string &ip) {
    m_servers = std::make_unique<httplib::SSLServer>("ssl/server.crt", "ssl/server.key");

    Logger::info("HTTPServer Initialized, listening to all requests...");

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
    Geo &m_geo{Geo::Get()};

    // Add middleware to check geolocation and rate limits for all requests
    m_servers->set_pre_routing_handler([&](const httplib::Request &req, httplib::Response &res) -> httplib::Server::HandlerResponse {
        auto start_time = std::chrono::high_resolution_clock::now();
        std::string ip = req.remote_addr;
        
        // Check if IP is allowed based on geolocation
        if (!m_geo.isAllowed(ip)) {
            Logger::warn("GEO-BLOCKED: IP {} from non-trusted region attempted to access {}", ip, req.path);
            res.status = 403;
            res.set_content("Access denied: Your region is not supported", "text/plain");
            return httplib::Server::HandlerResponse::Handled; // Stop processing this request
        }
        
        // Check rate limits if geo check passed
        bool rate_limited = m_limiter.ListenRequest(req, res);
        if (rate_limited) {
            // Response is already set in the ListenRequest method
            return httplib::Server::HandlerResponse::Handled; // Stop processing this request
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
        
        // Log successful entry with performance metrics
        Logger::debug("REQ-ALLOWED: {} {} from IP {} (Processed in {} μs)", 
            req.method, req.path, ip, duration);
        
        return httplib::Server::HandlerResponse::Unhandled; // Continue processing the request
    });

    m_servers->Get("/", [](const httplib::Request &req, httplib::Response &res) { 
        res.set_content("Hello World!", "text/plain"); 
    });

    m_servers->Get("/config", [](const httplib::Request &req, httplib::Response &res) { 
        res.set_content(Config::toJson().dump(4), "application/json"); 
    });
    
    // Add favicon.ico handler to prevent 404 errors
    m_servers->Get("/favicon.ico", [](const httplib::Request &req, httplib::Response &res) {
        // Try to read favicon file if it exists
        std::ifstream file("favicon.ico", std::ios::binary);
        if (file) {
            // Read the file content
            std::vector<char> buffer(std::istreambuf_iterator<char>(file), {});
            file.close();
            
            // Set the content type and serve the file
            res.set_content(std::string(buffer.begin(), buffer.end()), "image/x-icon");
            Logger::debug("Served favicon.ico from file");
        } else {
            // If favicon file doesn't exist, serve a minimal transparent favicon
            // This is a 1x1 transparent ICO file in binary format
            static const char transparent_favicon[] = {
                0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x00,
                0x18, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x28, 0x00,
                0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00,
                0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00
            };

            res.set_content(std::string(transparent_favicon, sizeof(transparent_favicon)), "image/x-icon");
            Logger::debug("Served default transparent favicon.ico");
        }
    });
    
    // Add new endpoint to check geolocation
    m_servers->Get("/geo-status", [&](const httplib::Request &req, httplib::Response &res) {
        std::string ip = req.remote_addr;
        bool is_allowed = m_geo.isAllowed(ip);
        
        json response;
        response["ip"] = ip;
        response["allowed"] = is_allowed;
        response["trusted_regions"] = Config::trustedRegion;
        
        res.set_content(response.dump(4), "application/json");
    });

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
            Config::ip, Config::port, Config::loginurl, meta);

        res.set_content(std::move(content), "text/html");
        return;
    });

    // Move these catch-all handlers to the end to ensure they don't override our favicon handler
    m_servers->Get(".*", [](const httplib::Request &req, httplib::Response &res) { res.status = 404; });
    m_servers->Post(".*", [](const httplib::Request &req, httplib::Response &res) { res.status = 404; });

    m_servers->set_logger([](const httplib::Request &req, const httplib::Response &res) { 
        if (res.status >= 400) {
            Logger::warn("HTTP {} [{}]: {} - Client: {}", req.method, res.status, req.path, req.remote_addr);
        } else {
            Logger::info("HTTP {} [{}]: {} - Client: {}", req.method, res.status, req.path, req.remote_addr);
        }
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