cmake_minimum_required(VERSION 3.24)

project(swiss_table
    VERSION 0.1.0
    DESCRIPTION "Swiss-table hash map with benchmarks against baselines"
    LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Global settings
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)            # -std=c++20, not -std=gnu++20
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)    # compile_commands.json for clangd / IntelliSense

# Default to Release for single-config generators (Ninja, MinGW Makefiles).
# Benchmarks from a Debug build are meaningless, so this guards against forgetting.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(SWISS_BUILD_TESTS      "Build unit tests"                          ON)
option(SWISS_BUILD_BENCHMARKS "Build benchmarks"                          ON)
option(SWISS_SANITIZE         "Build with sanitisers (tests only)"        OFF)
option(SWISS_NATIVE           "Compile with -march=native"                OFF)

# ---------------------------------------------------------------------------
# The library: header-only, so an INTERFACE target
# ---------------------------------------------------------------------------
# An INTERFACE library compiles nothing itself. It carries include paths and
# flags to whatever links against it.

add_library(swiss_table INTERFACE)
add_library(swiss::swiss_table ALIAS swiss_table)
target_include_directories(swiss_table INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(swiss_table INTERFACE cxx_std_20)

if(SWISS_NATIVE)
    target_compile_options(swiss_table INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-march=native>)
endif()

# Warnings live in their own target so they apply to this project's code only,
# not to GoogleTest or Google Benchmark.
add_library(swiss_warnings INTERFACE)
target_compile_options(swiss_warnings INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->)

# ---------------------------------------------------------------------------
# Dependencies, downloaded at configure time
# ---------------------------------------------------------------------------

include(FetchContent)

if(SWISS_BUILD_TESTS)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.15.2)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)   # MSVC only, harmless elsewhere
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

if(SWISS_BUILD_BENCHMARKS)
    FetchContent_Declare(benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG        v1.9.1)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    # Benchmark builds itself with -Werror. Newer GCC adds warnings its source
    # does not yet silence (GCC 16: -Wswitch in sysinfo.cc), which breaks the build.
    set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(benchmark)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

if(SWISS_BUILD_TESTS)
    enable_testing()

    add_executable(swiss_tests
        tests/test_hash.cpp
        tests/test_hash_candidates.cpp
        tests/test_keys.cpp)
    target_link_libraries(swiss_tests PRIVATE
        swiss::swiss_table swiss_warnings GTest::gtest_main)
    # The key generators live with the benchmarks but are tested here.
    target_include_directories(swiss_tests PRIVATE bench)

    if(SWISS_SANITIZE)
        if(MINGW)
            # MinGW GCC ships no ASan or UBSan runtime. UBSan in trap mode needs
            # no runtime, so it still works. It aborts instead of printing a report.
            message(STATUS "MinGW: AddressSanitizer unavailable, using UBSan trap mode only")
            set(SWISS_SAN_FLAGS -fsanitize=undefined -fsanitize-undefined-trap-on-error)
        else()
            set(SWISS_SAN_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer)
        endif()
        target_compile_options(swiss_tests PRIVATE ${SWISS_SAN_FLAGS})
        target_link_options(swiss_tests PRIVATE ${SWISS_SAN_FLAGS})
    endif()

    include(GoogleTest)
    gtest_discover_tests(swiss_tests)
endif()

# ---------------------------------------------------------------------------
# Benchmarks
# ---------------------------------------------------------------------------

if(SWISS_BUILD_BENCHMARKS)
    add_executable(swiss_bench
        bench/bench_hash.cpp)
    target_link_libraries(swiss_bench PRIVATE
        swiss::swiss_table swiss_warnings benchmark::benchmark_main)

    # Hash-level quality analysis. Deterministic counts, not timings, so it
    # needs no benchmark library.
    add_executable(swiss_hash_quality
        bench/hash_quality.cpp)
    target_link_libraries(swiss_hash_quality PRIVATE
        swiss::swiss_table swiss_warnings)
endif()
