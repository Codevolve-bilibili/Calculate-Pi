// Copyright (c) 2026 Calculate Pi project
// SPDX-License-Identifier: MIT

#include "cpi/bigint.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace cpi {

namespace {

constexpr uint64_t BASE = 1ULL << 32;
constexpr uint64_t BASE_MASK = BASE - 1;

// Return 2^bits as a positive BigInt.
BigInt power_of_two(uint64_t bits) {
    std::vector<uint32_t> limbs(bits / 32 + 1, 0);
    limbs[bits / 32] = 1u << (bits % 32);
    return BigInt(Sign::Positive, std::move(limbs));
}

// Thread-local default multiplier. Initialized lazily to AdaptiveMultiplier.
std::shared_ptr<MultiplicationStrategy>& thread_local_default_multiplier() {
    static thread_local std::shared_ptr<MultiplicationStrategy> strategy;
    if (!strategy) {
        strategy = std::make_shared<AdaptiveMultiplier>();
    }
    return strategy;
}

} // namespace

// ---------------------------------------------------------------------------
// Error conversion
// ---------------------------------------------------------------------------
std::string to_string(BigIntError error) {
    switch (error) {
        case BigIntError::DivisionByZero: return "division by zero";
        case BigIntError::InvalidString: return "invalid decimal string";
        case BigIntError::InvalidRadix: return "invalid radix (must be 2..36)";
        case BigIntError::NegativeSqrt: return "square root of negative number";
    }
    return "unknown BigInt error";
}

// ---------------------------------------------------------------------------
// Default multiplier
// ---------------------------------------------------------------------------
void BigInt::set_default_multiplier(std::shared_ptr<MultiplicationStrategy> strategy) {
    thread_local_default_multiplier() = std::move(strategy);
}

