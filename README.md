# GTPSWebServer

HTTPServer for Growtopia Private Server to login and play GTPS

## TODO Lists

- [x] Serve server_data.php path to connect to GTPS
- [x] Request checker to prevent request getting server data
- [ ] Adding public folder `cache/` for caching
- [ ] Implement rate limiting to prevent DDoS
- [ ] Implement IP Blocking to end request if IP is blocked
- [ ] Geo Location checker to prevent request from other country
- [ ] Better Logging System
- [ ] Implementing reverse proxy to prevent DDoS

## Build

The following dependencies are required to build this project:

- [CMake](https://cmake.org/)
- [Conan](https://conan.io/)

Building the project:

1. CLone this repository:

    ```bash
    git clone https://github.com/YoruAkio/GTPSWebServer.git
    cd GTPSWebServer
    ```

2. Install Python 3.10 or higher (For Windows User: select 'Add Python to PATH' during installation)
3. Install Conan:

    ```bash
    pip install conan
    ```

4. Follow this step to building the project:

    ```bash
    mkdir build
    cd build
    cmake ..
    cmake --build .
    ```

## Notes

- This project is still in development, so there are many bugs and issues.
- This project is tested on Arch Linux Based OS, and Windows will be tested soon.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
