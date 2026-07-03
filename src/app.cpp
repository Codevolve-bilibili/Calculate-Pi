// Copyright (c) 2026 Calculate Pi project
// SPDX-License-Identifier: MIT

#include "cpi/app.hpp"

#include "cpi/chudnovsky.hpp"
#include "cpi/concurrency.hpp"
#include "cpi/io.hpp"
#include "cpi/sysinfo.hpp"

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <thread>

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
        case AppError::MemoryGuard:
            return "Memory limit exceeded";
        case AppError::UnknownError:
            return "Unknown error";
    }
    return "Unknown application error";
}

// Conservative memory estimator: ~50 bytes per scaled digit.
// Accounts for BigInt limbs, FFT buffers, recursion overhead, and temporaries.
uint64_t estimate_memory_bytes(uint64_t scale_digits) {
    constexpr uint64_t kBytesPerDigit = 50;
    if (scale_digits > std::numeric_limits<uint64_t>::max() / kBytesPerDigit) {
        return std::numeric_limits<uint64_t>::max();
    }
    return scale_digits * kBytesPerDigit;
}

std::string format_memory_mib(uint64_t bytes) {
    constexpr uint64_t kMiB = 1024 * 1024;
    if (bytes < kMiB) {
        return std::to_string(bytes) + " bytes";
    }
    double mib = static_cast<double>(bytes) / static_cast<double>(kMiB);
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << mib << " MiB";
    return oss.str();
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
    // Easter egg: pi has no last digit.
    if (options.last_digit_easter_egg) {
        std::cout << "π is irrational — it has no last digit.\n"
                  << "Use --terms <N> to compute the first N digits, or --help for more options.\n";
        return 0;
    }

    // Determine output digits from terms.
    uint64_t output_digits = digits_for_terms(options.terms) - 1;
    if (output_digits == 0) {
        output_digits = 1;
    }

    const uint64_t guard_digits = 5;
    const uint64_t scale_digits = output_digits + guard_digits;

    // Memory safety guard.
    if (!options.force) {
        uint64_t estimated_bytes = estimate_memory_bytes(scale_digits);

        uint64_t allowed_bytes = std::numeric_limits<uint64_t>::max();
        if (options.max_memory_mb.has_value()) {
            allowed_bytes = *options.max_memory_mb * 1024 * 1024;
        } else {
            auto total_mem = total_physical_memory_bytes();
            if (total_mem.has_value()) {
                // Default guard: allow up to 50% of physical memory.
                allowed_bytes = *total_mem / 2;
            } else {
                // Conservative fallback when system memory is unknown.
                constexpr uint64_t kFallbackBytes = 1024ULL * 1024 * 1024;
                allowed_bytes = kFallbackBytes;
            }
        }

        if (estimated_bytes > allowed_bytes) {
            std::ostringstream oss;
            oss << "Requested terms would require approximately "
               << format_memory_mib(estimated_bytes)
               << " of memory.\n"
               << "Use --terms <N>, --max-memory-mb <M>, or --force to proceed anyway.\n";
            print_error(oss.str(), std::cerr);
            return static_cast<int>(AppError::MemoryGuard);
        }
    }

    ChudnovskyOptions chud_opts{options.terms, output_digits, guard_digits};

    // Determine thread count.
    size_t num_threads = options.thread_count;
    if (num_threads == 0) {
        num_threads = std::max(
            size_t{1}, static_cast<size_t>(std::thread::hardware_concurrency()));
    }
    if (options.eco_mode && num_threads > 1) {
        num_threads = std::max(size_t{1}, num_threads / 2);
    }

    if (options.eco_mode) {
        set_eco_priority();
    }

    auto pool = std::make_shared<ThreadPool>(num_threads);
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

    // Warn when a large result would be printed to the console without --output.
    constexpr uint64_t kLargeOutputThreshold = 1'000'000;
    if (!options.quiet && !options.output_path.has_value() &&
        output_digits > kLargeOutputThreshold) {
        std::cerr << "Warning: Large result; use --output <file> to save instead of printing to console.\n";
    }

    if (!options.quiet) {
        print_to_console(console_output, options.show_stats);
    } else if (options.show_stats) {
        std::cout << "Computed " << output_digits << " digits in "
                  << result->elapsed_ms << " ms.\n";
    }

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
