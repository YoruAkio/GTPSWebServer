#include "database.h"

#include <iostream>
#include <thread>

#include "../limiter/limiter.h"
#include "../utils/logger.h"
#include "sqlite3.h"

namespace Ventura {
bool Database::open_db(const std::string& path) {
    sqlite3* db = nullptr;
    int rc = sqlite3_open(path.c_str(), &db);
    m_db.reset(db);
    if (rc) {
        Logger::error("Can't open database: {}", sqlite3_errmsg(m_db.get()));
        return false;
    }

    char* zErrMsg = 0;
    const char* sql =
        "CREATE TABLE IF NOT EXISTS ip_blacklist (ip TEXT PRIMARY KEY NOT NULL);"
        "CREATE TABLE IF NOT EXISTS rate_limiter (ip TEXT PRIMARY KEY NOT NULL, time_added INTEGER NOT NULL, cooldown_end INTEGER NOT NULL);";
    rc = sqlite3_exec(m_db.get(), sql, nullptr, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        Logger::error("SQL error: {}", zErrMsg);
        sqlite3_free(zErrMsg);
        return false;
    }

    m_thread = std::make_unique<std::thread>([this]() {
        while (true) {
            std::string sql = "SELECT * FROM rate_limiter;";
            sqlite3_stmt* stmt;
            int rc = sqlite3_prepare_v2(m_db.get(), sql.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                Logger::error("Failed to prepare statement: {}", sqlite3_errmsg(m_db.get()));
                std::this_thread::sleep_for(std::chrono::seconds(10));
                continue;
            }
            int time_now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            Logger::info("Checking rate limiter table...");
            Logger::info("Time Now: {}", time_now);

            while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
                std::string ip = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
                int time_added = sqlite3_column_int(stmt, 1);
                int cooldown_end = sqlite3_column_int(stmt, 2);

                Logger::info("IP: {}, Time Added: {}, Cooldown End: {}", ip, time_added, cooldown_end);

                if (time_now > cooldown_end) {
                    this->remove_rate_limiter(ip);

                    Logger::info("Removed rate limiter for IP: {}", ip);
                }
            }

            sqlite3_finalize(stmt);
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    });

    return true;
}

int Database::total_table_row(const eTable& table) {
    std::string sql;
    switch (table) {
    case eTable::IP_BLACKLIST:
        sql = "SELECT COUNT(*) FROM ip_blacklist;";
        break;
    case eTable::RATE_LIMITER:
        sql = "SELECT COUNT(*) FROM rate_limiter;";
        break;
    }

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(m_db.get(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        Logger::error("Failed to prepare statement: {}", sqlite3_errmsg(m_db.get()));
        return -1;
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        Logger::error("Failed to step statement: {}", sqlite3_errmsg(m_db.get()));
        return -1;
    }

    return sqlite3_column_int(stmt, 0);
}

void Database::print_all_table_value(const eTable& table) {
    std::string sql;
    switch (table) {
    case eTable::IP_BLACKLIST:
        sql = "SELECT * FROM ip_blacklist;";
        break;
    case eTable::RATE_LIMITER:
        sql = "SELECT * FROM rate_limiter;";
        break;
    }

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(m_db.get(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        Logger::error("Failed to prepare statement: {}", sqlite3_errmsg(m_db.get()));
        return;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        for (int i = 0; i < sqlite3_column_count(stmt); i++) {
            std::cout << sqlite3_column_text(stmt, i) << " ";
        }
        std::cout << std::endl;
    }
}

std::vector<std::string> Database::find_rate_limited(const std::string& ip) {
    std::string sql = "SELECT * FROM rate_limiter WHERE ip = ?;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(m_db.get(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        Logger::error("Failed to prepare statement: {}", sqlite3_errmsg(m_db.get()));
        return {};
    }

    rc = sqlite3_bind_text(stmt, 1, ip.c_str(), -1, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        Logger::error("Failed to bind text: {}", sqlite3_errmsg(m_db.get()));
        return {};
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        Logger::error("Failed to step statement: {}", sqlite3_errmsg(m_db.get()));
        return {};
    }

    std::vector<std::string> ret;
    for (int i = 0; i < sqlite3_column_count(stmt); i++) {
        ret.push_back(std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, i))));
    }

    return ret;
}

