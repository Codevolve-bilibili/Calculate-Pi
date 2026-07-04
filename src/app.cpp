// Copyright (c) 2026 Calculate Pi project
// SPDX-License-Identifier: MIT

#include "cpi/app.hpp"

#include "cpi/chudnovsky.hpp"
#include "cpi/concurrency.hpp"
#include "cpi/io.hpp"
#include "cpi/sysinfo.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <thread>

namespace cpi {

namespace {

std::string app_error_message(AppError error, Language lang) {
    if (lang == Language::Chinese) {
        switch (error) {
            case AppError::CliError:
                return "命令行错误";
            case AppError::ComputeError:
                return "计算错误";
            case AppError::IOError:
                return "I/O 错误";
            case AppError::MemoryGuard:
                return "超出内存限制";
            case AppError::UnknownError:
                return "未知错误";
        }
        return "未知应用错误";
    }

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

// Compute the memory limit in bytes from ComputeOptions.
uint64_t compute_allowed_memory_bytes(const ComputeOptions& options) {
    if (options.max_memory_mb.has_value()) {
        return *options.max_memory_mb * 1024 * 1024;
    }
    auto total_mem = total_physical_memory_bytes();
    if (total_mem.has_value()) {
        // Default guard: allow up to 50% of physical memory.
        return *total_mem / 2;
    }
    // Conservative fallback when system memory is unknown.
    constexpr uint64_t kFallbackBytes = 1024ULL * 1024 * 1024;
    return kFallbackBytes;
}

// Generate a timestamped filename for infinite mode: pi_YYYYMMDD_HHMMSS_mmm.txt.
std::filesystem::path generate_infinite_filename(
    const std::filesystem::path& dir) {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) %
              1000;
    std::time_t t = std::chrono::system_clock::to_time_t(now);

    std::tm local_tm{};
#ifdef _WIN32
    localtime_s(&local_tm, &t);
#else
    localtime_r(&t, &local_tm);
#endif

    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &local_tm);

    std::ostringstream name;
    name << "pi_" << buf << '_' << std::setfill('0') << std::setw(3)
         << ms.count() << ".txt";
    return dir / name.str();
}

} // namespace

namespace {

std::optional<Language> detect_language_from_argv(int argc, const char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if ((arg == "--lang" || arg == "-l") && i + 1 < argc) {
            return parse_language(argv[++i]);
        }
    }
    return std::nullopt;
}

} // namespace

Application::Application() = default;

int Application::run(int argc, const char* argv[]) {
    auto cli_result = parse_cli(argc, argv);
    if (!cli_result) {
        Language err_lang = detect_language_from_argv(argc, argv)
                                .value_or(Language::Chinese);
        print_error(to_string(cli_result.error(), err_lang), std::cerr);
        return 1;
    }

    // Help mode uses the default language (Chinese) directly without asking.
    if (cli_result->mode == RunMode::Help) {
        Language lang = cli_result->language.value_or(Language::Chinese);
        print_help(std::cout, lang);
        return 0;
    }

    // Resolve language first: prompt if it was not specified on the CLI.
    Language lang = resolve_language(*cli_result, std::cin);

    if (cli_result->mode == RunMode::Interactive ||
        cli_result->mode == RunMode::Compute) {
        auto options = gather_options(*cli_result, lang);
        if (!options) {
            print_error(app_error_message(options.error(), lang), std::cerr);
            return 1;
        }
        return run_compute(*options, lang);
    }

    print_error(app_error_message(AppError::UnknownError, lang), std::cerr);
    return 1;
}

Language Application::resolve_language(const CliResult& cli,
                                       std::istream& is) {
    if (cli.language.has_value()) {
        return *cli.language;
    }

    while (true) {
        std::cout << "请选择语言 / Choose language (zh/中文/en/english): ";
        std::string line;
        if (!std::getline(is, line)) {
            // If input is unavailable, fall back to the default language.
            return Language::Chinese;
        }
        auto lang = parse_language(line);
        if (lang) {
            return *lang;
        }
        std::cout << "无效输入，请重新选择。/ Invalid input, please try again.\n";
    }
}

