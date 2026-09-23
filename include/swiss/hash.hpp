#pragma once

#include <cstddef>
#include <cstdint>

namespace swiss {

// The H1/H2 split takes bits from the whole 64-bit hash.
static_assert(sizeof(std::size_t) == 8, "a 64-bit size_t is required");

// splitmix64 finaliser. Shifts and constants match Vigna's reference
// splitmix64.c, without the generator's counter increment.
//
// Every step is invertible. x ^= x >> s is undone by repeating it, and
// multiplying by an odd constant is undone by its inverse mod 2^64. So the map
// is a bijection: distinct keys never share a full 64-bit hash. Collisions only
// come from reducing the hash to H1 and H2.
//
// 0 is a fixed point: mix(0) == 0. Harmless for correctness.
constexpr std::uint64_t splitmix64_mix(std::uint64_t x) noexcept {
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

// The single hash used by every structure in the study. Hash quality is varied
// only in its own experiment, by swapping this template argument.
//
// noexcept is load-bearing. libstdc++'s std::unordered_map stores the hash code
// in every node unless the hasher is nothrow-invocable (__cache_default in
// bits/hashtable.h). Without noexcept the baseline's nodes grow by 8 bytes.
struct SplitMix64Hash {
    constexpr std::size_t operator()(std::uint64_t key) const noexcept {
        return static_cast<std::size_t>(splitmix64_mix(key));
    }
};

}  // namespace swiss
