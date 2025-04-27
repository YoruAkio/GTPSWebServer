# GTPSWebServer

[![Build Executable](https://github.com/yourusername/GTPSWebServer/actions/workflows/build.yml/badge.svg)](https://github.com/yourusername/GTPSWebServer/actions/workflows/build.yml)

A modern, secure GTPS web server with features including:
- HTTP/HTTPS Support via cpp-httplib
- Regional Geolocation Filtering
- Advanced Rate Limiting
- SQLite Database Integration

## Features

### Geo Location Filtering
- Filters requests based on the country of origin
- Customizable trusted regions list in config.json
- Multiple lookup services with fallback mechanisms
- Memory caching for improved performance

### Rate Limiting
- Protects against abuse and DDoS attacks
- Configurable rate limits and cooldown periods
- Persistent storage of rate limits in SQLite database
- Detailed statistics and logging

## Building

### Requirements
- C++17 compiler
- CMake 3.16+
- OpenSSL
- SQLite3
- libfmt
- libcurl

### Linux
```bash
mkdir -p build && cd build
cmake ..
cmake --build .
```

### Windows
```bash
mkdir build
cd build
conan install .. --build=missing
cmake ..
cmake --build .
```

## Configuration
Edit `bin/config.json` to configure:
```json
{
  "ip": "127.0.0.1",
  "port": 17091,
  "loginurl": "gtbackend-login.vercel.app",
  "rateLimit": 50,
  "rateLimitTime": 300,
  "trustedRegion": ["ID", "SG", "MY"]
}
```

## License
See [LICENSE](LICENSE) file for details.
