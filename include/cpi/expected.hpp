// Copyright (c) 2026 Calculate Pi project
// SPDX-License-Identifier: MIT

#pragma once

#include <stdexcept>
#include <variant>

namespace cpi {

// Minimal std::expected polyfill for C++20.
// Mirrors a subset of std::expected sufficient for this project.
template <typename T, typename E>
class expected {
public:
    expected(T value) : data_(std::move(value)) {}
    expected(E error) : data_(std::move(error)) {}

    expected(const expected&) = default;
    expected(expected&&) = default;
    expected& operator=(const expected&) = default;
    expected& operator=(expected&&) = default;
    ~expected() = default;

    [[nodiscard]] bool has_value() const noexcept { return std::holds_alternative<T>(data_); }
    explicit operator bool() const noexcept { return has_value(); }

    T& value() & {
        if (!has_value()) { throw std::runtime_error("expected contains error"); }
        return std::get<T>(data_);
    }
    const T& value() const& {
        if (!has_value()) { throw std::runtime_error("expected contains error"); }
        return std::get<T>(data_);
    }
    T&& value() && {
        if (!has_value()) { throw std::runtime_error("expected contains error"); }
        return std::get<T>(std::move(data_));
    }

    E& error() & {
        if (has_value()) { throw std::runtime_error("expected contains value"); }
        return std::get<E>(data_);
    }
    const E& error() const& {
        if (has_value()) { throw std::runtime_error("expected contains value"); }
        return std::get<E>(data_);
    }
    E&& error() && {
        if (has_value()) { throw std::runtime_error("expected contains value"); }
        return std::get<E>(std::move(data_));
    }

    T* operator->() { return &value(); }
    const T* operator->() const { return &value(); }
    T& operator*() & { return value(); }
    const T& operator*() const& { return value(); }
    T&& operator*() && { return std::move(value()); }

private:
    std::variant<T, E> data_;
};

// Specialization for void value type (e.g. expected<void, Error>).
template <typename E>
class expected<void, E> {
public:
    expected() : data_(std::monostate()) {}
    expected(E error) : data_(std::move(error)) {}

    expected(const expected&) = default;
    expected(expected&&) = default;
    expected& operator=(const expected&) = default;
    expected& operator=(expected&&) = default;
    ~expected() = default;

    [[nodiscard]] bool has_value() const noexcept {
        return std::holds_alternative<std::monostate>(data_);
    }
    explicit operator bool() const noexcept { return has_value(); }

    void value() const {
        if (!has_value()) { throw std::runtime_error("expected contains error"); }
    }

    E& error() & {
        if (has_value()) { throw std::runtime_error("expected contains value"); }
        return std::get<E>(data_);
    }
    const E& error() const& {
        if (has_value()) { throw std::runtime_error("expected contains value"); }
        return std::get<E>(data_);
    }
    E&& error() && {
        if (has_value()) { throw std::runtime_error("expected contains value"); }
        return std::get<E>(std::move(data_));
    }

private:
    std::variant<std::monostate, E> data_;
};

} // namespace cpi
