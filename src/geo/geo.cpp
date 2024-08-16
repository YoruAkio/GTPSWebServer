#include "geo.h"
#include <spdlog/spdlog.h>

/**
** @brief Geo object destructor
** Code is unmaintainable and not suitable for production
*/
namespace Ventura {
Geo::~Geo() {
    spdlog::info("Geo object destroyed");
}

bool Geo::isAllowed(const std::string& ip) {
    spdlog::info("Checking if IP is allowed: {}", ip);
    return true;
}
}  // namespace Ventura