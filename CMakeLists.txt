cmake_minimum_required(VERSION 3.20)
project(my_program CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTS "Build the unit tests (downloads GoogleTest)" ON)
option(ENABLE_SANITIZERS "Build with AddressSanitizer + UBSan (not MSVC)" OFF)

# ---------------------------------------------------------------- core library
# Everything except main.cpp, so the program and the tests share one build.
add_library(core STATIC
    Unit/unit_impl.cpp
    Move/move_helper.cpp
    State/state_impl.cpp
)

target_include_directories(core
    PUBLIC
        Move
        Unit
        State
)

# Sanitizers must be on the library too, since the bugs live in its code.
# PUBLIC so they propagate to my_program and the tests.
if(ENABLE_SANITIZERS AND NOT MSVC)
    target_compile_options(core PUBLIC -fsanitize=address,undefined -g)
    target_link_options(core PUBLIC -fsanitize=address,undefined)
endif()

# ------------------------------------------------------------------ executable
add_executable(my_program main.cpp)
target_link_libraries(my_program PRIVATE core)

# Set output directory so the binary lands in build/ (or your build tree)
set_target_properties(my_program PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# ----------------------------------------------------------------------- tests
if(BUILD_TESTS)
    enable_testing()

    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE) # needed on MSVC
    FetchContent_MakeAvailable(googletest)

    add_executable(conflict_tests tests/conflict_resolve_test.cpp)
    target_link_libraries(conflict_tests PRIVATE core GTest::gtest_main)
    set_target_properties(conflict_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )

    include(GoogleTest)
    gtest_discover_tests(conflict_tests)
endif()
