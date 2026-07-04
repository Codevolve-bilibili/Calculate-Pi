// Copyright (c) 2026 Calculate Pi project
// SPDX-License-Identifier: MIT

#include "cpi/io.hpp"

#include <cctype>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>

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

std::string trim_string(const std::string& s) {
    size_t start = 0;
    while (start < s.size() &&
           std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    size_t end = s.size();
    while (end > start &&
           std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return s.substr(start, end - start);
}

bool answer_is_yes(const std::string& answer, Language lang) {
    if (answer.empty()) return false;
    if (lang == Language::Chinese) {
        return answer == "y" || answer == "Y" ||
               answer == "yes" || answer == "YES" ||
               answer == "是" || answer == "1";
    }
    return answer == "y" || answer == "Y" ||
           answer == "yes" || answer == "YES";
}

expected<bool, IOError> read_yes_no(const std::string& prompt,
                                      Language lang, std::istream& is) {
    std::cout << prompt;
    std::string line;
    if (!std::getline(is, line)) {
        return IOError::InteractiveInputFailed;
    }
    const std::string answer = trim_string(line);
    return answer_is_yes(answer, lang);
}

} // namespace

namespace detail {

BigInt round_scaled_pi(const BigInt& scaled_pi, uint64_t guard_digits) {
    return ::cpi::round_scaled_pi(scaled_pi, guard_digits);
}

} // namespace detail

// -----------------------------------------------------------------------------
// Built-in sinks
// -----------------------------------------------------------------------------

OstreamSink::OstreamSink(std::ostream& os) : os_(os) {}

void OstreamSink::write(std::string_view data) {
    if (!os_.good()) return;
    os_.write(data.data(), static_cast<std::streamsize>(data.size()));
    os_.flush();
}

bool OstreamSink::good() const {
    return os_.good();
}

FileSink::FileSink(const std::filesystem::path& path) {
    try {
        if (path.string().size() > std::numeric_limits<size_t>::max() / 2) {
            error_ = IOError::PathTooLong;
            return;
        }
        file_.open(path, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!file_.is_open()) {
            error_ = IOError::FileNotWritable;
        }
    } catch (const std::filesystem::filesystem_error&) {
        error_ = IOError::PathInvalid;
    } catch (const std::exception&) {
        error_ = IOError::FileNotWritable;
    }
}

void FileSink::write(std::string_view data) {
    if (!file_.is_open() || !file_.good()) return;
    file_.write(data.data(), static_cast<std::streamsize>(data.size()));
    file_.flush();
    if (!file_.good() && error_ == IOError::FileNotWritable) {
        // Keep the first recorded error.
    }
}

bool FileSink::good() const {
    return file_.is_open() && file_.good();
}

IOError FileSink::error() const {
    if (file_.is_open() && !file_.good()) {
        return IOError::FileNotWritable;
    }
    return error_;
}

// -----------------------------------------------------------------------------
// Formatting helpers
// -----------------------------------------------------------------------------

namespace {

class StringSink {
public:
    void write(std::string_view data) { buffer_ += data; }
    [[nodiscard]] bool good() const { return true; }
    [[nodiscard]] std::string& buffer() { return buffer_; }

private:
    std::string buffer_;
};

} // namespace

std::string format_pi(const BigInt& scaled_pi, uint64_t output_digits,
                      uint64_t guard_digits) {
    StringSink sink;
    auto res = stream_pi(scaled_pi, output_digits, guard_digits, sink);
    static_cast<void>(res); // StringSink never fails.
    return std::move(sink.buffer());
}

std::string format_stats(const ConsoleOutput& output, Language lang) {
    std::ostringstream oss;
    if (lang == Language::Chinese) {
        oss << "项数: " << output.terms << "\n"
            << "位数: " << output.decimal_digits << "\n"
            << "耗时: " << output.elapsed_ms << " 毫秒\n";
    } else {
        oss << "Terms: " << output.terms << "\n"
            << "Digits: " << output.decimal_digits << "\n"
            << "Time: " << output.elapsed_ms << " ms\n";
    }
    return oss.str();
}

expected<ComputeOptions, IOError> read_interactive_options(
    Language lang, std::istream& is) {
    return read_interactive_options(ComputeOptions{}, lang, is);
}

expected<ComputeOptions, IOError> read_interactive_options(
    const ComputeOptions& defaults, Language lang, std::istream& is) {
    // The "last digit" easter egg needs no further input.
    if (defaults.last_digit_easter_egg) {
        return defaults;
    }

    ComputeOptions options = defaults;

    const bool chinese = (lang == Language::Chinese);

    if (options.terms == 0) {
        std::cout << (chinese ? "请输入项数 N（正整数）: "
                              : "Enter the number of terms (N): ");
        std::string line;
        if (!std::getline(is, line)) {
            return IOError::InteractiveInputFailed;
        }
        auto terms = parse_terms(trim_string(line));
        if (!terms) {
            return IOError::InteractiveInputFailed;
        }
        options.terms = *terms;
    }

    if (!options.output_path.has_value()) {
        auto save = read_yes_no(
            chinese ? "是否保存到文件？(y/n): "
                    : "Save to file? (y/n): ",
            lang, is);
        if (!save) {
            return save.error();
        }
        if (*save) {
            std::cout << (chinese ? "请输入输出文件路径: "
                                  : "Enter output file path: ");
            std::string path_str;
            if (!std::getline(is, path_str)) {
                return IOError::InteractiveInputFailed;
            }
            std::string trimmed = trim_string(path_str);
            if (trimmed.empty()) {
                return IOError::InteractiveInputFailed;
            }
            auto path = parse_output_path(trimmed);
            if (!path) {
                return IOError::InteractiveInputFailed;
            }
            options.output_path = *path;
        }
    }

    if (!options.show_stats) {
        auto stats = read_yes_no(
            chinese ? "是否显示统计信息？(y/n): "
                    : "Show statistics? (y/n): ",
            lang, is);
        if (!stats) {
            return stats.error();
        }
        options.show_stats = *stats;
    }

    if (!options.quiet) {
        auto quiet = read_yes_no(
            chinese ? "是否开启安静模式（不打印 π）？(y/n): "
                    : "Quiet mode (do not print pi)? (y/n): ",
            lang, is);
        if (!quiet) {
            return quiet.error();
        }
        options.quiet = *quiet;
    }

    if (!options.thread_count.has_value()) {
        std::cout << (chinese ? "工作线程数（0 = 硬件并发数）: "
                              : "Number of worker threads (0 = hardware concurrency): ");
        std::string line;
        if (!std::getline(is, line)) {
            return IOError::InteractiveInputFailed;
        }
        std::string trimmed = trim_string(line);
        if (trimmed.empty()) {
            options.thread_count = size_t{0};
        } else {
            auto count = parse_thread_count(trimmed);
            if (!count) {
                return IOError::InteractiveInputFailed;
            }
            options.thread_count = *count;
        }
    }

    if (!options.eco_mode) {
        auto eco = read_yes_no(
            chinese ? "是否开启节能模式（使用更少线程）？(y/n): "
                    : "Eco mode (use fewer threads)? (y/n): ",
            lang, is);
        if (!eco) {
            return eco.error();
        }
        options.eco_mode = *eco;
    }

    if (!options.max_memory_mb.has_value()) {
        std::cout << (chinese ? "最大内存限制（单位 MB，留空表示无限制）: "
                              : "Maximum memory in MB (empty = no limit): ");
        std::string line;
        if (!std::getline(is, line)) {
            return IOError::InteractiveInputFailed;
        }
        std::string trimmed = trim_string(line);
        if (!trimmed.empty()) {
            auto mb = parse_memory_mb(trimmed);
            if (!mb) {
                return IOError::InteractiveInputFailed;
            }
            options.max_memory_mb = *mb;
        }
    }

    if (!options.force) {
        auto force = read_yes_no(
            chinese ? "是否跳过内存安全 guard？(y/n): "
                    : "Skip memory safety guard? (y/n): ",
            lang, is);
        if (!force) {
            return force.error();
        }
        options.force = *force;
    }

    return options;
}

std::string to_string(IOError error, Language lang) {
    if (lang == Language::Chinese) {
        switch (error) {
            case IOError::FileNotWritable:
                return "无法写入指定文件";
            case IOError::PathInvalid:
                return "文件路径无效";
            case IOError::PathTooLong:
                return "文件路径过长";
            case IOError::InteractiveInputFailed:
                return "交互输入失败或包含无效数据";
        }
        return "未知 I/O 错误";
    }

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