std::shared_ptr<MultiplicationStrategy> BigInt::default_multiplier() {
    return thread_local_default_multiplier();
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
BigInt::BigInt() noexcept : sign_(Sign::Positive), limbs_() {}

BigInt::BigInt(Sign sign, std::vector<uint32_t> limbs)
    : sign_(sign), limbs_(std::move(limbs)) {
    normalize();
}

BigInt::BigInt(int64_t value) : sign_(Sign::Positive) {
    if (value == 0) {
        normalize();
        return;
    }
    if (value < 0) {
        sign_ = Sign::Negative;
        value = -value;
    }
    limbs_.push_back(static_cast<uint32_t>(value & BASE_MASK));
    limbs_.push_back(static_cast<uint32_t>(static_cast<uint64_t>(value) >> 32));
    normalize();
}

BigInt::BigInt(std::string_view decimal) : sign_(Sign::Positive) {
    if (decimal.empty()) {
        throw std::invalid_argument("BigInt string is empty");
    }

    // Strip optional leading '+' or '-'
    size_t pos = 0;
    if (decimal[pos] == '+' || decimal[pos] == '-') {
        if (decimal[pos] == '-') sign_ = Sign::Negative;
        ++pos;
    }

    if (pos == decimal.size()) {
        throw std::invalid_argument("BigInt string has no digits");
    }

    // Skip leading zeros
    while (pos < decimal.size() && decimal[pos] == '0') ++pos;

    if (pos == decimal.size()) {
        sign_ = Sign::Positive;
        normalize();
        return;
    }

    // Validate and convert from base 10.
    BigInt result(0);
    for (; pos < decimal.size(); ++pos) {
        char c = decimal[pos];
        if (c < '0' || c > '9') {
            throw std::invalid_argument("BigInt string contains non-digit character");
        }
        result *= BigInt(10);
        result += BigInt(static_cast<int64_t>(c - '0'));
    }
    limbs_ = std::move(result.limbs_);
    // sign_ is preserved from the optional leading '-'.
}

// ---------------------------------------------------------------------------
// Normalization and accessors
// ---------------------------------------------------------------------------
void BigInt::normalize() {
    while (!limbs_.empty() && limbs_.back() == 0) {
        limbs_.pop_back();
    }
    if (limbs_.empty()) {
        sign_ = Sign::Positive;
    }
}

bool BigInt::is_zero() const noexcept { return limbs_.empty(); }
bool BigInt::is_positive() const noexcept { return sign_ == Sign::Positive && !is_zero(); }
bool BigInt::is_negative() const noexcept { return sign_ == Sign::Negative; }
Sign BigInt::sign() const noexcept { return sign_; }
const std::vector<uint32_t>& BigInt::limbs() const noexcept { return limbs_; }

// ---------------------------------------------------------------------------
// Magnitude helpers
// ---------------------------------------------------------------------------
int BigInt::compare_magnitude(const BigInt& rhs) const {
    if (limbs_.size() != rhs.limbs_.size()) {
        return static_cast<int>(limbs_.size()) < static_cast<int>(rhs.limbs_.size()) ? -1 : 1;
    }
    for (size_t i = limbs_.size(); i-- > 0;) {
        if (limbs_[i] != rhs.limbs_[i]) {
            return limbs_[i] < rhs.limbs_[i] ? -1 : 1;
        }
    }
    return 0;
}

BigInt BigInt::abs() const {
    BigInt result(*this);
    result.sign_ = Sign::Positive;
    return result;
}

BigInt BigInt::negate() const {
    BigInt result(*this);
    if (!result.is_zero()) {
        result.sign_ = (result.sign_ == Sign::Positive) ? Sign::Negative : Sign::Positive;
    }
    return result;
}

// ---------------------------------------------------------------------------
// Comparison operators
// ---------------------------------------------------------------------------
bool operator==(const BigInt& lhs, const BigInt& rhs) {
    return lhs.sign_ == rhs.sign_ && lhs.limbs_ == rhs.limbs_;
}

std::strong_ordering operator<=>(const BigInt& lhs, const BigInt& rhs) {
    if (lhs.is_zero() && rhs.is_zero()) return std::strong_ordering::equal;
    if (lhs.sign_ != rhs.sign_) {
        return lhs.sign_ == Sign::Positive ? std::strong_ordering::greater : std::strong_ordering::less;
    }
    int mag = lhs.compare_magnitude(rhs);
    if (mag == 0) return std::strong_ordering::equal;
    if (lhs.sign_ == Sign::Positive) {
        return mag < 0 ? std::strong_ordering::less : std::strong_ordering::greater;
    }
    return mag > 0 ? std::strong_ordering::less : std::strong_ordering::greater;
}

// ---------------------------------------------------------------------------
// Addition / subtraction on magnitudes
// ---------------------------------------------------------------------------
BigInt BigInt::add_magnitude(const BigInt& rhs) const {
    const std::vector<uint32_t>& a = limbs_;
    const std::vector<uint32_t>& b = rhs.limbs_;
    size_t n = std::max(a.size(), b.size());
    std::vector<uint32_t> result;
    result.reserve(n + 1);
    uint64_t carry = 0;
    for (size_t i = 0; i < n; ++i) {
        uint64_t av = i < a.size() ? a[i] : 0;
        uint64_t bv = i < b.size() ? b[i] : 0;
        uint64_t sum = av + bv + carry;
        result.push_back(static_cast<uint32_t>(sum & BASE_MASK));
        carry = sum >> 32;
    }
    if (carry) {
        result.push_back(static_cast<uint32_t>(carry));
    }
    return BigInt(Sign::Positive, std::move(result));
}

BigInt BigInt::sub_magnitude(const BigInt& rhs) const {
    assert(compare_magnitude(rhs) >= 0);
    const std::vector<uint32_t>& a = limbs_;
    const std::vector<uint32_t>& b = rhs.limbs_;
    std::vector<uint32_t> result;
    result.reserve(a.size());
    int64_t borrow = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        int64_t av = static_cast<int64_t>(a[i]);
        int64_t bv = i < b.size() ? static_cast<int64_t>(b[i]) : 0;
        int64_t diff = av - bv - borrow;
        if (diff < 0) {
            diff += BASE;
            borrow = 1;
        } else {
            borrow = 0;
        }
        result.push_back(static_cast<uint32_t>(diff));
    }
    assert(borrow == 0);
    return BigInt(Sign::Positive, std::move(result));
}

// ---------------------------------------------------------------------------
// Addition / subtraction
// ---------------------------------------------------------------------------
BigInt BigInt::operator+(const BigInt& rhs) const {
    if (is_zero()) return rhs;
    if (rhs.is_zero()) return *this;

    if (sign_ == rhs.sign_) {
        BigInt result = add_magnitude(rhs);
        result.sign_ = sign_;
        return result;
    }

    int cmp = compare_magnitude(rhs);
    if (cmp == 0) return BigInt();
    if (cmp > 0) {
        BigInt result = sub_magnitude(rhs);
        result.sign_ = sign_;
        return result;
    }
    BigInt result = rhs.sub_magnitude(*this);
    result.sign_ = rhs.sign_;
    return result;
}

