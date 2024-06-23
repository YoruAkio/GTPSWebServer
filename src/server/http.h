#include <string>
#include <iostream>
#include <future>
#include <httplib.h>

namespace GTWebServer {
    class HTTPServer {
    public:
        HTTPServer (const std::string& host, const uint16_t& port);
        ~HTTPServer();

        bool listen();
        void stop();
        void thread();

        bool bind_to_port(const std::pair<std::string, uint16_t>& val) {
            return m_server->bind_to_port(val.first.c_str(), val.second);
        }
        
    private:
        std::unique_ptr<httplib::SSLServer> m_server{};
        std::pair<std::string, uint16_t> m_config{};
    };
}

