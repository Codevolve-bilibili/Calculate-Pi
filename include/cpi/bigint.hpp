// Copyright (c) 2026 Calculate Pi project
// SPDX-License-Identifier: MIT

#pragma once

#include "cpi/expected.hpp"

#include <compare>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace cpi {

enum class Sign { Positive, Negative };

enum class BigIntError {
    DivisionByZero,
    InvalidString,
    InvalidRadix,
    NegativeSqrt,
};

std::string to_string(BigIntError error);

// Forward declaration
class BigInt;

// ---------------------------------------------------------------------------
// Multiplication strategy interface
// ---------------------------------------------------------------------------
class MultiplicationStrategy {
public:
    virtual ~MultiplicationStrategy() = default;

    // Multiply two magnitude vectors (little-endian, base 2^32).
    // The returned vector must be normalized (no leading zeros except for zero).
    virtual std::vector<uint32_t> multiply(
        const std::vector<uint32_t>& a,
        const std::vector<uint32_t>& b) const = 0;
};

class NaiveMultiplier : public MultiplicationStrategy {
public:
    std::vector<uint32_t> multiply(
        const std::vector<uint32_t>& a,
        const std::vector<uint32_t>& b) const override;
};

class KaratsubaMultiplier : public MultiplicationStrategy {
public:
    explicit KaratsubaMultiplier(size_t threshold = 64) : threshold_(threshold) {}

    std::vector<uint32_t> multiply(
        const std::vector<uint32_t>& a,
        const std::vector<uint32_t>& b) const override;

private:
    size_t threshold_;

    static std::vector<uint32_t> karatsuba(
        const std::vector<uint32_t>& a,
        const std::vector<uint32_t>& b,
        size_t threshold);
};

class FFTMultiplier : public MultiplicationStrategy {
public:
    FFTMultiplier() = default;

    std::vector<uint32_t> multiply(
        const std::vector<uint32_t>& a,
        const std::vector<uint32_t>& b) const override;

private:
    static std::vector<uint32_t> fft_multiply(
        const std::vector<uint32_t>& a,
        const std::vector<uint32_t>& b);
};

class AdaptiveMultiplier : public MultiplicationStrategy {
public:
    explicit AdaptiveMultiplier(
        size_t karatsuba_threshold = 64,
        size_t fft_threshold = 1024);

    std::vector<uint32_t> multiply(
        const std::vector<uint32_t>& a,
        const std::vector<uint32_t>& b) const override;

private:
    std::unique_ptr<NaiveMultiplier> naive_;
    std::unique_ptr<KaratsubaMultiplier> karatsuba_;
    std::unique_ptr<FFTMultiplier> fft_;
    size_t karatsuba_threshold_;
    size_t fft_threshold_;
};

// ---------------------------------------------------------------------------
// BigInt
// ---------------------------------------------------------------------------
class BigInt {
public:
    // Constructors
    BigInt() noexcept;
    explicit BigInt(int64_t value);
    explicit BigInt(std::string_view decimal);

    // Accessors
    [[nodiscard]] bool is_zero() const noexcept;
    [[nodiscard]] bool is_positive() const noexcept;
    [[nodiscard]] bool is_negative() const noexcept;
    [[nodiscard]] Sign sign() const noexcept;
    [[nodiscard]] const std::vector<uint32_t>& limbs() const noexcept;

    // Magnitude comparison (public for internal helpers/tests).
    [[nodiscard]] int compare_magnitude(const BigInt& rhs) const;

    // Sign / magnitude
    [[nodiscard]] BigInt abs() const;
    [[nodiscard]] BigInt negate() const;

    // Arithmetic
    [[nodiscard]] BigInt operator+(const BigInt& rhs) const;
    [[nodiscard]] BigInt operator-(const BigInt& rhs) const;
    [[nodiscard]] BigInt operator*(const BigInt& rhs) const;
    [[nodiscard]] expected<BigInt, BigIntError> operator/(const BigInt& rhs) const;
    [[nodiscard]] expected<BigInt, BigIntError> operator%(const BigInt& rhs) const;

    BigInt& operator+=(const BigInt& rhs);
    BigInt& operator-=(const BigInt& rhs);
    BigInt& operator*=(const BigInt& rhs);
    [[nodiscard]] expected<BigInt, BigIntError> operator/=(const BigInt& rhs);
    [[nodiscard]] expected<BigInt, BigIntError> operator%=(const BigInt& rhs);

    // Increment / decrement helpers
    BigInt& operator++();
    BigInt operator++(int);
    BigInt& operator--();
    BigInt operator--(int);

    // Advanced operations
    [[nodiscard]] static BigInt pow(const BigInt& base, uint64_t exp);
    [[nodiscard]] static BigInt factorial(uint64_t n);
    [[nodiscard]] static expected<BigInt, BigIntError> sqrt(const BigInt& n);
    [[nodiscard]] static expected<BigInt, BigIntError> sqrt(
        const BigInt& n,
        const std::function<void(double)>& progress_callback);

    // Division with progress callback. operator/ remains callback-free.
    [[nodiscard]] expected<BigInt, BigIntError> divide(
        const BigInt& divisor,
        const std::function<void(double)>& progress_callback) const;

    // Bit shifts (only positive shift counts are valid; negative is undefined).
    [[nodiscard]] BigInt operator<<(uint64_t shift) const;
    [[nodiscard]] BigInt operator>>(uint64_t shift) const;

    // Conversion
    [[nodiscard]] std::string to_string() const;
    [[nodiscard]] std::string to_string_radix(int radix) const;

    // Stream exactly `digits` decimal digits of the absolute value, padding with
    // leading zeros. Emits chunks from most-significant to least-significant.
    void stream_decimal_fixed(uint64_t digits,
                              std::function<void(std::string_view)> sink) const;

    // Multiply strategy injection (primarily for testing).
    static void set_default_multiplier(std::shared_ptr<MultiplicationStrategy> strategy);
    static std::shared_ptr<MultiplicationStrategy> default_multiplier();

    // Internal magnitude constructor (used by this module, assumes normalized limbs).
    BigInt(Sign sign, std::vector<uint32_t> limbs);

private:
    Sign sign_;
    std::vector<uint32_t> limbs_;

    void normalize();

    [[nodiscard]] BigInt add_magnitude(const BigInt& rhs) const;
    [[nodiscard]] BigInt sub_magnitude(const BigInt& rhs) const; // assumes |this| >= |rhs|

    friend bool operator==(const BigInt& lhs, const BigInt& rhs);
    friend std::strong_ordering operator<=>(const BigInt& lhs, const BigInt& rhs);
    friend std::ostream& operator<<(std::ostream& os, const BigInt& value);
};

[[nodiscard]] bool operator==(const BigInt& lhs, const BigInt& rhs);
[[nodiscard]] std::strong_ordering operator<=>(const BigInt& lhs, const BigInt& rhs);

inline bool operator!=(const BigInt& lhs, const BigInt& rhs) { return !(lhs == rhs); }
inline bool operator<(const BigInt& lhs, const BigInt& rhs) { return (lhs <=> rhs) < 0; }
inline bool operator>(const BigInt& lhs, const BigInt& rhs) { return (lhs <=> rhs) > 0; }
inline bool operator<=(const BigInt& lhs, const BigInt& rhs) { return (lhs <=> rhs) <= 0; }
inline bool operator>=(const BigInt& lhs, const BigInt& rhs) { return (lhs <=> rhs) >= 0; }

std::ostream& operator<<(std::ostream& os, const BigInt& value);

} // namespace cpi
