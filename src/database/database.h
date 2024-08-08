#pragma once

#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "sqlite3.h"

namespace Ventura {
class Database {
public:
    enum class eTable { IP_BLACKLIST, RATE_LIMITER };

public:
    Database() = default;
    ~Database();

    bool open_db(const std::string& path);
    void close();
    bool loop_db();

    // TODO: Base Database
    int total_table_row(const eTable& table);
    void print_all_table_value(const eTable& table);

    // TODO: HTTPDatabase
    void find_blacklist(const std::string& ip);
    void find_rate_limiter(const std::string& ip);

    // will find and return the value of ip, time_added, cooldown_end
    std::vector<std::string> find_rate_limited(const std::string& ip);

    // TODO: RateLimiterDatabase
    bool insert_rate_limiter(const std::string& ip, const int& time_added, const int& cooldown_end);
    bool remove_rate_limiter(const std::string& ip);

    // TODO: IPBlacklistDatabase
    bool insert_blacklist(const std::string& ip);
    bool remove_blacklist(const std::string& ip);

    sqlite3* get();

public:
    static Database& Get() {
        static Database ret;
        return ret;
    }

private:
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> m_db{nullptr, sqlite3_close};
    std::unique_ptr<std::thread> m_thread{};
};
}  // namespace Ventura