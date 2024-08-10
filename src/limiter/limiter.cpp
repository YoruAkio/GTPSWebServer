#include "limiter.h"

#include <spdlog/spdlog.h>

#include <memory>
#include <string>
#include <thread>

#include "../config.h"
#include "../database/database.h"
#include "httplib.h"

Ventura::Database& m_db( Ventura::Database::Get() );

namespace Ventura {
bool Limiter::ListenRequest(const httplib::Request& req, httplib::Response& res) {
    std::string ip = req.remote_addr;

    // adding request count into m_request_count[ip].first then set the second as Config::rateLimit
    m_request_count[ip].first++;
    if (m_request_count[ip].first == 1) {
        m_request_count[ip].second = Config::rateLimit;
    }

    spdlog::info("Request from {}, count is {}, and rate limit is {}", ip, m_request_count[ip].first, m_request_count[ip].second);

    int time_now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    // int time_cooldown is time_now + 5 mins for the cooldown
    int time_cooldown = time_now + 300;
    if (m_request_count[ip].first > m_request_count[ip].second) {
        m_db.insert_rate_limiter(ip, time_now, time_cooldown);
        m_db.print_all_table_value(Ventura::Database::eTable::RATE_LIMITER);

        m_request_data[ip] = std::make_pair(time_now, time_cooldown);

        spdlog::info("IP: {} is rate limited", ip);
        res.status = 429;
        res.set_content("Rate limited", "text/plain");

        return false;
    }

    spdlog::info("IP: {}, rateLimited at: {}, and available at: {}", ip, time_now, time_cooldown);
    if (m_request_data[ip].second > time_now) {
        m_request_count[ip].first = 0;
        spdlog::info("Reset request count: {}, current second: {}", ip, time_now);
    }

    // if (m_request_data.find(ip) != m_request_data.end()) {
    //     auto time_diff = std::chrono::duration_cast<std::chrono::seconds>(now - std::chrono::system_clock::from_time_t(m_request_data[ip].first)).count();
    //     if (time_diff < m_request_data[ip].second) {
    //         spdlog::info("IP: {} is rate limited", ip);
    //         res.status = 429;
    //         res.set_content("Rate limited", "text/plain");
    //         return false;
    //     }
    // }
    return 1;
}

void Limiter::Stop() {
    if (m_thread && m_thread->joinable()) {
        m_thread->join();
    }
}

Limiter::~Limiter() { this->Stop(); }
}  // namespace Ventura
