#include "database.h"
#include <spdlog/spdlog.h>
#include <iostream>
#include "sqlite3.h"

namespace Ventura {
bool Database::open_db(const std::string& path) {
    sqlite3* db = nullptr;
    int rc = sqlite3_open(path.c_str(), &db);
    m_db.reset(db);
    if (rc) {
        spdlog::error("Can't open database: {}", sqlite3_errmsg(m_db.get()));
        return false;
    }

    char* zErrMsg = 0;
    const char* sql = "CREATE TABLE IF NOT EXISTS ip_blacklist (ip TEXT PRIMARY KEY NOT NULL);"
                      "CREATE TABLE IF NOT EXISTS rate_limiter (ip TEXT PRIMARY KEY NOT NULL, count INTEGER NOT NULL, last_access INTEGER NOT NULL);";
    rc = sqlite3_exec(m_db.get(), sql, nullptr, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        spdlog::error("SQL error: {}", zErrMsg);
        sqlite3_free(zErrMsg);
        return false;
    }
    
    return true;
}

Database::~Database() {
    this->close();
}

void Database::close() {
    if (m_db) {
        sqlite3_close(m_db.get());
        m_db.reset();
    }
}

sqlite3* Database::get() {
    return m_db.get();
}

}  // namespace Ventura