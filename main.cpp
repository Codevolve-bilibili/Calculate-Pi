// Copyright (c) 2026 Calculate Pi project
// SPDX-License-Identifier: MIT

#include "cpi/app.hpp"

#include <exception>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

// On Windows, set the console code page to UTF-8 while the program runs and
// restore the original code page on exit. This keeps Chinese/English output
// readable in cmd/PowerShell without requiring the user to run chcp.
struct ConsoleCodePageRestorer {
#ifdef _WIN32
    UINT original_output_cp_ = 0;
    UINT original_input_cp_ = 0;

    ConsoleCodePageRestorer() {
        original_output_cp_ = GetConsoleOutputCP();
        original_input_cp_ = GetConsoleCP();
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
    }

    ~ConsoleCodePageRestorer() {
        if (original_output_cp_ != 0) {
            SetConsoleOutputCP(original_output_cp_);
        }
        if (original_input_cp_ != 0) {
            SetConsoleCP(original_input_cp_);
        }
    }
#else
    ConsoleCodePageRestorer() = default;
    ~ConsoleCodePageRestorer() = default;
#endif
};

} // namespace

int main(int argc, char* argv[]) {
    ConsoleCodePageRestorer cp_restorer;

    try {
        cpi::Application app;
        return app.run(argc, const_cast<const char**>(argv));
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << '\n';
        return 1;
    }
}
