// Copyright (c) 2026 Calculate Pi project
// SPDX-License-Identifier: MIT

#include "cpi/app.hpp"

#include <exception>
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        cpi::Application app;
        return app.run(argc, const_cast<const char**>(argv));
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << '\n';
        return 1;
    }
}
