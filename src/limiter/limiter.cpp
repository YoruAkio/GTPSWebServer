#include <spdlog/spdlog.h>

#include <memory>
#include <string>
#include <thread>

#include "../database/database.h"
#include "httplib.h"
#include "limiter.h"

namespace Ventura {
bool Limiter::ListenRequest(const httplib::Request& req, httplib::Response& res) {
    // Getting the IP Address
    std::string ip = req.remote_addr;

    auto now = std::chrono::system_clock::now();
    
    // print the ip data is not just return user ip is know limited
    if (m_request_data.find(ip) != m_request_data.end()) {
        auto time_diff = std::chrono::duration_cast<std::chrono::seconds>(now - std::chrono::system_clock::from_time_t(m_request_data[ip].first)).count();
        if (time_diff < m_request_data[ip].second) {
            spdlog::info("IP: {} is rate limited", ip);
            res.status = 429;
            res.set_content("Rate limited", "text/plain");
            return false;
        }
    } else {
        auto now_seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        auto cooldown_end = now_seconds + 60; // 60 seconds
        m_request_data[ip] = { now_seconds, cooldown_end };
        spdlog::info("IP: {} is not rate limited", ip);
    }

    return 1;
}

void Limiter::Stop() {
    if (m_thread && m_thread->joinable()) {
        m_thread->join();
    }
}

Limiter::~Limiter() {
    this->Stop();
}
}  // namespace Ventura
