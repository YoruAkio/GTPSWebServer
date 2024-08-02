#pragma once

#include <string>
#include <thread>
#include <memory>
#include "sqlite3.h"

namespace Ventura {
class Database {
public:
    Database() = default;
    ~Database();

    bool open_db(const std::string& path);
    void close();
    
    sqlite3* get();

public:
    static Database& Get() {
        static Database ret;
        return ret;
    }

private:
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> m_db{nullptr, sqlite3_close};
    // std::unique_ptr<std::thread> m_thread{};
};
}  // namespace Ventura