#pragma once
#include <string>
#include <cstdint>
#include <string_view>

#define HTTP_SERVER

using namespace std;

namespace config {
    namespace http {
        constexpr std::string address = "0.0.0.0";
        constexpr uint16_t port = 443;
        namespace gt {
            constexpr std::string_view address = "127.0.0.1";
            constexpr uint16_t port = 17091;
        }
    }
    namespace server_default {
        constexpr std::string_view address = "127.0.0.1";
        constexpr uint16_t port = 17091;
    }
}