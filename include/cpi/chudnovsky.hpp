// Copyright (c) 2026 Calculate Pi project
// SPDX-License-Identifier: MIT

#pragma once

#include "cpi/bigint.hpp"
#include "cpi/concurrency.hpp"
#include "cpi/expected.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace cpi {

constexpr double DIGITS_PER_TERM = 14.1816474627254776555;

// Convert desired decimal digits to the number of Chudnovsky terms needed.
[[nodiscard]] uint64_t terms_for_digits(uint64_t digits);

// Convert a term count to the corresponding approximate decimal digit count.
[[nodiscard]] uint64_t digits_for_terms(uint64_t terms);

struct ChudnovskyOptions {
    uint64_t terms;           // Number of series terms N.
    uint64_t output_digits;   // Target decimal digits D.
    uint64_t guard_digits = 5; // Guard digits G for rounding error absorption.

    // Optional progress callback; argument is in [0, 1].
    std::function<void(double)> progress_callback;
    uint64_t progress_interval_ms = 50;
};

struct PiValue {
    BigInt scaled_pi;   // π * 10^(output_digits + guard_digits).
    uint64_t output_digits;
    uint64_t guard_digits;
    uint64_t elapsed_ms;
};

// Holds the merged binary-split state P(0,terms), Q(0,terms), T(0,terms).
// This can be reused across incremental computations to avoid recomputing the
// entire series from scratch.
struct ChudnovskyState {
    BigInt P;
    BigInt Q;
    BigInt T;
    uint64_t terms = 0;
};

enum class ComputeError {
    InvalidTerms,
    InsufficientPrecision,
    DivisionByZero,
    OutOfMemory,
};

class ChudnovskyCalculator {
public:
    explicit ChudnovskyCalculator(ChudnovskyOptions options);
    explicit ChudnovskyCalculator(
        ChudnovskyOptions options, std::shared_ptr<ThreadPool> pool);

    [[nodiscard]] expected<PiValue, ComputeError> compute();

    // Compute pi for new_terms by extending a previously computed state.
    // Returns both the pi value and the new merged state for the next call.
    [[nodiscard]] expected<std::pair<PiValue, ChudnovskyState>, ComputeError>
    compute_incremental(uint64_t new_terms,
                        uint64_t output_digits,
                        const ChudnovskyState& previous);

private:
    struct SplitResult {
        BigInt P;
        BigInt Q;
        BigInt T;
    };

    [[nodiscard]] SplitResult binary_split(uint64_t a, uint64_t b);
    [[nodiscard]] SplitResult binary_split_serial(uint64_t a, uint64_t b);
    [[nodiscard]] SplitResult merge(
        const SplitResult& left, const SplitResult& right);
    void report_progress(double p);

    // Shared finalization: sqrt(10005) at the current scale, then C_s*Q/T.
    [[nodiscard]] expected<PiValue, ComputeError> finalize_pi(
        const SplitResult& result,
        std::chrono::steady_clock::time_point start);

    ChudnovskyOptions options_;
    std::shared_ptr<ThreadPool> pool_;
    uint64_t parallel_threshold_ = 64;
    std::atomic<uint64_t> completed_terms_{0};
    std::atomic<double> overall_progress_{0.0};
    std::mutex progress_mutex_;
};

[[nodiscard]] std::string to_string(ComputeError error);

} // namespace cpi
