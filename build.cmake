cmake_minimum_required(VERSION 3.20)
project(my_program CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Define the target executable and source files
add_executable(my_program
    main.cpp
    Unit/unit_impl.cpp
    Move/move_helper.cpp
    State/state_impl.cpp
)

# Specify include directories
target_include_directories(my_program
    PRIVATE
        Move
        Unit
        State
)

# Set output directory so the binary lands in build/ (or your build tree)
set_target_properties(my_program PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)