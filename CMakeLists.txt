cmake_minimum_required(VERSION 3.24)
project(cortex-cli VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(USE_LLAMA_CPP "Build with llama.cpp support" OFF)

# Find threads
find_package(Threads REQUIRED)

# Include FetchContent for dependency management
include(FetchContent)

# ------------------------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------------------------

# CLI11 for command line parsing
FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG        v2.4.1
)
FetchContent_MakeAvailable(CLI11)

# FTXUI for Terminal UI
FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
    GIT_TAG        v5.0.0
)
FetchContent_MakeAvailable(ftxui)

# ZeroMQ (libzmq and cppzmq)
FetchContent_Declare(
    libzmq
    GIT_REPOSITORY https://github.com/zeromq/libzmq.git
    GIT_TAG        v4.3.5
)
# Disable building tests/tools for libzmq to speed up
set(ZMQ_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WITH_PERF_TOOL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(libzmq)

FetchContent_Declare(
    cppzmq
    GIT_REPOSITORY https://github.com/zeromq/cppzmq.git
    GIT_TAG        v4.10.0
)
FetchContent_MakeAvailable(cppzmq)

# JSON for modern C++
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.2
)
FetchContent_MakeAvailable(json)

# cpp-httplib for HTTP requests (Ollama & Remote APIs)
set(CPP_HTTPLIB_OPENSSL_SUPPORT ON CACHE BOOL "" FORCE)
find_package(OpenSSL REQUIRED)

FetchContent_Declare(
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG        v0.14.3
)
FetchContent_MakeAvailable(httplib)

FetchContent_MakeAvailable(httplib)

if(USE_LLAMA_CPP)
    # llama.cpp for local LLM runtime
    FetchContent_Declare(
        llama
        GIT_REPOSITORY https://github.com/ggerganov/llama.cpp.git
        GIT_TAG        master  # or a specific tag like b2000
    )
    # Disable llama.cpp examples and tests to keep build fast
    set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
    set(LLAMA_CURL ON CACHE BOOL "" FORCE) # Good for remote models eventually
    FetchContent_MakeAvailable(llama)
endif()

# SQLite3 for persistent memory
FetchContent_Declare(
    sqlite3_dep
    URL https://www.sqlite.org/2024/sqlite-amalgamation-3450100.zip
)
FetchContent_MakeAvailable(sqlite3_dep)

# Create a library target for SQLite
add_library(sqlite3 STATIC 
    ${sqlite3_dep_SOURCE_DIR}/sqlite3.c
)
target_include_directories(sqlite3 PUBLIC ${sqlite3_dep_SOURCE_DIR})
# Enable some standard SQLite features
target_compile_definitions(sqlite3 PRIVATE SQLITE_ENABLE_COLUMN_METADATA SQLITE_ENABLE_RTREE)

# ------------------------------------------------------------------------------
# Include Directories
# ------------------------------------------------------------------------------
include_directories(
    ${CMAKE_SOURCE_DIR}/include
)

# Executable Configuration
# ------------------------------------------------------------------------------
add_executable(cortex
    src/main.cpp
    src/core/agent_manager.cpp
    src/core/debate_manager.cpp
    src/core/workflow_manager.cpp
    src/core/stats_manager.cpp
    src/core/logger.cpp
    src/tui/dashboard.cpp
    src/messaging/zeromq_bus.cpp
    src/llm/ollama_client.cpp
    src/llm/remote_client.cpp
    src/memory/memory_manager.cpp
)

target_link_libraries(cortex PRIVATE 
    CLI11::CLI11 
    ftxui::screen 
    ftxui::dom 
    ftxui::component
    libzmq-static
    cppzmq
    nlohmann_json::nlohmann_json
    httplib::httplib
    OpenSSL::SSL
    OpenSSL::Crypto
    sqlite3
    Threads::Threads
)

if(USE_LLAMA_CPP)
    target_link_libraries(cortex PRIVATE llama)
    target_compile_definitions(cortex PRIVATE USE_LLAMA_CPP)
endif()
