#pragma once
#include <cstdint>

namespace Ventura {
class Geo{
public:
    Geo() = default;
    ~Geo();

    // TODO: implement this function
    bool isAllowed(const std::string& ip);
    
public:
    static Geo& Get() {
        static Geo ret;
        return ret;
    }
};
}