BigInt BigInt::operator-(const BigInt& rhs) const {
    return *this + rhs.negate();
}

BigInt& BigInt::operator+=(const BigInt& rhs) {
    *this = *this + rhs;
    return *this;
}

BigInt& BigInt::operator-=(const BigInt& rhs) {
    *this = *this - rhs;
    return *this;
}

BigInt& BigInt::operator++() {
    *this += BigInt(1);
    return *this;
}

BigInt BigInt::operator++(int) {
    BigInt tmp(*this);
    ++(*this);
    return tmp;
}

BigInt& BigInt::operator--() {
    *this -= BigInt(1);
    return *this;
}

BigInt BigInt::operator--(int) {
    BigInt tmp(*this);
    --(*this);
    return tmp;
}

// ---------------------------------------------------------------------------
// Multiplication
// ---------------------------------------------------------------------------
BigInt BigInt::operator*(const BigInt& rhs) const {
    if (is_zero() || rhs.is_zero()) return BigInt();

    auto strategy = default_multiplier();
    std::vector<uint32_t> mag = strategy->multiply(limbs_, rhs.limbs_);
    Sign result_sign = (sign_ == rhs.sign_) ? Sign::Positive : Sign::Negative;
    return BigInt(result_sign, std::move(mag));
}

BigInt& BigInt::operator*=(const BigInt& rhs) {
    *this = *this * rhs;
    return *this;
}

// ---------------------------------------------------------------------------
// Bit shifts
// ---------------------------------------------------------------------------
BigInt BigInt::operator<<(uint64_t shift) const {
    if (is_zero() || shift == 0) return *this;
    uint64_t limb_shift = shift / 32;
    uint64_t bit_shift = shift % 32;
    std::vector<uint32_t> result(limbs_.size() + limb_shift + 1, 0);
    uint64_t carry = 0;
    for (size_t i = 0; i < limbs_.size(); ++i) {
        uint64_t v = (static_cast<uint64_t>(limbs_[i]) << bit_shift) | carry;
        result[i + limb_shift] = static_cast<uint32_t>(v & BASE_MASK);
        carry = v >> 32;
    }
    if (carry) {
        result.back() = static_cast<uint32_t>(carry);
    } else {
        result.pop_back();
    }
    return BigInt(sign_, std::move(result));
}

BigInt BigInt::operator>>(uint64_t shift) const {
    if (is_zero() || shift == 0) return *this;
    uint64_t limb_shift = shift / 32;
    if (limb_shift >= limbs_.size()) return BigInt();
    uint64_t bit_shift = shift % 32;
    std::vector<uint32_t> result(limbs_.size() - limb_shift);
    uint32_t carry = 0;
    if (bit_shift != 0) {
        uint64_t inv_shift = 32 - bit_shift;
        for (size_t i = limbs_.size(); i-- > limb_shift;) {
            uint32_t cur = limbs_[i];
            result[i - limb_shift] = (cur >> bit_shift) | carry;
            carry = cur << inv_shift;
        }
    } else {
        for (size_t i = limbs_.size(); i-- > limb_shift;) {
            result[i - limb_shift] = limbs_[i];
        }
    }
    BigInt r(sign_, std::move(result));
    if (r.is_zero()) r.sign_ = Sign::Positive;
    return r;
}

