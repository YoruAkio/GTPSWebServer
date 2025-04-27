cmake_minimum_required(VERSION 3.16)

project(GTPSWebServer)

# Enable C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set the output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# Check if we're using Conan (Windows builds)
if(EXISTS "${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
    include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    conan_basic_setup(TARGETS)
    
    # Include Conan-generated CMake find scripts
    set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH})
    set(CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR} ${CMAKE_PREFIX_PATH})
endif()

# Find packages 
find_package(OpenSSL REQUIRED)

# Find CURL - try Conan's FindCURL first, then system
find_package(CURL QUIET)
if(NOT CURL_FOUND)
    message(STATUS "CURL not found with normal find_package, trying alternative methods...")
    
    # Try to find CURL from Conan using CMake Find modules 
    if(EXISTS "${CMAKE_BINARY_DIR}/FindCURL.cmake")
        message(STATUS "Using Conan's FindCURL module")
        find_package(CURL REQUIRED)
    else()
        # Manual search for Windows
        if(WIN32)
            find_path(CURL_INCLUDE_DIR curl/curl.h
                PATHS
                "${CONAN_INCLUDE_DIRS_LIBCURL}"
                "$ENV{ProgramFiles}/curl/include"
            )
            
            find_library(CURL_LIBRARY
                NAMES libcurl_imp libcurl curl
                PATHS
                "${CONAN_LIB_DIRS_LIBCURL}"
                "$ENV{ProgramFiles}/curl/lib"
            )
            
            if(CURL_INCLUDE_DIR AND CURL_LIBRARY)
                set(CURL_FOUND TRUE)
                message(STATUS "Found CURL manually: ${CURL_LIBRARY}")
            endif()
        endif()
    endif()
endif()

if(CURL_FOUND)
    message(STATUS "Found CURL: ${CURL_LIBRARIES}")
    include_directories(${CURL_INCLUDE_DIRS})
else()
    message(FATAL_ERROR "Could not find CURL library. Please install curl or check your conan configuration.")
endif()

# Find SQLite3
find_package(SQLite3 QUIET)
if(NOT SQLite3_FOUND)
    # Use pkg-config to find sqlite3 on Linux systems
    if(UNIX AND NOT APPLE)
        find_package(PkgConfig QUIET)
        if(PKG_CONFIG_FOUND)
            pkg_check_modules(SQLITE3 QUIET sqlite3)
        endif()
    endif()
    
    # Manual search as a fallback
    if(NOT SQLITE3_FOUND)
        find_path(SQLITE3_INCLUDE_DIR sqlite3.h)
        find_library(SQLITE3_LIBRARY sqlite3)
    endif()
    
    if(SQLITE3_INCLUDE_DIR AND (SQLITE3_LIBRARY OR SQLITE3_LIBRARIES))
        set(SQLITE3_FOUND TRUE)
        # Use SQLITE3_LIBRARIES from pkg-config if available, otherwise use our found library
        if(NOT SQLITE3_LIBRARIES)
            set(SQLITE3_LIBRARIES ${SQLITE3_LIBRARY})
        endif()
        
        message(STATUS "Found SQLite3: ${SQLITE3_LIBRARIES}")
        
        # Create imported target if it doesn't exist yet
        if(NOT TARGET SQLite3::SQLite3)
            add_library(SQLite3::SQLite3 UNKNOWN IMPORTED)
            set_target_properties(SQLite3::SQLite3 PROPERTIES
                IMPORTED_LOCATION "${SQLITE3_LIBRARIES}"
                INTERFACE_INCLUDE_DIRECTORIES "${SQLITE3_INCLUDE_DIR}")
        endif()
    else()
        message(FATAL_ERROR "Could not find SQLite3 library. Please install libsqlite3-dev.")
    endif()
endif()

# Find fmt library
find_package(fmt QUIET)
if(NOT fmt_FOUND)
    # Create custom target if system fmt not found
    find_path(FMT_INCLUDE_DIR fmt/core.h)
    find_library(FMT_LIBRARY fmt)
    
    if(FMT_INCLUDE_DIR AND FMT_LIBRARY)
        message(STATUS "Found fmt: ${FMT_LIBRARY}")
        if(NOT TARGET fmt::fmt)
            add_library(fmt UNKNOWN IMPORTED)
            set_target_properties(fmt PROPERTIES
                IMPORTED_LOCATION "${FMT_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${FMT_INCLUDE_DIR}")
        endif()
    else()
        message(FATAL_ERROR "Could not find fmt library")
    endif()
endif()

# Add include directories for external libraries
include_directories(
    ${OPENSSL_INCLUDE_DIR}
    ${CURL_INCLUDE_DIR}
    ${SQLITE3_INCLUDE_DIR}
    ${FMT_INCLUDE_DIR}
    vendor/
    vendor/httplib
)

# Add subdirectories
add_subdirectory(src)