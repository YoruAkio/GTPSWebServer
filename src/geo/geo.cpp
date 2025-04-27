#include "geo.h"
#include "../utils/logger.h"

namespace Ventura {
Geo::~Geo() {
    Logger::info("Geo object destroyed");
}

bool Geo::isAllowed(const std::string& ip) const {
    // Example: Only allow IPs starting with "192."
    bool allowed = ip.rfind("192.", 0) == 0;
    Logger::info("Checking if IP is allowed: {} -> {}", ip, allowed ? "yes" : "no");
    return allowed;
}
}  // namespace Ventura