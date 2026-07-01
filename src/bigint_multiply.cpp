// Copyright (c) 2026 Calculate Pi project
// SPDX-License-Identifier: MIT

#include "cpi/bigint.hpp"

#include <algorithm>
#include <complex>
#include <cmath>
#include <numbers>
#include <vector>

namespace cpi {

namespace {

constexpr uint64_t BASE = 1ULL << 32;
constexpr uint64_t BASE_MASK = BASE - 1;

// Normalize a magnitude vector in-place.
void normalize_vector(std::vector<uint32_t>& v) {
    while (!v.empty() && v.back() == 0) v.pop_back();
}

// Add two magnitude vectors.
std::vector<uint32_t> add_vectors(const std::vector<uint32_t>& a,
                                    const std::vector<uint32_t>& b) {
    size_t n = std::max(a.size(), b.size());
    std::vector<uint32_t> r;
    r.reserve(n + 1);
    uint64_t carry = 0;
    for (size_t i = 0; i < n; ++i) {
        uint64_t av = i < a.size() ? a[i] : 0;
        uint64_t bv = i < b.size() ? b[i] : 0;
        uint64_t sum = av + bv + carry;
        r.push_back(static_cast<uint32_t>(sum & BASE_MASK));
        carry = sum >> 32;
    }
    if (carry) r.push_back(static_cast<uint32_t>(carry));
    return r;
}

// Subtract smaller magnitude from larger (a >= b).
std::vector<uint32_t> sub_vectors(const std::vector<uint32_t>& a,
                                    const std::vector<uint32_t>& b) {
    std::vector<uint32_t> r;
    r.reserve(a.size());
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
        r.push_back(static_cast<uint32_t>(diff));
    }
    normalize_vector(r);
    return r;
}

} // namespace

// ---------------------------------------------------------------------------
// Naive multiplication
// ---------------------------------------------------------------------------
std::vector<uint32_t> NaiveMultiplier::multiply(
    const std::vector<uint32_t>& a,
    const std::vector<uint32_t>& b) const {
    if (a.empty() || b.empty()) return {};
    std::vector<uint64_t> temp(a.size() + b.size(), 0);
    for (size_t i = 0; i < a.size(); ++i) {
        uint64_t carry = 0;
        for (size_t j = 0; j < b.size() || carry; ++j) {
            uint64_t p = temp[i + j] + carry;
            if (j < b.size()) {
                p += static_cast<uint64_t>(a[i]) * b[j];
            }
            temp[i + j] = p & BASE_MASK;
            carry = p >> 32;
        }
    }
    std::vector<uint32_t> result;
    result.reserve(temp.size());
    for (auto v : temp) result.push_back(static_cast<uint32_t>(v));
    normalize_vector(result);
    return result;
}

// ---------------------------------------------------------------------------
// Karatsuba multiplication
// ---------------------------------------------------------------------------
std::vector<uint32_t> KaratsubaMultiplier::multiply(
    const std::vector<uint32_t>& a,
    const std::vector<uint32_t>& b) const {
    return karatsuba(a, b, threshold_);
}

std::vector<uint32_t> KaratsubaMultiplier::karatsuba(
    const std::vector<uint32_t>& a,
    const std::vector<uint32_t>& b,
    size_t threshold) {
    if (a.empty() || b.empty()) return {};
    if (std::min(a.size(), b.size()) < threshold) {
        NaiveMultiplier naive;
        return naive.multiply(a, b);
    }

    size_t n = std::max(a.size(), b.size());
    size_t m = n / 2;

    // Split a into a0 (low) and a1 (high)
    std::vector<uint32_t> a0(a.begin(), a.begin() + std::min(m, a.size()));
    std::vector<uint32_t> a1(a.begin() + std::min(m, a.size()), a.end());
    std::vector<uint32_t> b0(b.begin(), b.begin() + std::min(m, b.size()));
    std::vector<uint32_t> b1(b.begin() + std::min(m, b.size()), b.end());

    normalize_vector(a0); normalize_vector(a1);
    normalize_vector(b0); normalize_vector(b1);

    std::vector<uint32_t> z0 = karatsuba(a0, b0, threshold);
    std::vector<uint32_t> z2 = karatsuba(a1, b1, threshold);

    std::vector<uint32_t> a0a1 = add_vectors(a0, a1);
    std::vector<uint32_t> b0b1 = add_vectors(b0, b1);
    std::vector<uint32_t> z1 = karatsuba(a0a1, b0b1, threshold);
    z1 = sub_vectors(z1, z0);
    z1 = sub_vectors(z1, z2);

    // result = z0 + z1 * B^m + z2 * B^(2m)
    std::vector<uint32_t> result(a.size() + b.size(), 0);
    auto add_at = [&](size_t offset, const std::vector<uint32_t>& v) {
        uint64_t carry = 0;
        for (size_t i = 0; i < v.size() || carry; ++i) {
            uint64_t sum = carry;
            if (i < v.size()) sum += v[i];
            if (offset + i < result.size()) sum += result[offset + i];
            if (offset + i >= result.size()) break;
            result[offset + i] = static_cast<uint32_t>(sum & BASE_MASK);
            carry = sum >> 32;
        }
    };
    add_at(0, z0);
    add_at(m, z1);
    add_at(2 * m, z2);
    normalize_vector(result);
    return result;
}

