// Copyright (c) 2026 Calculate Pi project
// SPDX-License-Identifier: MIT

#include "cpi/cli.hpp"

#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace cpi {

namespace {

bool is_decimal_uint64(std::string_view s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
    }
    return true;
}

} // namespace

expected<uint64_t, CliError> parse_terms(std::string_view s) {
    if (!is_decimal_uint64(s)) {
        return CliError::InvalidTermsFormat;
    }
    // Reject leading zeros except for "0" itself.
    if (s.size() > 1 && s[0] == '0') {
        return CliError::InvalidTermsFormat;
    }
    try {
        size_t pos = 0;
        unsigned long long value = std::stoull(std::string(s), &pos);
        if (pos != s.size() || value > std::numeric_limits<uint64_t>::max()) {
            return CliError::TermsOutOfRange;
        }
        if (value == 0) {
            return CliError::TermsOutOfRange;
        }
        return static_cast<uint64_t>(value);
    } catch (const std::exception&) {
        return CliError::TermsOutOfRange;
    }
}

expected<std::filesystem::path, CliError> parse_output_path(
    std::string_view s) {
    try {
        return std::filesystem::path(s);
    } catch (const std::exception&) {
        return CliError::InvalidOutputPath;
    }
}

expected<uint64_t, CliError> parse_memory_mb(std::string_view s) {
    if (!is_decimal_uint64(s)) {
        return CliError::InvalidMemoryLimit;
    }
    if (s.size() > 1 && s[0] == '0') {
        return CliError::InvalidMemoryLimit;
    }
    try {
        size_t pos = 0;
        unsigned long long value = std::stoull(std::string(s), &pos);
        if (pos != s.size() || value > std::numeric_limits<uint64_t>::max()) {
            return CliError::InvalidMemoryLimit;
        }
        if (value == 0) {
            return CliError::InvalidMemoryLimit;
        }
        return static_cast<uint64_t>(value);
    } catch (const std::exception&) {
        return CliError::InvalidMemoryLimit;
    }
}

expected<size_t, CliError> parse_thread_count(std::string_view s) {
    if (!is_decimal_uint64(s)) {
        return CliError::InvalidThreadCount;
    }
    if (s.size() > 1 && s[0] == '0') {
        return CliError::InvalidThreadCount;
    }
    try {
        size_t pos = 0;
        unsigned long long value = std::stoull(std::string(s), &pos);
        if (pos != s.size() || value > std::numeric_limits<size_t>::max()) {
            return CliError::InvalidThreadCount;
        }
        // 0 is allowed and means "use hardware concurrency".
        return static_cast<size_t>(value);
    } catch (const std::exception&) {
        return CliError::InvalidThreadCount;
    }
}

expected<CliResult, CliError> parse_cli(int argc, const char* argv[]) {
    if (argc == 1) {
        return CliResult{RunMode::Interactive, ComputeOptions{}};
    }

    CliResult result{RunMode::Compute, ComputeOptions{}};
    ComputeOptions& opts = *result.compute_options;
    bool interactive_requested = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);

        if (arg == "--help" || arg == "-h") {
            return CliResult{RunMode::Help, std::nullopt};
        }

        if (arg == "--interactive" || arg == "-i") {
            interactive_requested = true;
            continue;
        }

        if (arg == "--terms" || arg == "-n") {
            if (i + 1 >= argc) {
                return CliError::MissingArgumentValue;
            }
            auto terms = parse_terms(argv[++i]);
            if (!terms) return terms.error();
            opts.terms = *terms;
            continue;
        }

        if (arg == "--output" || arg == "-o") {
            if (i + 1 >= argc) {
                return CliError::MissingArgumentValue;
            }
            auto path = parse_output_path(argv[++i]);
            if (!path) return path.error();
            if (opts.output_path.has_value()) {
                return CliError::ConflictingOptions;
            }
            opts.output_path = *path;
            continue;
        }

        if (arg == "--stats") {
            opts.show_stats = true;
            continue;
        }

        if (arg == "--quiet" || arg == "-q") {
            opts.quiet = true;
            continue;
        }

        if (arg == "--force" || arg == "-f") {
            opts.force = true;
            continue;
        }

        if (arg == "--eco" || arg == "-e") {
            opts.eco_mode = true;
            continue;
        }

        if (arg == "--threads" || arg == "-t") {
            if (i + 1 >= argc) {
                return CliError::MissingArgumentValue;
            }
            auto count = parse_thread_count(argv[++i]);
            if (!count) return count.error();
            opts.thread_count = *count;
            continue;
        }

        if (arg == "--max-memory-mb" || arg == "-m") {
            if (i + 1 >= argc) {
                return CliError::MissingArgumentValue;
            }
            auto mb = parse_memory_mb(argv[++i]);
            if (!mb) return mb.error();
            if (opts.max_memory_mb.has_value()) {
                return CliError::ConflictingOptions;
            }
            opts.max_memory_mb = *mb;
            continue;
        }

        if (arg == "--last-digit") {
            opts.last_digit_easter_egg = true;
            continue;
        }

        return CliError::UnknownArgument;
    }

    if (interactive_requested) {
        result.mode = RunMode::Interactive;
    }

    // --terms is required for compute mode unless --last-digit is used.
    if (result.mode == RunMode::Compute && opts.terms == 0 &&
        !opts.last_digit_easter_egg) {
        return CliError::MissingArgumentValue;
    }

    return result;
}

std::string to_string(CliError error) {
    switch (error) {
        case CliError::UnknownArgument:
            return "Unknown command-line argument";
        case CliError::MissingArgumentValue:
            return "Missing value for a command-line option";
        case CliError::InvalidTermsFormat:
            return "Invalid terms: must be a positive decimal integer";
        case CliError::TermsOutOfRange:
            return "Terms out of range: must be a positive integer";
        case CliError::InvalidOutputPath:
            return "Invalid output file path";
        case CliError::ConflictingOptions:
            return "Conflicting command-line options";
        case CliError::InvalidThreadCount:
            return "Invalid thread count: must be a decimal integer (0 = hardware concurrency)";
        case CliError::InvalidMemoryLimit:
            return "Invalid memory limit: must be a positive decimal integer in MB";
    }
    return "Unknown CLI error";
}

void print_help(std::ostream& os) {
    os << "Calculate Pi - Chudnovsky binary splitting\n"
       << "\n"
       << "Usage:\n"
       << "  CalculatePi [options]\n"
       << "  CalculatePi --terms <N> [-o <path>] [--stats]\n"
       << "  CalculatePi --interactive [options]\n"
       << "\n"
       << "Options:\n"
       << "  -n, --terms <N>      Number of Chudnovsky series terms (required in CLI mode)\n"
       << "  -o, --output <path>  Write the result to the specified file\n"
       << "      --stats          Print additional statistics\n"
       << "  -q, --quiet          Do not print the full pi string to the console\n"
       << "  -t, --threads <N>    Number of worker threads (0 = hardware concurrency)\n"
       << "  -e, --eco            Use fewer threads to reduce CPU load\n"
       << "  -m, --max-memory-mb  Maximum memory the computation may use in MB\n"
       << "  -f, --force          Skip the memory safety guard\n"
       << "      --last-digit     Explain that pi has no last digit\n"
       << "  -i, --interactive    Enter interactive mode (use other flags as defaults)\n"
       << "  -h, --help           Show this help message\n"
       << "\n"
       << "If no arguments are given, the program enters interactive mode.\n";
}

} // namespace cpi
