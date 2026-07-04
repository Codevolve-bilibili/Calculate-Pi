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
    src/sysinfo.cpp
)

if(WIN32)
    # Prevent <windows.h> from defining min/max macros that conflict with
    # std::min / std::max in C++ code.
    target_compile_definitions(calculate_pi PRIVATE NOMINMAX)

    target_sources(calculate_pi PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/res/Calculate-Pi.rc
    )

    # Tell the resource compiler that the .rc file is UTF-8 so that
    # Chinese characters in version strings are preserved.
    if(CMAKE_RC_COMPILER MATCHES "windres")
        # MSYS2 windres needs an explicit preprocessor and -DRC_INVOKED so that
        # windows.h skips inline assembly/intrinsics that windres cannot parse.
        set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -c 65001 --use-temp-file --preprocessor clang --preprocessor-arg=-E --preprocessor-arg=-xc --preprocessor-arg=-DRC_INVOKED")
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

# Highest optimization level
if(MSVC)
    target_compile_options(calculate_pi PRIVATE /Ox /W4 /WX-)
else()
    target_compile_options(calculate_pi PRIVATE -O3 -Wall -Wextra -Wpedantic)
endif()