void Application::print_help(std::ostream& os, Language lang) {
    cpi::print_help(os, lang);
}

void Application::print_error(std::string_view message, std::ostream& os) {
    os << "Error: " << message << "\n";
}

expected<ComputeOptions, AppError> Application::gather_options(
    const CliResult& cli, Language lang) {
    if (cli.mode == RunMode::Compute) {
        if (!cli.compute_options.has_value()) {
            return AppError::CliError;
        }
        return *cli.compute_options;
    }

    if (cli.mode == RunMode::Interactive) {
        auto opts = read_interactive_options(
            cli.compute_options.value_or(ComputeOptions{}), lang);
        if (!opts) {
            print_error(to_string(opts.error(), lang), std::cerr);
            return AppError::IOError;
        }
        return *opts;
    }

    return AppError::CliError;
}

int Application::run_compute(const ComputeOptions& options, Language lang) {
    // Easter egg: pi has no last digit.
    if (options.last_digit_easter_egg) {
        if (lang == Language::Chinese) {
            std::cout << "π 是无理数——它没有最后一位。\n"
                      << "使用 --terms <N> 计算前 N 位，或使用 --help 查看更多选项。\n";
        } else {
            std::cout << "π is irrational — it has no last digit.\n"
                      << "Use --terms <N> to compute the first N digits, or --help for more options.\n";
        }
        return 0;
    }

    if (options.infinite) {
        return run_infinite(options, lang);
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
            if (lang == Language::Chinese) {
                oss << "请求的项数大约需要 "
                    << format_memory_mib(estimated_bytes)
                    << " 内存。\n"
                    << "请使用 --terms <N>、--max-memory-mb <M> 或 --force 继续。\n";
            } else {
                oss << "Requested terms would require approximately "
                    << format_memory_mib(estimated_bytes)
                    << " of memory.\n"
                    << "Use --terms <N>, --max-memory-mb <M>, or --force to proceed anyway.\n";
            }
            print_error(oss.str(), std::cerr);
            return static_cast<int>(AppError::MemoryGuard);
        }
    }

    ChudnovskyOptions chud_opts;
    chud_opts.terms = options.terms;
    chud_opts.output_digits = output_digits;
    chud_opts.guard_digits = guard_digits;
    struct ProgressState {
        double last_p = -1.0;
        std::string last_text;
    };

    std::function<void(double)> progress_callback = [lang,
                                                   state = std::make_shared<ProgressState>()](double p) {
        if (p <= state->last_p) {
            return;
        }
        state->last_p = p;

        std::ostringstream oss;
        oss << (lang == Language::Chinese ? "计算进度: " : "Progress: ")
            << std::fixed << std::setprecision(2) << (p * 100.0) << '%';
        std::string text = oss.str();
        if (text == state->last_text) {
            return;
        }
        state->last_text = text;
        // Pad with spaces to erase any leftover characters from a longer
        // previous progress line (e.g. 100.00% followed by 4.76%).
        std::cerr << '\r' << text << std::string(20, ' ') << std::flush;
    };
    chud_opts.progress_callback = progress_callback;

    // Determine thread count.
    size_t num_threads = options.thread_count.value_or(0);
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

    ConsoleOutput stats;
    stats.terms = options.terms;
    stats.decimal_digits = output_digits;
    stats.elapsed_ms = result->elapsed_ms;

    // Warn when a large result would be printed to the console without --output.
    constexpr uint64_t kLargeOutputThreshold = 1'000'000;
    if (!options.quiet && !options.output_path.has_value() &&
        output_digits > kLargeOutputThreshold) {
        if (lang == Language::Chinese) {
            std::cerr << "警告：结果较大；请使用 --output <文件> 保存到文件，而不是打印到控制台。\n";
        } else {
            std::cerr << "Warning: Large result; use --output <file> to save instead of printing to console.\n";
        }
    }

    auto output_progress = [&progress_callback](double local) {
        if (progress_callback) {
            local = std::min(local, 0.999);
            progress_callback(0.20 + 0.80 * local);
        }
    };

    if (!options.quiet && options.output_path.has_value()) {
        OstreamSink console_sink(std::cout);
        FileSink file_sink(*options.output_path);
        if (!file_sink.good()) {
            print_error(to_string(file_sink.error(), lang), std::cerr);
            return 1;
        }
        struct MultiSink {
            OstreamSink* console;
            FileSink* file;
            bool ok = true;
            void write(std::string_view data) {
                if (!ok) return;
                console->write(data);
                if (!console->good()) ok = false;
                file->write(data);
                if (!file->good()) ok = false;
            }
            [[nodiscard]] bool good() const { return ok; }
        } multi{&console_sink, &file_sink};
        auto res = stream_pi(result->scaled_pi, output_digits, result->guard_digits,
                             multi, output_progress);
        if (!res) {
            print_error(to_string(res.error(), lang), std::cerr);
            return 1;
        }
        std::cout << '\n';
        if (progress_callback) {
            progress_callback(1.0);
            std::cerr << '\n';
        }
        if (options.show_stats) {
            std::cout << format_stats(stats, lang);
        }
        std::cout << '\n';
    } else if (!options.quiet) {
        OstreamSink console_sink(std::cout);
        auto res = stream_pi(result->scaled_pi, output_digits, result->guard_digits,
                             console_sink, output_progress);
        if (!res) {
            print_error(to_string(res.error(), lang), std::cerr);
            return 1;
        }
        std::cout << '\n';
        if (progress_callback) {
            progress_callback(1.0);
            std::cerr << '\n';
        }
        if (options.show_stats) {
            std::cout << format_stats(stats, lang);
        }
        std::cout << '\n';
    } else if (options.output_path.has_value()) {
        FileSink file_sink(*options.output_path);
        if (!file_sink.good()) {
            print_error(to_string(file_sink.error(), lang), std::cerr);
            return 1;
        }
        auto res = stream_pi(result->scaled_pi, output_digits, result->guard_digits,
                             file_sink, output_progress);
        if (!res) {
            print_error(to_string(res.error(), lang), std::cerr);
            return 1;
        }
        if (progress_callback) {
            progress_callback(1.0);
            std::cerr << '\n';
        }
        if (options.show_stats) {
            if (lang == Language::Chinese) {
                std::cout << "已计算 " << output_digits << " 位，耗时 "
                          << result->elapsed_ms << " 毫秒。\n";
            } else {
                std::cout << "Computed " << output_digits << " digits in "
                          << result->elapsed_ms << " ms.\n";
            }
        }
    } else if (options.show_stats) {
        if (progress_callback) {
            progress_callback(1.0);
            std::cerr << '\n';
        }
        if (lang == Language::Chinese) {
            std::cout << "已计算 " << output_digits << " 位，耗时 "
                      << result->elapsed_ms << " 毫秒。\n";
        } else {
            std::cout << "Computed " << output_digits << " digits in "
                      << result->elapsed_ms << " ms.\n";
        }
    } else if (progress_callback) {
        progress_callback(1.0);
        std::cerr << '\n';
    }

    return 0;
}

