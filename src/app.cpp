// Copyright (c) 2026 Calculate Pi project
// SPDX-License-Identifier: MIT

#include "cpi/app.hpp"

#include "cpi/chudnovsky.hpp"
#include "cpi/concurrency.hpp"
#include "cpi/io.hpp"

#include <filesystem>
#include <iostream>
#include <limits>
#include <sstream>

namespace cpi {

namespace {

std::string app_error_message(AppError error) {
    switch (error) {
        case AppError::CliError:
            return "Command-line error";
        case AppError::ComputeError:
            return "Computation error";
        case AppError::IOError:
            return "I/O error";
        case AppError::UnknownError:
            return "Unknown error";
    }
    return "Unknown application error";
}

} // namespace

Application::Application() = default;

int Application::run(int argc, const char* argv[]) {
    auto cli_result = parse_cli(argc, argv);
    if (!cli_result) {
        print_error(to_string(cli_result.error()), std::cerr);
        return 1;
    }

    switch (cli_result->mode) {
        case RunMode::Help:
            print_help(std::cout);
            return 0;
        case RunMode::Interactive:
        case RunMode::Compute: {
            auto options = gather_options(*cli_result);
            if (!options) {
                print_error(app_error_message(options.error()), std::cerr);
                return 1;
            }
            return run_compute(*options);
        }
    }

    print_error("Unexpected run mode", std::cerr);
    return 1;
}

void Application::print_help(std::ostream& os) {
    cpi::print_help(os);
}

void Application::print_error(std::string_view message, std::ostream& os) {
    os << "Error: " << message << "\n";
}

expected<ComputeOptions, AppError> Application::gather_options(
    const CliResult& cli) {
    if (cli.mode == RunMode::Compute) {
        if (!cli.compute_options.has_value()) {
            return AppError::CliError;
        }
        return *cli.compute_options;
    }

    if (cli.mode == RunMode::Interactive) {
        auto opts = read_interactive_options();
        if (!opts) {
            print_error(to_string(opts.error()), std::cerr);
            return AppError::IOError;
        }
        return *opts;
    }

    return AppError::CliError;
}

int Application::run_compute(const ComputeOptions& options) {
    // Determine output digits from terms.
    uint64_t output_digits = digits_for_terms(options.terms) - 1;
    if (output_digits == 0) {
        output_digits = 1;
    }

    ChudnovskyOptions chud_opts{options.terms, output_digits, 5};

    auto pool = std::make_shared<ThreadPool>();
    ChudnovskyCalculator calculator(chud_opts, pool);

    auto result = calculator.compute();
    if (!result) {
        print_error(to_string(result.error()), std::cerr);
        return 1;
    }

    std::string formatted = format_pi(result->scaled_pi, output_digits,
                                      result->guard_digits);

    ConsoleOutput console_output;
    console_output.pi_formatted = formatted;
    console_output.terms = options.terms;
    console_output.decimal_digits = output_digits;
    console_output.elapsed_ms = result->elapsed_ms;

    print_to_console(console_output, options.show_stats);

    if (options.output_path.has_value()) {
        auto write_res = write_text_file(*options.output_path, formatted, true);
        if (!write_res) {
            print_error(to_string(write_res.error()), std::cerr);
            return 1;
        }
    }

    return 0;
}

} // namespace cpi
