// Copyright (c) 2026 Calculate Pi project
// SPDX-License-Identifier: MIT

#pragma once

#include "cpi/bigint.hpp"
#include "cpi/concurrency.hpp"
#include "cpi/expected.hpp"

#include <cstdint>
#include <memory>
#include <string>

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
};

struct PiValue {
    BigInt scaled_pi;   // π * 10^(output_digits + guard_digits).
    uint64_t output_digits;
    uint64_t guard_digits;
    uint64_t elapsed_ms;
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

    ChudnovskyOptions options_;
    std::shared_ptr<ThreadPool> pool_;
    uint64_t parallel_threshold_ = 64;
};

[[nodiscard]] std::string to_string(ComputeError error);

} // namespace cpi
