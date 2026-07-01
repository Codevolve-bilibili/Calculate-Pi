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

} // namespace

expected<CliResult, CliError> parse_cli(int argc, const char* argv[]) {
    if (argc == 1) {
        return CliResult{RunMode::Interactive, std::nullopt};
    }

    CliResult result{RunMode::Compute, ComputeOptions{}};
    ComputeOptions& opts = *result.compute_options;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);

        if (arg == "--help" || arg == "-h") {
            return CliResult{RunMode::Help, std::nullopt};
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

        return CliError::UnknownArgument;
    }

    // --terms is required for compute mode.
    if (opts.terms == 0) {
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
    }
    return "Unknown CLI error";
}

void print_help(std::ostream& os) {
    os << "Calculate Pi - Chudnovsky binary splitting\n"
       << "\n"
       << "Usage:\n"
       << "  CalculatePi [options]\n"
       << "  CalculatePi --terms <N> [-o <path>] [--stats]\n"
       << "\n"
       << "Options:\n"
       << "  -n, --terms <N>   Number of Chudnovsky series terms (required in CLI mode)\n"
       << "  -o, --output <path>  Write the result to the specified file\n"
       << "      --stats       Print additional statistics\n"
       << "  -h, --help        Show this help message\n"
       << "\n"
       << "If no arguments are given, the program enters interactive mode.\n";
}

} // namespace cpi
