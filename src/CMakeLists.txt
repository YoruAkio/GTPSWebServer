project(GTPSWebServer LANGUAGES CXX)

# Find all source and header files
file(GLOB_RECURSE SOURCES
    "*.cpp"
)

file(GLOB_RECURSE HEADERS
    "*.h"
)

# Create the executable
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
)

# Enable standard compiler warnings
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
    add_definitions(/EHsc)
    
    # Silence MSVC STL deprecation warnings for checked_array_iterator
    target_compile_definitions(${PROJECT_NAME} PRIVATE 
        _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
        _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS
    )
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
    add_definitions(-fexceptions)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()

# Linux-specific settings
if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PUBLIC pthread)
endif()

# Required compiler definition for OpenSSL support in httplib
target_compile_definitions(${PROJECT_NAME} PRIVATE
    CPPHTTPLIB_OPENSSL_SUPPORT
)

# Custom handling for SQLite3 on different platforms
if(WIN32)
    # Windows build using Conan packages
    target_link_libraries(${PROJECT_NAME} PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
        CONAN_PKG::sqlite3
        CONAN_PKG::fmt
        CONAN_PKG::libcurl
    )
elseif(UNIX)
    # Ubuntu/Linux direct linking approach
    find_library(SQLITE3_LIB sqlite3 REQUIRED)
    find_library(FMT_LIB fmt REQUIRED)
    
    target_link_libraries(${PROJECT_NAME} PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
        ${SQLITE3_LIB}
        ${FMT_LIB}
        ${CURL_LIBRARIES}
    )
else()
    # Standard system libraries (fallback)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
        SQLite3::SQLite3
        fmt
        ${CURL_LIBRARIES}
    )
endif()

# Set output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
