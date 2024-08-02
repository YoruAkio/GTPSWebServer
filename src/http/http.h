#pragma once
#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <thread>
#include "httplib.h"

namespace Ventura {
class HTTPServer {
public:
    HTTPServer() = default;
    ~HTTPServer();

    bool listen(const std::string& ip);
    void stop();
    void thread();

public:
    static HTTPServer& Get() {
        static HTTPServer ret;
        return ret;
    }

private:
    std::unique_ptr<httplib::SSLServer> m_servers{};
    std::unique_ptr<std::thread> m_thread{};
};
}  // namespace Ventura