#pragma once

#include <cstddef>

// Growth policy shared by every open-addressing table in the study. R0, R1 and
// R2 use exactly these functions, so the steps differ only in the feature being
// measured, never in when or how they resize.
namespace swiss::growth {

// One SSE2 group. R0 uses the same minimum for parity.
inline constexpr std::size_t kMinCapacity = 16;

// Maximum load 7/8, counting tombstones as well as live keys. A tombstone
// lengthens probes exactly as a live key does, so it has to count towards the
// limit. At least 1/8 of the slots therefore stay EMPTY, which is what makes
// every probe loop terminate.
constexpr std::size_t max_used(std::size_t capacity) noexcept {
    return capacity - capacity / 8;
}

// Smallest capacity that holds n keys within the load limit.
constexpr std::size_t capacity_for(std::size_t n) noexcept {
    std::size_t capacity = kMinCapacity;
    while (max_used(capacity) < n) {
        capacity *= 2;
    }
    return capacity;
}

// Called when an insert would exceed the load limit. `live` counts the keys
// after the insert. If at most half the slots will hold live keys, most of the
// used slots are tombstones, so rebuild at the same capacity to clear them.
// Otherwise double. In both cases at least 3/8 of the capacity is free for new
// keys afterwards, so rehashing costs amortised O(1) per insert.
constexpr std::size_t next_capacity(std::size_t capacity, std::size_t live) noexcept {
    if (capacity == 0) {
        return kMinCapacity;
    }
    return live <= capacity / 2 ? capacity : capacity * 2;
}

}  // namespace swiss::growth
