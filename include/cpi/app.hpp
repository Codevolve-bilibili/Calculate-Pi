// Copyright (c) 2026 Calculate Pi project
// SPDX-License-Identifier: MIT

#pragma once

#include "cpi/cli.hpp"
#include "cpi/expected.hpp"

#include <istream>
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
    void print_help(std::ostream& os, Language lang);
    void print_error(std::string_view message, std::ostream& os);

    Language resolve_language(const CliResult& cli, std::istream& is);

    int run_compute(const ComputeOptions& options, Language lang);
    int run_infinite(const ComputeOptions& options, Language lang);
    expected<ComputeOptions, AppError> gather_options(
        const CliResult& cli, Language lang);
};

} // namespace cpi
