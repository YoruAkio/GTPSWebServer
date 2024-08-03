#pragma once

#include <memory>
#include <string>
#include <thread>

#include "../database/database.h"
#include "httplib.h"

namespace Ventura {
class Limiter {
public:
    Limiter() = default;
    ~Limiter();

    // TODO: Base Limiter
    bool ListenRequest(const httplib::Request& req, httplib::Response& res);
    void Stop();

public:
    static Limiter& Get() {
        static Limiter ret;
        return ret;
    }

private:
    std::unique_ptr<std::thread> m_thread{};
    std::unordered_map<std::string, std::pair<int, int>> m_request_data{};

    int reqCount = 0;
};
}  // namespace Ventura