// ---------------------------------------------------------------------------
// Division and modulo
// ---------------------------------------------------------------------------
// Long division: returns {quotient, remainder} for magnitudes, |this| / |rhs|.
// Implements Knuth's Algorithm D with explicit (n+1)-digit product handling.
// Assumes rhs is non-zero.
static std::pair<BigInt, BigInt> divide_magnitude(const BigInt& dividend, const BigInt& divisor) {
    if (dividend.compare_magnitude(divisor) < 0) {
        return {BigInt(), dividend};
    }

    const auto& d = divisor.limbs();
    const auto& u = dividend.limbs();

    // Normalize so that d.back() >= BASE/2.
    uint32_t norm_shift = 0;
    uint32_t top = d.back();
    while (top < (1u << 31)) {
        top <<= 1;
        ++norm_shift;
    }

    auto shl = [](const std::vector<uint32_t>& v, uint32_t shift) {
        if (shift == 0) return v;
        std::vector<uint32_t> r(v.size() + 1, 0);
        uint32_t carry = 0;
        for (size_t i = 0; i < v.size(); ++i) {
            uint64_t w = (static_cast<uint64_t>(v[i]) << shift) | carry;
            r[i] = static_cast<uint32_t>(w & BASE_MASK);
            carry = static_cast<uint32_t>(w >> 32);
        }
        if (carry) r.back() = carry;
        else r.pop_back();
        return r;
    };

    std::vector<uint32_t> vn = shl(d, norm_shift);
    std::vector<uint32_t> un = shl(u, norm_shift);

    size_t n = vn.size();
    size_t m = un.size() - n;        // quotient has m+1 digits
    un.resize(n + m + 1, 0);         // leading zero for Knuth Algorithm D

    std::vector<uint32_t> q(m + 1, 0);
    uint64_t v_top = vn.back();
    uint64_t v_next = (n >= 2) ? vn[n - 2] : 0;

    std::vector<uint64_t> prod(n + 1);
    for (size_t j = m + 1; j-- > 0;) {
        // Estimate quotient digit.
        uint64_t u_top = (static_cast<uint64_t>(un[j + n]) << 32) | un[j + n - 1];
        uint64_t qhat = u_top / v_top;
        uint64_t rhat = u_top % v_top;

        while (qhat == BASE ||
               (v_next > 0 && qhat * v_next > (rhat << 32) + un[j + n - 2])) {
            --qhat;
            rhat += v_top;
            if (rhat >= BASE) break;
        }

        // Compute qhat * vn as an (n+1)-digit product.
        uint64_t carry = 0;
        for (size_t i = 0; i < n; ++i) {
            uint64_t p = qhat * vn[i] + carry;
            prod[i] = p & BASE_MASK;
            carry = p >> 32;
        }
        prod[n] = carry;

        // Subtract product from un[j..j+n].
        int64_t borrow = 0;
        for (size_t i = 0; i <= n; ++i) {
            int64_t t = static_cast<int64_t>(un[j + i]) - static_cast<int64_t>(prod[i]) - borrow;
            borrow = 0;
            if (t < 0) {
                t += BASE;
                borrow = 1;
            }
            un[j + i] = static_cast<uint32_t>(t);
        }

        // Add back if we underflowed.
        if (borrow) {
            --qhat;
            uint32_t add_carry = 0;
            for (size_t i = 0; i <= n; ++i) {
                uint64_t addend = (i < n) ? vn[i] : 0;
                uint64_t sum = static_cast<uint64_t>(un[j + i]) + addend + add_carry;
                un[j + i] = static_cast<uint32_t>(sum & BASE_MASK);
                add_carry = static_cast<uint32_t>(sum >> 32);
            }
            // After a valid add-back there must be no final carry.
        }

        q[j] = static_cast<uint32_t>(qhat);
    }

    BigInt quotient(Sign::Positive, std::move(q));

    // Remainder is un[0..n-1] shifted right by norm_shift.
    auto shr = [](std::vector<uint32_t> v, uint32_t shift) {
        if (shift == 0) {
            while (!v.empty() && v.back() == 0) v.pop_back();
            return v;
        }
        uint32_t inv_shift = 32 - shift;
        uint32_t carry = 0;
        for (size_t i = v.size(); i-- > 0;) {
            uint32_t cur = v[i];
            v[i] = (cur >> shift) | carry;
            carry = cur << inv_shift;
        }
        while (!v.empty() && v.back() == 0) v.pop_back();
        return v;
    };
    std::vector<uint32_t> rem(un.begin(), un.begin() + n);
    BigInt remainder(Sign::Positive, shr(rem, norm_shift));
    return {quotient, remainder};
}

expected<BigInt, BigIntError> BigInt::operator/(const BigInt& rhs) const {
    if (rhs.is_zero()) return BigIntError::DivisionByZero;
    if (is_zero()) return BigInt();

    auto [quot, rem] = divide_magnitude(this->abs(), rhs.abs());
    if (!quot.is_zero()) {
        quot.sign_ = (sign_ == rhs.sign_) ? Sign::Positive : Sign::Negative;
    }
    return quot;
}

