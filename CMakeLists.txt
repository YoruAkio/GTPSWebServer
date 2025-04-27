cmake_minimum_required(VERSION 3.16)

project(GTPSWebServer)

# Enable C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set the output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# Find packages directly from the system
find_package(OpenSSL REQUIRED)

# Find SQLite3 (Ubuntu package doesn't provide SQLite3::SQLite3 target)
find_path(SQLITE3_INCLUDE_DIR sqlite3.h)
find_library(SQLITE3_LIBRARY sqlite3)
if(SQLITE3_INCLUDE_DIR AND SQLITE3_LIBRARY)
    message(STATUS "Found SQLite3: ${SQLITE3_LIBRARY}")
    add_library(SQLite3::SQLite3 UNKNOWN IMPORTED)
    set_target_properties(SQLite3::SQLite3 PROPERTIES
        IMPORTED_LOCATION "${SQLITE3_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SQLITE3_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "Could not find SQLite3 library")
endif()

# Find fmt library
find_package(fmt QUIET)
if(NOT fmt_FOUND)
    # Create custom target if system fmt not found
    find_path(FMT_INCLUDE_DIR fmt/core.h)
    find_library(FMT_LIBRARY fmt)
    if(FMT_INCLUDE_DIR AND FMT_LIBRARY)
        message(STATUS "Found fmt: ${FMT_LIBRARY}")
        add_library(fmt UNKNOWN IMPORTED)
        set_target_properties(fmt PROPERTIES
            IMPORTED_LOCATION "${FMT_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${FMT_INCLUDE_DIR}")
    else()
        message(FATAL_ERROR "Could not find fmt library")
    endif()
endif()

# Add include directories for external libraries
include_directories(
    ${OPENSSL_INCLUDE_DIR}
    ${SQLITE3_INCLUDE_DIR}
    ${FMT_INCLUDE_DIR}
    vendor/
    vendor/httplib
)

# Add subdirectories
add_subdirectory(src)