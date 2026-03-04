cmake_minimum_required(VERSION 3.16)
project(sta_prototype VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(STA_BUILD_TESTS "Build the STA regression test binary" ON)

add_library(sta_core
    src/engine.cpp
)

target_include_directories(sta_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(sta_core
    PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

add_executable(sta
    src/main.cpp
)

target_link_libraries(sta PRIVATE sta_core)

if(STA_BUILD_TESTS)
    enable_testing()

    add_executable(sta_tests
        tests/test_sta.cpp
    )
    target_link_libraries(sta_tests PRIVATE sta_core)

    add_test(NAME sta_tests COMMAND sta_tests)
endif()