int Application::run_infinite(const ComputeOptions& options, Language lang) {
    const auto& out_dir = *options.output_path;

    try {
        std::filesystem::create_directories(out_dir);
    } catch (const std::filesystem::filesystem_error& e) {
        std::ostringstream oss;
        if (lang == Language::Chinese) {
            oss << "无法创建输出目录: ";
        } else {
            oss << "Unable to create output directory: ";
        }
        oss << e.what();
        print_error(oss.str(), std::cerr);
        return static_cast<int>(AppError::IOError);
    }

    if (!std::filesystem::is_directory(out_dir)) {
        print_error(lang == Language::Chinese ? "输出路径不是目录"
                                              : "Output path is not a directory",
                    std::cerr);
        return static_cast<int>(AppError::IOError);
    }

    // Determine thread count.
    size_t num_threads = options.thread_count.value_or(0);
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

    uint64_t allowed_bytes = std::numeric_limits<uint64_t>::max();
    if (!options.force) {
        allowed_bytes = compute_allowed_memory_bytes(options);
    }

    std::optional<std::chrono::seconds> max_duration;
    if (options.infinite_max_time_seconds.has_value()) {
        max_duration = std::chrono::seconds(*options.infinite_max_time_seconds);
    }
    auto loop_start = std::chrono::steady_clock::now();

    ChudnovskyState state;
    state.P = BigInt(1);
    state.Q = BigInt(1);
    state.T = BigInt(0);
    state.terms = 0;

    uint64_t current_terms = options.terms;
    uint64_t iteration = 0;
    uint64_t files_created = 0;
    constexpr uint64_t kGuardDigits = 5;

    while (true) {
        ++iteration;

        if (options.infinite_max_files.has_value() &&
            files_created >= *options.infinite_max_files) {
            break;
        }

        if (max_duration.has_value()) {
            auto elapsed = std::chrono::steady_clock::now() - loop_start;
            if (elapsed >= *max_duration) {
                break;
            }
        }

        uint64_t output_digits = digits_for_terms(current_terms);
        if (output_digits == 0) {
            output_digits = 1;
        } else {
            --output_digits;
        }
        uint64_t scale_digits = output_digits + kGuardDigits;

        if (!options.force) {
            uint64_t estimated_bytes = estimate_memory_bytes(scale_digits);
            if (estimated_bytes > allowed_bytes) {
                std::ostringstream oss;
                if (lang == Language::Chinese) {
                    oss << "请求的项数大约需要 "
                       << format_memory_mib(estimated_bytes)
                       << " 内存。\n"
                       << "请使用更小的 --step、--max-memory-mb <M> 或 --force 继续。\n";
                } else {
                    oss << "Requested terms would require approximately "
                       << format_memory_mib(estimated_bytes)
                       << " of memory.\n"
                       << "Use a smaller --step, --max-memory-mb <M>, or --force to proceed anyway.\n";
                }
                print_error(oss.str(), std::cerr);
                return static_cast<int>(AppError::MemoryGuard);
            }
        }

        ChudnovskyOptions chud_opts;
        chud_opts.terms = current_terms;
        chud_opts.output_digits = output_digits;
        chud_opts.guard_digits = kGuardDigits;
        // No progress callback in infinite mode to keep console output clean.

        ChudnovskyCalculator calculator(chud_opts, pool);
        auto result = calculator.compute_incremental(current_terms, output_digits,
                                                     state);
        if (!result) {
            print_error(to_string(result.error()), std::cerr);
            return 1;
        }

        PiValue pi = std::move(result->first);
        state = std::move(result->second);

        auto file_path = generate_infinite_filename(out_dir);
        FileSink file_sink(file_path);
        if (!file_sink.good()) {
            print_error(to_string(file_sink.error(), lang), std::cerr);
            return 1;
        }
        auto write_res = stream_pi(pi.scaled_pi, output_digits, pi.guard_digits,
                                   file_sink, /*progress_callback=*/{});
        if (!write_res) {
            print_error(to_string(write_res.error(), lang), std::cerr);
            return 1;
        }
        ++files_created;

        ConsoleOutput stats;
        stats.terms = current_terms;
        stats.decimal_digits = output_digits;
        stats.elapsed_ms = pi.elapsed_ms;

        if (lang == Language::Chinese) {
            std::cout << "第 " << iteration << " 次迭代\n";
        } else {
            std::cout << "Iteration " << iteration << '\n';
        }
        std::cout << format_stats(stats, lang);
        std::cout << (lang == Language::Chinese ? "输出文件: " : "Output file: ")
                  << file_path << "\n\n";

        if (current_terms > std::numeric_limits<uint64_t>::max() - options.step) {
            if (lang == Language::Chinese) {
                std::cout << "已达到 uint64_t 项数上限，停止。\n";
            } else {
                std::cout << "Reached uint64_t term limit, stopping.\n";
            }
            break;
        }
        current_terms += options.step;
    }

    return 0;
}

} // namespace cpi
