cmake_minimum_required(VERSION 3.16)
project("Calculate Pi" CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(calculate_pi)

target_sources(calculate_pi PRIVATE
    main.cpp
    src/app.cpp
    src/bigint.cpp
    src/bigint_multiply.cpp
    src/chudnovsky.cpp
    src/cli.cpp
    src/concurrency.cpp
    src/io.cpp
)

target_include_directories(calculate_pi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(Threads REQUIRED)
target_link_libraries(calculate_pi PRIVATE Threads::Threads)

# Warning flags
if(MSVC)
    target_compile_options(calculate_pi PRIVATE /W4 /WX-)
else()
    target_compile_options(calculate_pi PRIVATE -Wall -Wextra -Wpedantic)
endif()
