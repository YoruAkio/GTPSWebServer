#include <iostream>
#include <string>
#include <future>

#include <config.h>
#include <httplib.h>
#include <server/http.h>
#include <fmt/core.h>

int main() {
    fmt::print("Starting WebServer on {}:{}.\n", config::http::address, config::http::port);
    GTWebServer::HTTPServer server(config::http::address, config::http::port);
    return 0;
}

