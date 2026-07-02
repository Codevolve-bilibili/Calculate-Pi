cmake_minimum_required(VERSION 3.16)
project("Calculate Pi" VERSION 1.0.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Keep the executable in the build root on all platforms and generators
# (e.g. avoids build/Release/calculate_pi.exe on Windows multi-config generators).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# -----------------------------------------------------------------------------
# Windows resource file version numbers
# -----------------------------------------------------------------------------
set(RC_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(RC_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(RC_VERSION_PATCH ${PROJECT_VERSION_PATCH})
if(PROJECT_VERSION_TWEAK)
    set(RC_VERSION_TWEAK ${PROJECT_VERSION_TWEAK})
else()
    set(RC_VERSION_TWEAK 0)
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/res/resource.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/res/resource.h
    @ONLY
)

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

if(WIN32)
    target_sources(calculate_pi PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/res/Calculate-Pi.rc
    )

    # Tell the resource compiler that the .rc file is UTF-8 so that
    # Chinese characters in version strings are preserved.
    if(CMAKE_RC_COMPILER MATCHES "windres")
        set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} --codepage=65001")
    else()
        set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} /c 65001")
    endif()
endif()

target_include_directories(calculate_pi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/res
)

find_package(Threads REQUIRED)
target_link_libraries(calculate_pi PRIVATE Threads::Threads)

# Warning flags
if(MSVC)
    target_compile_options(calculate_pi PRIVATE /W4 /WX-)
else()
    target_compile_options(calculate_pi PRIVATE -Wall -Wextra -Wpedantic)
endif()
