// Copyright (c) 2026 Calculate Pi project
// SPDX-License-Identifier: MIT

#include "cpi/chudnovsky.hpp"

#include <cmath>
#include <chrono>
#include <stdexcept>

namespace cpi {

namespace {

constexpr int64_t kA = 13591409;
constexpr int64_t kB = 545140134;
constexpr uint64_t kC = 426880;
constexpr uint64_t kD = 10005;
// 640320^3, the per-term denominator factor in the Chudnovsky series.
constexpr uint64_t kQBase = 262537412640768000ULL;

// N(k) = -24 (2k+1)(6k+1)(6k+5).
// Product_{i=0}^{k-1} N(i) = (-1)^k (6k)! / (3k)!.
int64_t compute_N(uint64_t k) {
    // N(k) = -24 (2k+1)(6k+1)(6k+5).
    // All intermediate values fit in int64_t for k < 10^5, which covers the
    // term counts used by this project (~1e6 digits needs ~7e4 terms).
    const int64_t a = static_cast<int64_t>(2 * k + 1);
    const int64_t b = static_cast<int64_t>(6 * k + 1);
    const int64_t c = static_cast<int64_t>(6 * k + 5);
    return -24 * a * b * c;
}

} // namespace

uint64_t terms_for_digits(uint64_t digits) {
    if (digits == 0) {
        return 1;
    }
    return static_cast<uint64_t>(std::ceil(digits / DIGITS_PER_TERM)) + 1;
}

uint64_t digits_for_terms(uint64_t terms) {
    return static_cast<uint64_t>(terms * DIGITS_PER_TERM);
}

ChudnovskyCalculator::ChudnovskyCalculator(ChudnovskyOptions options)
    : options_(std::move(options)), pool_(nullptr) {}

ChudnovskyCalculator::ChudnovskyCalculator(
    ChudnovskyOptions options, std::shared_ptr<ThreadPool> pool)
    : options_(std::move(options)), pool_(std::move(pool)) {}

expected<PiValue, ComputeError> ChudnovskyCalculator::compute() {
    if (options_.terms == 0) {
        return ComputeError::InvalidTerms;
    }

    if (options_.output_digits == 0) {
        return ComputeError::InsufficientPrecision;
    }

    const uint64_t guard = options_.guard_digits;
    const uint64_t scale_digits = options_.output_digits + guard;

    try {
        // S = floor(sqrt(10005) * 10^scale_digits).
        // Compute as floor(sqrt(10005 * 10^(2*scale_digits))).
        BigInt ten(10);
        BigInt pow10_2m = BigInt::pow(ten, 2 * scale_digits);
        BigInt scaled_kd = BigInt(kD) * pow10_2m;
        BigInt S = BigInt::sqrt(scaled_kd).value();

        // C_s = 426880 * S.
        BigInt C_s = BigInt(kC) * S;

        auto start = std::chrono::steady_clock::now();
        SplitResult result = binary_split(0, options_.terms);
        auto elapsed = std::chrono::steady_clock::now() - start;
        uint64_t elapsed_ms = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());

        if (result.T.is_zero()) {
            return ComputeError::DivisionByZero;
        }

        BigInt numerator = C_s * result.Q;
        auto div_result = numerator / result.T;
        if (!div_result) {
            return ComputeError::DivisionByZero;
        }

        PiValue value;
        value.scaled_pi = std::move(*div_result);
        value.output_digits = options_.output_digits;
        value.guard_digits = guard;
        value.elapsed_ms = elapsed_ms;
        return value;
    } catch (const std::bad_alloc&) {
        return ComputeError::OutOfMemory;
    }
}

ChudnovskyCalculator::SplitResult
ChudnovskyCalculator::binary_split(uint64_t a, uint64_t b) {
    if (b - a < parallel_threshold_ || !pool_) {
        return binary_split_serial(a, b);
    }

    // Avoid nested deadlocks in a simple future-based ThreadPool: if the pool
    // already has work in flight, do this interval serially.
    if (pool_->busy()) {
        return binary_split_serial(a, b);
    }

    uint64_t m = a + (b - a) / 2;
    auto left_future = pool_->enqueue(
        &ChudnovskyCalculator::binary_split, this, a, m);
    auto right = binary_split(m, b);
    auto left = left_future.get();

    return merge(left, right);
}

ChudnovskyCalculator::SplitResult
ChudnovskyCalculator::binary_split_serial(uint64_t a, uint64_t b) {
    if (b - a == 1) {
        // Boundary values for [a, a+1).
        // N(a) = -24 (2a+1)(6a+1)(6a+5).  Fits in int64_t for the term counts
        // used in this project.
        BigInt P(static_cast<int64_t>(compute_N(a)));

        // Q(a,a+1) = (a+1)^3 * 640320^3.  Compute (a+1)^3 in int64_t to avoid
        // a wasteful BigInt::pow call for every leaf.
        int64_t a1 = static_cast<int64_t>(a) + 1;
        int64_t a1_cubed = a1 * a1 * a1;
        BigInt base_q = BigInt(a1_cubed) * BigInt(static_cast<int64_t>(kQBase));

        // T(a,a+1) = (13591409 + 545140134*a) * Q(a,a+1).  The linear
        // coefficient also fits in int64_t for our term ranges.
        int64_t coeff = kA + kB * static_cast<int64_t>(a);
        BigInt T = BigInt(coeff) * base_q;

        return {std::move(P), std::move(base_q), std::move(T)};
    }

    uint64_t m = a + (b - a) / 2;
    auto left = binary_split_serial(a, m);
    auto right = binary_split_serial(m, b);
    return merge(left, right);
}

ChudnovskyCalculator::SplitResult
ChudnovskyCalculator::merge(
    const SplitResult& left, const SplitResult& right) {
    SplitResult result;
    // P(a,b) = P(a,m) * P(m,b)
    result.P = left.P * right.P;
    // Q(a,b) = Q(a,m) * Q(m,b)
    result.Q = left.Q * right.Q;
    // T(a,b) = T(a,m) * Q(m,b) + P(a,m) * T(m,b)
    result.T = left.T * right.Q + left.P * right.T;
    return result;
}

std::string to_string(ComputeError error) {
    switch (error) {
    case ComputeError::InvalidTerms:
        return "Invalid terms: must be greater than zero";
    case ComputeError::InsufficientPrecision:
        return "Insufficient precision: output digit count must be greater than zero";
    case ComputeError::DivisionByZero:
        return "Division by zero encountered during computation";
    case ComputeError::OutOfMemory:
        return "Out of memory while computing pi";
    }
    return "Unknown compute error";
}

} // namespace cpi
