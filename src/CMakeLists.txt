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

# Link against system libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    SQLite3::SQLite3
    fmt
)

# Set output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
