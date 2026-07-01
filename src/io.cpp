// Copyright (c) 2026 Calculate Pi project
// SPDX-License-Identifier: MIT

#include "cpi/io.hpp"

#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

namespace cpi {

namespace {

// Round the scaled pi value to the desired number of output digits.
// scaled_pi = pi * 10^(output_digits + guard_digits).
// The result is pi * 10^output_digits as an integer.
BigInt round_scaled_pi(const BigInt& scaled_pi, uint64_t guard_digits) {
    if (guard_digits == 0) return scaled_pi;

    BigInt ten(10);
    BigInt divisor = BigInt::pow(ten, guard_digits);
    BigInt half_divisor = (divisor / BigInt(2)).value();
    // scaled_pi is positive for pi, but keep the sign handling general.
    BigInt rounded_up = (scaled_pi.abs() + half_divisor);
    BigInt rounded = (rounded_up / divisor).value();
    if (scaled_pi.is_negative()) rounded = rounded.negate();
    return rounded;
}

} // namespace

std::string format_pi(const BigInt& scaled_pi, uint64_t output_digits,
                      uint64_t guard_digits) {
    BigInt rounded = round_scaled_pi(scaled_pi, guard_digits);
    std::string s = rounded.to_string();

    // scaled_pi is positive and roughly pi * 10^(D+G), so the integer
    // representation has D+G+1 digits (the leading 3 plus D+G decimals).
    // After rounding to D digits, the integer should have D+1 digits.
    if (s.size() < output_digits + 1) {
        s = std::string(output_digits + 1 - s.size(), '0') + s;
    } else if (s.size() > output_digits + 1) {
        // Rounding caused an extra digit (e.g. 3.999... rounded to 4.000...).
        // Keep output_digits digits after the decimal point by dropping the
        // least significant digit.
        s.pop_back();
    }

    // Insert decimal point after the first digit.
    if (s.size() > 1) {
        s.insert(1, 1, '.');
    }
    return s;
}

std::string format_stats(const ConsoleOutput& output) {
    std::ostringstream oss;
    oss << "Terms: " << output.terms << "\n"
        << "Digits: " << output.decimal_digits << "\n"
        << "Time: " << output.elapsed_ms << " ms\n";
    return oss.str();
}

expected<void, IOError> write_text_file(const std::filesystem::path& path,
                                           std::string_view content,
                                           bool overwrite) {
    try {
        if (path.string().size() > std::numeric_limits<size_t>::max() / 2) {
            return IOError::PathTooLong;
        }

        std::ios_base::openmode mode = std::ios::out | std::ios::trunc;
        if (!overwrite) {
            mode = std::ios::out | std::ios::app;
        }

        std::ofstream file(path, mode);
        if (!file.is_open()) {
            return IOError::FileNotWritable;
        }

        file.write(content.data(), static_cast<std::streamsize>(content.size()));
        if (!file.good()) {
            return IOError::FileNotWritable;
        }

        return {};
    } catch (const std::filesystem::filesystem_error&) {
        return IOError::PathInvalid;
    } catch (const std::exception&) {
        return IOError::FileNotWritable;
    }
}

expected<ComputeOptions, IOError> read_interactive_options(std::istream& is) {
    std::cout << "Enter the number of terms (N): ";
    std::string line;
    if (!std::getline(is, line)) {
        return IOError::InteractiveInputFailed;
    }

    // Trim whitespace.
    size_t start = 0;
    while (start < line.size() && std::isspace(static_cast<unsigned char>(line[start]))) {
        ++start;
    }
    size_t end = line.size();
    while (end > start && std::isspace(static_cast<unsigned char>(line[end - 1]))) {
        --end;
    }
    std::string terms_str = line.substr(start, end - start);

    if (terms_str.empty()) {
        return IOError::InteractiveInputFailed;
    }

    for (char c : terms_str) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            return IOError::InteractiveInputFailed;
        }
    }

    if (terms_str.size() > 1 && terms_str[0] == '0') {
        return IOError::InteractiveInputFailed;
    }

    uint64_t terms;
    try {
        size_t pos = 0;
        unsigned long long value = std::stoull(terms_str, &pos);
        if (pos != terms_str.size() || value == 0 ||
            value > std::numeric_limits<uint64_t>::max()) {
            return IOError::InteractiveInputFailed;
        }
        terms = static_cast<uint64_t>(value);
    } catch (const std::exception&) {
        return IOError::InteractiveInputFailed;
    }

    ComputeOptions options;
    options.terms = terms;

    std::cout << "Save to file? (y/n): ";
    std::string save_answer;
    if (!std::getline(is, save_answer)) {
        return IOError::InteractiveInputFailed;
    }
    if (!save_answer.empty() &&
        (save_answer[0] == 'y' || save_answer[0] == 'Y')) {
        std::cout << "Enter output file path: ";
        std::string path_str;
        if (!std::getline(is, path_str)) {
            return IOError::InteractiveInputFailed;
        }
        // Trim.
        size_t pstart = 0;
        while (pstart < path_str.size() &&
               std::isspace(static_cast<unsigned char>(path_str[pstart]))) {
            ++pstart;
        }
        size_t pend = path_str.size();
        while (pend > pstart &&
               std::isspace(static_cast<unsigned char>(path_str[pend - 1]))) {
            --pend;
        }
        if (pstart < pend) {
            try {
                options.output_path = std::filesystem::path(
                    path_str.substr(pstart, pend - pstart));
            } catch (const std::exception&) {
                return IOError::InteractiveInputFailed;
            }
        }
    }

    std::cout << "Show statistics? (y/n): ";
    std::string stats_answer;
    if (!std::getline(is, stats_answer)) {
        return IOError::InteractiveInputFailed;
    }
    options.show_stats =
        !stats_answer.empty() && (stats_answer[0] == 'y' || stats_answer[0] == 'Y');

    return options;
}

void print_to_console(const ConsoleOutput& output, bool show_stats,
                      std::ostream& os) {
    os << output.pi_formatted;
    if (show_stats) {
        os << "\n" << format_stats(output);
    }
    os << "\n";
}

std::string to_string(IOError error) {
    switch (error) {
        case IOError::FileNotWritable:
            return "Unable to write to the specified file";
        case IOError::PathInvalid:
            return "Invalid file path";
        case IOError::PathTooLong:
            return "File path is too long";
        case IOError::InteractiveInputFailed:
            return "Interactive input failed or contained invalid data";
    }
    return "Unknown I/O error";
}

} // namespace cpi
