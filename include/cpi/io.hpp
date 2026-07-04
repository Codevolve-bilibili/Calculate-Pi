// Copyright (c) 2026 Calculate Pi project
// SPDX-License-Identifier: MIT

#pragma once

#include "cpi/bigint.hpp"
#include "cpi/cli.hpp"
#include "cpi/expected.hpp"

#include <concepts>
#include <filesystem>
#include <fstream>
#include <functional>
#include <istream>
#include <ostream>
#include <cstdint>
#include <string>
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

// -----------------------------------------------------------------------------
// Output sink concept and built-in sinks
// -----------------------------------------------------------------------------

template<typename Sink>
concept OutputSink = requires(Sink& s, std::string_view data) {
    { s.write(data) } -> std::same_as<void>;
    { s.good() } -> std::convertible_to<bool>;
};

class OstreamSink {
public:
    explicit OstreamSink(std::ostream& os);
    void write(std::string_view data);
    [[nodiscard]] bool good() const;

private:
    std::ostream& os_;
};

class FileSink {
public:
    explicit FileSink(const std::filesystem::path& path);
    void write(std::string_view data);
    [[nodiscard]] bool good() const;
    [[nodiscard]] IOError error() const;

private:
    std::ofstream file_;
    IOError error_ = IOError::FileNotWritable;
};

// -----------------------------------------------------------------------------
// Streaming pi formatting
// -----------------------------------------------------------------------------

namespace detail {
[[nodiscard]] BigInt round_scaled_pi(const BigInt& scaled_pi,
                                     uint64_t guard_digits);
}

template<OutputSink Sink>
[[nodiscard]] expected<void, IOError> stream_pi(const BigInt& scaled_pi,
                                                uint64_t output_digits,
                                                uint64_t guard_digits,
                                                Sink& sink,
                                                const std::function<void(double)>& progress_callback = {}) {
    BigInt rounded = detail::round_scaled_pi(scaled_pi, guard_digits);

    BigInt ten(10);
    BigInt pow10_out = BigInt::pow(ten, output_digits);
    BigInt int_part = (rounded / pow10_out).value();
    BigInt frac_part = (rounded % pow10_out).value();

    sink.write(int_part.to_string());
    sink.write(".");

    bool ok = true;
    uint64_t emitted = 0;
    frac_part.stream_decimal_fixed(output_digits, [&](std::string_view chunk) {
        if (!ok) return;
        sink.write(chunk);
        if (!sink.good()) ok = false;
        emitted += static_cast<uint64_t>(chunk.size());
        if (progress_callback && emitted < output_digits) {
            progress_callback(static_cast<double>(emitted)
                              / static_cast<double>(output_digits));
        }
    });

    if (!ok) return IOError::FileNotWritable;
    return {};
}

// Backwards-compatible helper: format the full pi string into memory.
[[nodiscard]] std::string format_pi(const BigInt& scaled_pi,
                                    uint64_t output_digits,
                                    uint64_t guard_digits);

[[nodiscard]] std::string format_stats(const ConsoleOutput& output,
                                       Language lang = Language::Chinese);

[[nodiscard]] expected<ComputeOptions, IOError> read_interactive_options(
    Language lang, std::istream& is = std::cin);
[[nodiscard]] expected<ComputeOptions, IOError> read_interactive_options(
    const ComputeOptions& defaults, Language lang, std::istream& is = std::cin);

[[nodiscard]] std::string to_string(IOError error,
                                    Language lang = Language::Chinese);

} // namespace cpi
