// Copyright (c) 2026 Calculate Pi project
// SPDX-License-Identifier: MIT

#pragma once

#include "cpi/bigint.hpp"
#include "cpi/cli.hpp"
#include "cpi/expected.hpp"

#include <filesystem>
#include <istream>
#include <ostream>
#include <cstdint>
#include <string_view>

namespace cpi {

struct ConsoleOutput {
    std::string pi_formatted;
    uint64_t terms;
    uint64_t decimal_digits;
    uint64_t elapsed_ms;
};

enum class IOError {
    FileNotWritable,
    PathInvalid,
    PathTooLong,
    InteractiveInputFailed,
};

// Format a scaled pi value into a decimal string "3.xxx...".
// scaled_pi = pi * 10^(output_digits + guard_digits).
[[nodiscard]] std::string format_pi(const BigInt& scaled_pi,
                                    uint64_t output_digits,
                                    uint64_t guard_digits);

[[nodiscard]] std::string format_stats(const ConsoleOutput& output);

[[nodiscard]] expected<void, IOError> write_text_file(
    const std::filesystem::path& path,
    std::string_view content,
    bool overwrite = true);

[[nodiscard]] expected<ComputeOptions, IOError> read_interactive_options(
    std::istream& is = std::cin);

void print_to_console(const ConsoleOutput& output, bool show_stats,
                      std::ostream& os = std::cout);

[[nodiscard]] std::string to_string(IOError error);

} // namespace cpi
