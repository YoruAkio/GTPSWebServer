#pragma once
#include <fmt/core.h>
#include <fmt/color.h>
#include <chrono>
#include <ctime>
#include <iostream>

namespace Logger {

inline std::string current_time() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    char buf[20];
    std::strftime(buf, sizeof(buf), "%F %T", std::localtime(&now_c));
    return buf;
}

template<typename... Args>
void info(const char* fmt_str, Args&&... args) {
    fmt::print(fmt::fg(fmt::color::cyan), "[{}] [INFO ] ", current_time());
    fmt::print(fmt_str, std::forward<Args>(args)...);
    fmt::print("\n");
}

template<typename... Args>
void error(const char* fmt_str, Args&&... args) {
    fmt::print(fmt::fg(fmt::color::red), "[{}] [ERROR] ", current_time());
    fmt::print(fmt_str, std::forward<Args>(args)...);
    fmt::print("\n");
}

template<typename... Args>
void warn(const char* fmt_str, Args&&... args) {
    fmt::print(fmt::fg(fmt::color::yellow), "[{}] [WARN ] ", current_time());
    fmt::print(fmt_str, std::forward<Args>(args)...);
    fmt::print("\n");
}

template<typename... Args>
void debug(const char* fmt_str, Args&&... args) {
    fmt::print(fmt::fg(fmt::color::green), "[{}] [DEBUG] ", current_time());
    fmt::print(fmt_str, std::forward<Args>(args)...);
    fmt::print("\n");
}

} // namespace Logger