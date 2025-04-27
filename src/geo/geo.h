#pragma once
#include <cstdint>
#include <string>

namespace Ventura {
class Geo{
public:
    Geo() = default;
    ~Geo();

    // Improved: add const qualifier for member function
    bool isAllowed(const std::string& ip) const;
    
public:
    static Geo& Get() {
        static Geo ret;
        return ret;
    }
};
}