// ---------------------------------------------------------------------------
// FFT multiplication
// ---------------------------------------------------------------------------
namespace {

using Complex = std::complex<double>;

// Iterative FFT. invert = false for forward, true for inverse.
void fft(std::vector<Complex>& a, bool invert) {
    size_t n = a.size();
    if (n <= 1) return;

    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }

    for (size_t len = 2; len <= n; len <<= 1) {
        double ang = 2 * std::numbers::pi / static_cast<double>(len) * (invert ? -1 : 1);
        Complex wlen(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            Complex w(1);
            for (size_t j = 0; j < len / 2; ++j) {
                Complex u = a[i + j];
                Complex v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }

    if (invert) {
        for (auto& x : a) x /= static_cast<double>(n);
    }
}

} // namespace

std::vector<uint32_t> FFTMultiplier::multiply(
    const std::vector<uint32_t>& a,
    const std::vector<uint32_t>& b) const {
    return fft_multiply(a, b);
}

std::vector<uint32_t> FFTMultiplier::fft_multiply(
    const std::vector<uint32_t>& a,
    const std::vector<uint32_t>& b) {
    if (a.empty() || b.empty()) return {};
    if (std::min(a.size(), b.size()) < 64) {
        NaiveMultiplier naive;
        return naive.multiply(a, b);
    }

    // Split each 32-bit limb into 4 base-2^8 digits.  This makes the
    // base conversion trivial (4 bytes per limb) and keeps FFT coefficient
    // magnitudes well below the 53-bit double precision threshold.
    constexpr size_t DIGITS_PER_LIMB = 4;
    constexpr uint32_t MASK = 0xFFu;

    size_t need = (a.size() + b.size()) * DIGITS_PER_LIMB;
    size_t n = 1;
    while (n < need) n <<= 1;

    std::vector<Complex> fa(n);
    for (size_t i = 0; i < a.size(); ++i) {
        fa[DIGITS_PER_LIMB * i] = static_cast<double>(a[i] & MASK);
        fa[DIGITS_PER_LIMB * i + 1] = static_cast<double>((a[i] >> 8) & MASK);
        fa[DIGITS_PER_LIMB * i + 2] = static_cast<double>((a[i] >> 16) & MASK);
        fa[DIGITS_PER_LIMB * i + 3] = static_cast<double>((a[i] >> 24) & MASK);
    }

    std::vector<Complex> fb(n);
    for (size_t i = 0; i < b.size(); ++i) {
        fb[DIGITS_PER_LIMB * i] = static_cast<double>(b[i] & MASK);
        fb[DIGITS_PER_LIMB * i + 1] = static_cast<double>((b[i] >> 8) & MASK);
        fb[DIGITS_PER_LIMB * i + 2] = static_cast<double>((b[i] >> 16) & MASK);
        fb[DIGITS_PER_LIMB * i + 3] = static_cast<double>((b[i] >> 24) & MASK);
    }

    fft(fa, false);
    fft(fb, false);
    for (size_t i = 0; i < n; ++i) fa[i] *= fb[i];
    fft(fa, true);

    // Carry propagation in base 2^8.
    std::vector<uint32_t> digits(need + 1, 0);
    uint32_t carry = 0;
    for (size_t i = 0; i < digits.size(); ++i) {
        uint64_t cur = carry;
        if (i < n) {
            cur += static_cast<uint64_t>(std::llround(fa[i].real()));
        }
        digits[i] = static_cast<uint32_t>(cur & MASK);
        carry = static_cast<uint32_t>(cur >> 8);
    }

    // Convert base-2^8 digits back to base-2^32 limbs (4 digits per limb).
    std::vector<uint32_t> result;
    result.reserve(a.size() + b.size() + 1);
    size_t i = 0;
    while (i < digits.size()) {
        uint32_t d0 = digits[i++];
        uint32_t d1 = (i < digits.size()) ? digits[i++] : 0;
        uint32_t d2 = (i < digits.size()) ? digits[i++] : 0;
        uint32_t d3 = (i < digits.size()) ? digits[i++] : 0;
        result.push_back(d0 | (d1 << 8) | (d2 << 16) | (d3 << 24));
    }
    normalize_vector(result);
    return result;
}

// ---------------------------------------------------------------------------
// Adaptive multiplier
// ---------------------------------------------------------------------------
AdaptiveMultiplier::AdaptiveMultiplier(size_t karatsuba_threshold,
                                       size_t fft_threshold)
    : naive_(std::make_unique<NaiveMultiplier>()),
      karatsuba_(std::make_unique<KaratsubaMultiplier>(karatsuba_threshold)),
      fft_(std::make_unique<FFTMultiplier>(fft_threshold)),
      karatsuba_threshold_(karatsuba_threshold),
      fft_threshold_(fft_threshold) {}

std::vector<uint32_t> AdaptiveMultiplier::multiply(
    const std::vector<uint32_t>& a,
    const std::vector<uint32_t>& b) const {
    size_t mn = std::min(a.size(), b.size());
    if (mn < karatsuba_threshold_) {
        return naive_->multiply(a, b);
    }
    if (mn < fft_threshold_) {
        return karatsuba_->multiply(a, b);
    }
    return fft_->multiply(a, b);
}

} // namespace cpi
