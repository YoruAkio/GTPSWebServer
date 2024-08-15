project(GTPSWebServer LANGUAGES CXX)

file(GLOB INCLUDE_FILES
    *.h
    **/*.h
    **/**/*.h
    **/**/**/*.h
    **/**/**/**/*.h)

file(GLOB SOURCE_FILES
    *.cpp
    **/*.cpp
    **/**/*.cpp
    **/**/**/*.cpp
    **/**/**/**/*.cpp)

add_executable(${PROJECT_NAME}
    ${INCLUDE_FILES}
    ${SOURCE_FILES}
)

if (MSVC)
    add_definitions(/EHsc)
else ()
    add_definitions(-fexceptions)
endif ()

if (CONAN_EXPORTED)
    include(${CMAKE_CURRENT_BINARY_DIR}/conanbuildinfo.cmake)
    conan_basic_setup()
else ()
    message(STATUS "Conan info: ${settings}")
    message(STATUS "Conan info: ${settings}")
    message(STATUS "Conan info: ${settings}")
    message(STATUS "Conan info: ${settings}")
    include(${CMAKE_CURRENT_SOURCE_DIR}/conan.cmake)
    conan_cmake_configure(REQUIRES
            fmt/10.2.1
            spdlog/1.14.1
            openssl/3.2.2
            sqlite3/3.46.0
        GENERATORS CMakeDeps CMakeToolchain
        IMPORTS "bin, *.dll -> ./bin"
        OPTIONS
            fmt/*:shared=True
            spdlog/*:shared=True
            openssl/*:shared=True
            sqlite3/*:shared=True)

    conan_cmake_autodetect(settings)
    conan_cmake_install(PATH_OR_REFERENCE .
        BUILD missing
        REMOTE conancenter)
endif ()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_BINARY_DIR}")
set(DCMAKE_BUILD_TYPE "Debug")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_BINARY_DIR}" ${CMAKE_MODULE_PATH})
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_BINARY_DIR}" ${CMAKE_PREFIX_PATH})

find_package(fmt REQUIRED)
if (fmt_FOUND)
    message(STATUS "Found fmt: ${fmt_VERSION}, at ${fmt_INCLUDE_DIRS}, ${fmt_LIBRARIES}")
    target_link_libraries(${PROJECT_NAME} PUBLIC fmt::fmt)
    target_include_directories(${PROJECT_NAME} PUBLIC ${fmt_INCLUDE_DIRS})
else ()
    message(FATAL_ERROR "Could not find fmt")
endif ()

find_package(spdlog REQUIRED)
if (spdlog_FOUND)
    message(STATUS "Found spdlog: ${spdlog_VERSION}, at ${spdlog_INCLUDE_DIRS}, ${spdlog_LIBRARIES}")
    target_link_libraries(${PROJECT_NAME} PUBLIC spdlog::spdlog)
    target_include_directories(${PROJECT_NAME} PUBLIC ${fmt_INCLUDE_DIRS})
else ()
    message(FATAL_ERROR "Could not find spdlog")
endif ()

find_package(OpenSSL REQUIRED)
if (OpenSSL_FOUND)
    message(STATUS "Found OpenSSL: ${OpenSSL_VERSION}, at ${OpenSSL_INCLUDE_DIRS}, ${OpenSSL_LIBRARIES}")
    target_link_libraries(${PROJECT_NAME} PUBLIC OpenSSL::SSL OpenSSL::Crypto)
    target_include_directories(${PROJECT_NAME} PUBLIC ${OpenSSL_INCLUDE_DIRS})
else ()
    message(FATAL_ERROR "Could not find OpenSSL")
endif ()

find_package(SQLite3 REQUIRED)
if (SQLite3_FOUND)
    message(STATUS "Found SQLite3: ${SQLite3_VERSION}, at ${SQLite3_INCLUDE_DIRS}, ${SQLite3_LIBRARIES}")
    target_link_libraries(${PROJECT_NAME} PUBLIC ${SQLite3_LIBRARIES})
    target_include_directories(${PROJECT_NAME} PUBLIC ${SQLite3_INCLUDE_DIRS})
else ()
    message(FATAL_ERROR "Could not find SQLite3")
endif ()

set_target_properties(${PROJECT_NAME} PROPERTIES
    C_STANDARD 11
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../bin
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    CPPHTTPLIB_OPENSSL_SUPPORT
)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../vendor
    ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/httplib
)
