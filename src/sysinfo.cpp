// Copyright (c) 2026 Calculate Pi project
// SPDX-License-Identifier: MIT

#include "cpi/sysinfo.hpp"

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/types.h>
#else
#include <sys/sysinfo.h>
#endif

namespace cpi {

std::optional<uint64_t> total_physical_memory_bytes() {
#ifdef _WIN32
    MEMORYSTATUSEX state;
    state.dwLength = sizeof(state);
    if (GlobalMemoryStatusEx(&state) == 0) {
        return std::nullopt;
    }
    return state.ullTotalPhys;
#elif defined(__APPLE__)
    uint64_t memsize = 0;
    size_t len = sizeof(memsize);
    if (sysctlbyname("hw.memsize", &memsize, &len, nullptr, 0) != 0) {
        return std::nullopt;
    }
    return memsize;
#else
    struct sysinfo info;
    if (sysinfo(&info) != 0) {
        return std::nullopt;
    }
    return static_cast<uint64_t>(info.totalram) * info.mem_unit;
#endif
}

} // namespace cpi
