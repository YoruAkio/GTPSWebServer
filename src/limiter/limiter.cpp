#include "limiter.h"

#include <spdlog/spdlog.h>

#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

#include "../config.h"
#include "../database/database.h"
#include "httplib.h"
#include "sqlite3.h"

Ventura::Database& m_db(Ventura::Database::Get());

namespace Ventura {
void Limiter::ListenRequest(const httplib::Request& req, httplib::Response& res) {
    int time_now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::string ip = req.remote_addr;

    if (m_request_count.find(ip) != m_request_count.end()) {
        if (m_request_count[ip].first < Config::rateLimit) {
            m_request_count[ip].first++;
            spdlog::info("Incremented rate limiter for IP: {}, count: {}", ip, m_request_count[ip].first);
        } else {
            spdlog::info("Added rate limiter for IP: {}, time_added: {}, cooldown_end: {}", ip, m_request_data[ip].first, m_request_data[ip].second);
            m_request_count[ip].first = 1;
            m_request_data[ip] = std::make_pair(time_now, time_now + Config::rateLimit);
            m_db.insert_rate_limiter(ip, time_now, time_now + Config::rateLimit);
            m_db.print_all_table_value(Ventura::Database::eTable::RATE_LIMITER);
        }
    } else {
        m_request_count[ip] = std::make_pair(1, Config::rateLimit);
        m_request_data[ip] = std::make_pair(1, 1);
    }

    if (m_request_data.find(ip) != m_request_data.end()) {
        if (m_request_data[ip].first == 1 && m_request_data[ip].second == 1) return;

        if (m_request_data[ip].second > time_now) {
            spdlog::info("IP: {} is rate limited", ip);
            res.status = 429;
            res.set_content("Rate limited", "text/plain");
            return;
        }
    }
}

bool Limiter::LoadLimiterData() {
    std::string sql = "SELECT * FROM rate_limiter;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(m_db.get(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(m_db.get()));
        return false;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const unsigned char* ips = sqlite3_column_text(stmt, 0);
        int time_added = sqlite3_column_int(stmt, 1);
        int cooldown_end = sqlite3_column_int(stmt, 2);

        std::string str_ips(reinterpret_cast<const char*>(ips));
        m_request_data[str_ips] = std::make_pair(time_added, cooldown_end);
    }

    sqlite3_finalize(stmt);

    spdlog::info("Loaded Limiter Data");

    return true;
}

void Limiter::Stop() {
    if (m_thread && m_thread->joinable()) {
        m_thread->join();
    }
}

Limiter::~Limiter() { this->Stop(); }
}  // namespace Ventura
