# 🌐 GTPSWebServer

<p align="center">
  <img src="https://raw.githubusercontent.com/YoruAkio/ProjectAssets/refs/heads/main/akio/gtpswebserver.png" alt="GTPSWebServer Logo" width="180" />
</p>

<div align="center">

[![Build Executable](https://github.com/YoruAkio/GTPSWebServer/actions/workflows/build.yml/badge.svg)](https://github.com/YoruAkio/GTPSWebServer/actions/workflows/build.yml)
[![License](https://img.shields.io/github/license/YoruAkio/GTPSWebServer?color=blue)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey)](https://github.com/YoruAkio/GTPSWebServer)

</div>

<p align="center">
  <b>A high-performance, cross-platform HTTP server for Growtopia Private Servers with advanced security features</b>
</p>

---

## ✨ Features

### 🔒 Security
- **Regional Geolocation Filtering**
  - Filter requests based on the client's country of origin
  - Customizable trusted regions list in config.json
  - Protection against foreign attacks

- **Advanced Rate Limiting**
  - Protects against DDoS and brute force attacks
  - Configurable rate limits and cooldown periods
  - Persistent storage of rate limits in SQLite database
  - Memory-efficient implementation with automatic cleanup

### 🚀 Performance
- **Lightweight HTTP/HTTPS Server**
  - Powered by [cpp-httplib](https://github.com/yhirose/cpp-httplib)
  - Support for both HTTP and HTTPS protocols
  - Efficient request handling with minimal overhead

- **Smart Caching**
  - Geo-location results cached to reduce external API calls
  - Optimized memory usage for high-traffic scenarios
  - Automatic cache expiration after 24 hours

### 🔄 Integration
- **Seamless Integration with Growtopia**
  - Drop-in replacement for standard Growtopia server data provider
  - Compatible with existing Growtopia Private Server implementations
  - Proper handling of server_data.php requests

- **Multiple Geo-Location Services**
  - Fallback mechanisms between three different geo providers
  - High reliability even if some services are unavailable
  - Debug logging for geo-resolution troubleshooting

## 🔧 Installation & Setup

### System Requirements
- C++17 compatible compiler
- CMake 3.16+
- OpenSSL
- SQLite3
- libfmt
- libcurl

### Building from Source

#### Linux
```bash
# Clone the repository
git clone https://github.com/YoruAkio/GTPSWebServer.git
cd GTPSWebServer

# Create build directory and build the project
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

#### Windows
```powershell
# Clone the repository
git clone https://github.com/YoruAkio/GTPSWebServer.git
cd GTPSWebServer

# Create build directory
mkdir build
cd build

# Install dependencies and build
conan install .. --build=missing
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

## ⚙️ Configuration

Create a `config.json` file in the `bin` directory or let the program generate one for you:

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

| Parameter | Description |
|-----------|-------------|
| `ip` | IP address to bind the server to |
| `port` | Port number to listen on |
| `loginurl` | Login server URL for Growtopia |
| `rateLimit` | Maximum number of requests allowed within the rate limit window |
| `rateLimitTime` | Rate limit window in seconds |
| `trustedRegion` | Array of 2-letter country codes allowed to access the server |

## 🚀 Usage

Run the executable from the `bin` directory:

```bash
cd bin
./GTPSWebServer
```

The server will start on the configured IP and port, and will generate SSL certificates if they don't exist.

### Default Endpoints

- `GET /` - Returns "Hello World!" (basic health check)
- `GET /config` - Returns the current server configuration in JSON format
- `GET /geo-status` - Returns information about the current geo-filtering status
- `POST /growtopia/server_data.php` - Main endpoint for Growtopia client communication
- `GET /favicon.ico` - Server favicon (prevents 404 errors in browser requests)

## 📊 Monitoring & Logs

GTPSWebServer provides detailed logging via the Logger class:

- **Info level**: Server startup, configuration, and normal operations
- **Debug level**: Cache hits/misses, geo-resolutions, request details
- **Warning level**: Rate limit violations, region blocks, potential security issues
- **Error level**: Service outages, database errors, configuration problems

## 🔧 Technical Details

### Architecture

The server is designed with a modular architecture:

```
GTPSWebServer
├── HTTP Server (httplib)
├── Rate Limiter
│   └── SQLite Database
├── Geo Location Checker
│   ├── IP-API Service
│   ├── FreeGeoIP Service 
│   └── IPInfo Service
└── Configuration Manager
```

### Security Considerations

- **Geo-filtering**: Blocks requests from non-trusted countries
- **Rate limiting**: Prevents abuse and DDoS attacks
- **HTTPS Support**: Encrypted communication with clients
- **Input validation**: Prevents common web vulnerabilities

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## 📄 License

This project is licensed under the MIT LICENSE - see the [LICENSE](LICENSE) file for details.

## 📧 Contact

Project Maintainer - [YoruAkio](https://t.me/ethermite)

---

<div align="center">
  Made with ❤️ by YoruAkio for the GTPS Community
</div>