expected<BigInt, BigIntError> BigInt::operator%(const BigInt& rhs) const {
    if (rhs.is_zero()) return BigIntError::DivisionByZero;
    if (is_zero()) return BigInt();

    auto [quot, rem] = divide_magnitude(this->abs(), rhs.abs());
    rem.sign_ = sign_; // sign of remainder matches dividend in C++ truncated division
    if (rem.is_zero()) rem.sign_ = Sign::Positive;
    return rem;
}

expected<BigInt, BigIntError> BigInt::operator/=(const BigInt& rhs) {
    auto res = *this / rhs;
    if (!res) return res.error();
    *this = std::move(*res);
    return *this;
}

expected<BigInt, BigIntError> BigInt::operator%=(const BigInt& rhs) {
    auto res = *this % rhs;
    if (!res) return res.error();
    *this = std::move(*res);
    return *this;
}

// ---------------------------------------------------------------------------
// Power
// ---------------------------------------------------------------------------
BigInt BigInt::pow(const BigInt& base, uint64_t exp) {
    BigInt result(1);
    BigInt cur(base);
    while (exp > 0) {
        if (exp & 1) result *= cur;
        cur *= cur;
        exp >>= 1;
    }
    return result;
}

// ---------------------------------------------------------------------------
// Factorial using binary splitting
// ---------------------------------------------------------------------------
static BigInt factorial_range(uint64_t a, uint64_t b) {
    if (a >= b) return BigInt(1);
    if (b - a == 1) return BigInt(static_cast<int64_t>(a));
    uint64_t m = a + (b - a) / 2;
    return factorial_range(a, m) * factorial_range(m, b);
}

BigInt BigInt::factorial(uint64_t n) {
    if (n <= 1) return BigInt(1);
    return factorial_range(1, n + 1);
}

// ---------------------------------------------------------------------------
// Integer square root using Newton's method for the reciprocal square root.
// ---------------------------------------------------------------------------
expected<BigInt, BigIntError> BigInt::sqrt(const BigInt& n) {
    if (n.is_negative()) return BigIntError::NegativeSqrt;
    if (n.is_zero() || n == BigInt(1)) return n;

    // For small values, compute directly using 64-bit floating point.
    if (n.limbs().size() <= 2) {
        uint64_t v = n.limbs()[0];
        if (n.limbs().size() > 1) {
            v |= static_cast<uint64_t>(n.limbs()[1]) << 32;
        }
        uint64_t r = static_cast<uint64_t>(std::sqrt(static_cast<long double>(v)));
        while ((r + 1) <= std::numeric_limits<uint64_t>::max() / (r + 1) &&
               (r + 1) * (r + 1) <= v) {
            ++r;
        }
        while (r > 0 && r > std::numeric_limits<uint64_t>::max() / r) {
            --r;
        }
        while (r > 0 && r * r > v) {
            --r;
        }
        return BigInt(static_cast<int64_t>(r));
    }

    // Number of bits in n.
    uint64_t m = static_cast<uint64_t>(n.limbs().size() - 1) * 32 +
                 (32 - std::countl_zero(n.limbs().back()));

    // Target precision.  We want floor(sqrt(n)) which has ceil(m/2) bits.
    // Use p = m + guard_bits so that the scaled reciprocal has enough precision.
    constexpr uint64_t kGuardBits = 64;
    uint64_t p = m + kGuardBits;

    // Initial approximation: Y0 = 2^(p - ceil(m/2)).
    // Since sqrt(n) ~ 2^(m/2), this is within a constant factor of 2^p / sqrt(n).
    uint64_t y0_bits = p - (m + 1) / 2;
    BigInt Y = power_of_two(y0_bits);

    // Constants used in the iteration: 2^p and 3 * 2^(2p).
    BigInt two_p = power_of_two(p);
    BigInt three_two_2p = two_p * two_p * BigInt(3);

    // Newton iteration for the scaled reciprocal square root:
    // Y_{k+1} = Y_k * (3*2^(2p) - N*Y_k^2) / 2^(2p+1)
    // This doubles the number of correct bits each iteration.
    BigInt Y_next;
    for (int iter = 0; iter < 64; ++iter) {
        BigInt Y2 = Y * Y;
        BigInt NY2 = n * Y2;
        BigInt diff = three_two_2p - NY2;
        // diff should be positive; if it underflows due to a transient
        // overshoot, stop and use the previous value.
        if (diff.is_negative()) break;
        BigInt num = Y * diff;
        Y_next = num >> (2 * p + 1);
        if (Y_next == Y) break;
        Y = std::move(Y_next);
    }

    // S = floor(N * Y / 2^p)
    BigInt S = (n * Y) >> p;

    // Adjust to ensure S^2 <= N < (S+1)^2.
    BigInt one(1);
    while (true) {
        BigInt sp1 = S + one;
        BigInt sq = sp1 * sp1;
        if (sq <= n) {
            S = std::move(sp1);
        } else {
            break;
        }
    }
    while (true) {
        BigInt sq = S * S;
        if (sq > n) {
            S = S - one;
        } else {
            break;
        }
    }

    return S;
}