bool Database::insert_rate_limiter(const std::string& ip, const int& time_added, const int& cooldown_end) {
    std::string sql = "INSERT INTO rate_limiter (ip, time_added, cooldown_end) VALUES (?, ?, ?) ON CONFLICT(ip) DO UPDATE SET time_added = ?, cooldown_end = ?;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(m_db.get(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        Logger::error("Failed to prepare statement: {}", sqlite3_errmsg(m_db.get()));
        return -1;
    }

    rc = sqlite3_bind_text(stmt, 1, ip.c_str(), -1, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        Logger::error("Failed to bind text: {}", sqlite3_errmsg(m_db.get()));
        return -1;
    }

    rc = sqlite3_bind_int(stmt, 2, time_added);
    if (rc != SQLITE_OK) {
        Logger::error("Failed to bind int: {}", sqlite3_errmsg(m_db.get()));
        return -1;
    }

    rc = sqlite3_bind_int(stmt, 3, cooldown_end);
    if (rc != SQLITE_OK) {
        Logger::error("Failed to bind int: {}", sqlite3_errmsg(m_db.get()));
        return -1;
    }

    rc = sqlite3_bind_int(stmt, 4, time_added);
    if (rc != SQLITE_OK) {
        Logger::error("Failed to bind int: {}", sqlite3_errmsg(m_db.get()));
        return -1;
    }

    rc = sqlite3_bind_int(stmt, 5, cooldown_end);
    if (rc != SQLITE_OK) {
        Logger::error("Failed to bind int: {}", sqlite3_errmsg(m_db.get()));
        return -1;
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        Logger::error("Failed to step statement: {}", sqlite3_errmsg(m_db.get()));
        return -1;
    }

    sqlite3_reset(stmt);
    return 1;
}

bool Database::remove_rate_limiter(const std::string& ip) {
    std::string sql = "DELETE FROM rate_limiter WHERE ip = ?;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(m_db.get(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        Logger::error("Failed to prepare statement: {}", sqlite3_errmsg(m_db.get()));
        return -1;
    }

    rc = sqlite3_bind_text(stmt, 1, ip.c_str(), -1, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        Logger::error("Failed to bind text: {}", sqlite3_errmsg(m_db.get()));
        return -1;
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        Logger::error("Failed to step statement: {}", sqlite3_errmsg(m_db.get()));
        return -1;
    }

    sqlite3_reset(stmt);
    return 1;
}

bool Database::insert_blacklist(const std::string& ip) {
    std::string sql = "INSERT INTO ip_blacklist (ip) VALUES (?) ON CONFLICT(ip) DO NOTHING;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(m_db.get(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        Logger::error("Failed to prepare statement: {}", sqlite3_errmsg(m_db.get()));
        return -1;
    }

    rc = sqlite3_bind_text(stmt, 1, ip.c_str(), -1, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        Logger::error("Failed to bind text: {}", sqlite3_errmsg(m_db.get()));
        return -1;
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        Logger::error("Failed to step statement: {}", sqlite3_errmsg(m_db.get()));
        return -1;
    }

    sqlite3_reset(stmt);
    return 1;
}

bool Database::remove_blacklist(const std::string& ip) {
    std::string sql = "DELETE FROM ip_blacklist WHERE ip = ?;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(m_db.get(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        Logger::error("Failed to prepare statement: {}", sqlite3_errmsg(m_db.get()));
        return -1;
    }

    rc = sqlite3_bind_text(stmt, 1, ip.c_str(), -1, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        Logger::error("Failed to bind text: {}", sqlite3_errmsg(m_db.get()));
        return -1;
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        Logger::error("Failed to step statement: {}", sqlite3_errmsg(m_db.get()));
        return -1;
    }

    sqlite3_reset(stmt);
    return 1;
}

Database::~Database() {
    this->close();
    if (m_thread && m_thread->joinable()) {
        m_thread->join();
    }
}

void Database::close() {
    if (m_db) {
        sqlite3_close(m_db.get());
        m_db.reset();
    }
}

sqlite3* Database::get() { return m_db.get(); }

}  // namespace Ventura