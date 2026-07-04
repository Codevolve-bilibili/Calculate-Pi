// Copyright (c) 2026 Calculate Pi project
// SPDX-License-Identifier: MIT

#pragma once

#include "cpi/expected.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

namespace cpi {

enum class RunMode {
    Help,
    Interactive,
    Compute
};

enum class Language {
    English,
    Chinese,
};

enum class TimeUnit {
    Seconds,
    Minutes,
    Hours,
};

struct ComputeOptions {
    uint64_t terms = 0;
    std::optional<std::filesystem::path> output_path;
    bool show_stats = false;
    bool quiet = false;
    bool force = false;
    bool eco_mode = false;
    bool last_digit_easter_egg = false;
    std::optional<size_t> thread_count;
    std::optional<uint64_t> max_memory_mb;

    // Infinite computation mode.
    bool infinite = false;
    uint64_t step = 0;
    std::optional<uint64_t> infinite_max_files;
    std::optional<uint64_t> infinite_max_time_seconds;
};

struct CliResult {
    RunMode mode;
    std::optional<Language> language;
    std::optional<ComputeOptions> compute_options;
};

enum class CliError {
    UnknownArgument,
    MissingArgumentValue,
    InvalidTermsFormat,
    TermsOutOfRange,
    InvalidOutputPath,
    ConflictingOptions,
    InvalidThreadCount,
    InvalidMemoryLimit,
    InvalidLanguage,
    InvalidStepFormat,
    StepOutOfRange,
    InvalidInfiniteMaxFiles,
    InvalidInfiniteMaxTime,
    InvalidInfiniteTimeUnit,
    InfiniteRequiresStep,
    InfiniteRequiresOutput,
    InfiniteNotAllowedWithInteractive,
    InfiniteNotAllowedWithLastDigit,
};

[[nodiscard]] expected<uint64_t, CliError> parse_terms(std::string_view s);
[[nodiscard]] expected<std::filesystem::path, CliError> parse_output_path(
    std::string_view s);
[[nodiscard]] expected<uint64_t, CliError> parse_memory_mb(std::string_view s);
[[nodiscard]] expected<size_t, CliError> parse_thread_count(std::string_view s);
[[nodiscard]] std::optional<Language> parse_language(std::string_view s);
[[nodiscard]] expected<uint64_t, CliError> parse_step(std::string_view s);
[[nodiscard]] expected<uint64_t, CliError> parse_max_files(std::string_view s);
[[nodiscard]] expected<uint64_t, CliError> parse_max_time(std::string_view s);
[[nodiscard]] std::optional<TimeUnit> parse_time_unit(std::string_view s);

expected<CliResult, CliError> parse_cli(int argc, const char* argv[]);
std::string to_string(CliError error, Language lang = Language::Chinese);

void print_help(std::ostream& os, Language lang = Language::Chinese);

} // namespace cpi
