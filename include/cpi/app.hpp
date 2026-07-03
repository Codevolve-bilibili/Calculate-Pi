// Copyright (c) 2026 Calculate Pi project
// SPDX-License-Identifier: MIT

#pragma once

#include "cpi/cli.hpp"
#include "cpi/expected.hpp"

#include <ostream>
#include <string_view>

namespace cpi {

enum class AppError {
    CliError,
    ComputeError,
    IOError,
    MemoryGuard,
    UnknownError,
};

class Application {
public:
    Application();

    int run(int argc, const char* argv[]);

private:
    void print_help(std::ostream& os);
    void print_error(std::string_view message, std::ostream& os);

    int run_compute(const ComputeOptions& options);
    expected<ComputeOptions, AppError> gather_options(const CliResult& cli);
};

} // namespace cpi
