// Copyright (c) 2026 Calculate Pi project
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <optional>

namespace cpi {

// Return the total physical memory of the machine in bytes.
// Returns std::nullopt if the value cannot be determined on this platform.
[[nodiscard]] std::optional<uint64_t> total_physical_memory_bytes();

} // namespace cpi