// ---------------------------------------------------------------------------
// Decimal string conversion
// ---------------------------------------------------------------------------
namespace {

std::string to_string_recursive(const BigInt& x, size_t k,
                                const std::vector<BigInt>& pow10) {
    if (x.is_zero()) return "0";
    if (k == 0) {
        // x < 10^18.  Split into high and low 9-digit chunks.
        constexpr uint64_t chunk = 1'000'000'000ull;
        auto q_res = x / BigInt(chunk);
        uint64_t q = 0;
        if (q_res) {
            q = q_res->limbs().empty() ? 0 : q_res->limbs()[0];
            if (q_res->limbs().size() > 1) {
                q |= static_cast<uint64_t>(q_res->limbs()[1]) << 32;
            }
        }
        auto r_res = x % BigInt(chunk);
        uint64_t r = 0;
        if (r_res) {
            r = r_res->limbs().empty() ? 0 : r_res->limbs()[0];
        }
        std::ostringstream oss;
        if (q != 0) oss << q;
        oss.width(9);
        oss.fill('0');
        oss << r;
        std::string s = oss.str();
        // Strip leading zeros, but keep at least one digit.
        size_t pos = 0;
        while (pos + 1 < s.size() && s[pos] == '0') ++pos;
        return s.substr(pos);
    }

    auto q_res = x / pow10[k];
    auto r_res = x % pow10[k];
    std::string high = to_string_recursive(*q_res, k - 1, pow10);
    std::string low = to_string_recursive(*r_res, k - 1, pow10);
    if (high == "0") return low;
    size_t expected_low_digits = 9ull << k; // 9 * 2^k
    if (low.size() < expected_low_digits) {
        low = std::string(expected_low_digits - low.size(), '0') + low;
    }
    return high + low;
}

} // namespace

std::string BigInt::to_string() const {
    if (is_zero()) return "0";

    // Precompute powers of 10^9: pow10[k] = 10^(9 * 2^k).
    // Use a static cache so repeated calls reuse the work.
    static std::vector<BigInt> pow10_cache = { BigInt(1'000'000'000) };
    {
        BigInt limit = abs();
        // Ensure the cache contains a power strictly larger than |this|.
        while (pow10_cache.back().compare_magnitude(limit) <= 0) {
            pow10_cache.push_back(pow10_cache.back() * pow10_cache.back());
        }
    }

    size_t k = pow10_cache.size() - 1;
    // Choose the largest k such that pow10_cache[k] <= |this|.
    while (k > 0 && abs().compare_magnitude(pow10_cache[k]) < 0) {
        --k;
    }

    std::string s = to_string_recursive(abs(), k, pow10_cache);
    if (sign_ == Sign::Negative) s = "-" + s;
    return s;
}

// ---------------------------------------------------------------------------
// Arbitrary radix conversion
// ---------------------------------------------------------------------------
std::string BigInt::to_string_radix(int radix) const {
    if (radix < 2 || radix > 36) {
        throw std::invalid_argument("radix must be between 2 and 36");
    }
    if (is_zero()) return "0";

    const char* alphabet = "0123456789abcdefghijklmnopqrstuvwxyz";
    BigInt tmp(abs());
    BigInt base(static_cast<int64_t>(radix));
    std::string result;
    while (!tmp.is_zero()) {
        auto rem_res = tmp % base;
        if (!rem_res) break;
        uint32_t digit = (*rem_res).limbs_.empty() ? 0 : (*rem_res).limbs_[0];
        result.push_back(alphabet[digit]);
        auto div_res = tmp / base;
        if (!div_res) break;
        tmp = std::move(*div_res);
    }
    if (sign_ == Sign::Negative) result.push_back('-');
    std::reverse(result.begin(), result.end());
    return result;
}

std::ostream& operator<<(std::ostream& os, const BigInt& value) {
    os << value.to_string();
    return os;
}

} // namespace cpi
