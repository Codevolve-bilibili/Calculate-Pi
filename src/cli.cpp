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

std::string to_lower_ascii(std::string_view s) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        if (c >= 'A' && c <= 'Z') {
            result.push_back(static_cast<char>(c + ('a' - 'A')));
        } else {
            result.push_back(c);
        }
    }
    return result;
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

std::optional<Language> parse_language(std::string_view s) {
    const std::string lowered = to_lower_ascii(s);
    if (lowered == "en" || lowered == "english" || lowered == "英文" ||
        lowered == "eng") {
        return Language::English;
    }
    if (lowered == "zh" || lowered == "chinese" || lowered == "中文" ||
        lowered == "zh-cn" || lowered == "cn" || lowered == "简体") {
        return Language::Chinese;
    }
    return std::nullopt;
}

expected<CliResult, CliError> parse_cli(int argc, const char* argv[]) {
    if (argc == 1) {
        return CliResult{RunMode::Interactive, std::nullopt, ComputeOptions{}};
    }

    CliResult result{RunMode::Compute, std::nullopt, ComputeOptions{}};
    ComputeOptions& opts = *result.compute_options;
    bool interactive_requested = false;
    bool help_requested = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);

        if (arg == "--help" || arg == "-h") {
            help_requested = true;
            continue;
        }

        if (arg == "--interactive" || arg == "-i") {
            interactive_requested = true;
            continue;
        }

        if (arg == "--lang" || arg == "-l") {
            if (i + 1 >= argc) {
                return CliError::MissingArgumentValue;
            }
            auto lang = parse_language(argv[++i]);
            if (!lang) {
                return CliError::InvalidLanguage;
            }
            if (result.language.has_value()) {
                return CliError::ConflictingOptions;
            }
            result.language = *lang;
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

    if (help_requested) {
        result.mode = RunMode::Help;
        result.compute_options = std::nullopt;
        return result;
    }

    // --terms is required for compute mode unless --last-digit is used.
    if (result.mode == RunMode::Compute && opts.terms == 0 &&
        !opts.last_digit_easter_egg) {
        return CliError::MissingArgumentValue;
    }

    return result;
}

std::string to_string(CliError error, Language lang) {
    if (lang == Language::Chinese) {
        switch (error) {
            case CliError::UnknownArgument:
                return "未知的命令行参数";
            case CliError::MissingArgumentValue:
                return "命令行选项缺少值";
            case CliError::InvalidTermsFormat:
                return "项数格式无效：必须是正十进制整数";
            case CliError::TermsOutOfRange:
                return "项数越界：必须是正整数";
            case CliError::InvalidOutputPath:
                return "输出文件路径无效";
            case CliError::ConflictingOptions:
                return "命令行选项冲突";
            case CliError::InvalidThreadCount:
                return "线程数无效：必须是十进制整数（0 = 硬件并发数）";
            case CliError::InvalidMemoryLimit:
                return "内存限制无效：必须是正十进制整数（单位 MB）";
            case CliError::InvalidLanguage:
                return "语言无效：支持 en/english/英文 或 zh/chinese/中文";
        }
        return "未知 CLI 错误";
    }

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
        case CliError::InvalidLanguage:
            return "Invalid language: use en/english/英文 or zh/chinese/中文";
    }
    return "Unknown CLI error";
}

void print_help(std::ostream& os, Language lang) {
    if (lang == Language::Chinese) {
        os << "Calculate Pi - 楚德诺夫斯基二进制分裂算法\n"
           << "\n"
           << "用法:\n"
           << "  CalculatePi [选项]\n"
           << "  CalculatePi --terms <N> [-o <路径>] [--stats]\n"
           << "  CalculatePi --interactive [选项]\n"
           << "\n"
           << "选项:\n"
           << "  -n, --terms <N>      楚德诺夫斯基级数项数（命令行模式下必需）\n"
           << "  -o, --output <路径>  将结果写入指定文件\n"
           << "      --stats          显示额外统计信息\n"
           << "  -q, --quiet          不在控制台打印完整 π 字符串\n"
           << "  -t, --threads <N>    工作线程数（0 = 硬件并发数）\n"
           << "  -e, --eco            使用更少线程以降低 CPU 负载\n"
           << "  -m, --max-memory-mb  计算允许使用的最大内存（MB）\n"
           << "  -f, --force          跳过内存安全检查\n"
           << "      --last-digit     说明 π 没有最后一位\n"
           << "  -l, --lang <语言>    界面语言：en/english/英文 或 zh/chinese/中文\n"
           << "  -i, --interactive    进入交互模式（将其他标志作为默认值）\n"
           << "  -h, --help           显示此帮助信息\n"
           << "\n"
           << "若未提供任何参数，程序将进入交互模式。\n";
        return;
    }

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
       << "  -l, --lang <lang>    UI language: en/english/英文 or zh/chinese/中文\n"
       << "  -i, --interactive    Enter interactive mode (use other flags as defaults)\n"
       << "  -h, --help           Show this help message\n"
       << "\n"
       << "If no arguments are given, the program enters interactive mode.\n";
}

} // namespace cpi
