cmake_minimum_required(VERSION 3.10)
project(BookstoreManagementSystem)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable optimizations
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")

# Include directories
include_directories(include)

add_executable(code
    src/main.cpp
    src/command_parser.cpp
    src/account_system.cpp
    src/book_system.cpp
    src/log_system.cpp
    src/file_manager.cpp
)
