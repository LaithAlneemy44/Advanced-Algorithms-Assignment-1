#pragma once

#include <cstddef>
#include <cstdint>

namespace swiss {

// The H1/H2 split takes bits from the whole 64-bit hash.
static_assert(sizeof(std::size_t) == 8, "a 64-bit size_t is required");

// ---------------------------------------------------------------------------
// H1/H2 split: top 7 bits, as in current Abseil (since release 20250814) and
// hashbrown. Defined only here, so the table and the analysis tools agree.
// With a hash that is good in every bit, which 7 bits H2 takes makes no
// difference to quality. Taking the top bits keeps a shift off the path from
// hash to the first memory load.
// ---------------------------------------------------------------------------

// H2: the 7 most significant bits, stored in a full slot's control byte.
constexpr std::uint8_t h2(std::size_t hash) noexcept {
    return static_cast<std::uint8_t>(hash >> 57);
}

// H1: the whole hash. Masked to the capacity, it is the slot where probing starts.
constexpr std::size_t h1(std::size_t hash, std::size_t capacity_mask) noexcept {
    return hash & capacity_mask;
}

// ---------------------------------------------------------------------------
// splitmix64
// ---------------------------------------------------------------------------

namespace detail {

inline constexpr std::uint64_t kSplitMixMul1 = 0xBF58476D1CE4E5B9ULL;
inline constexpr std::uint64_t kSplitMixMul2 = 0x94D049BB133111EBULL;

// Inverse of y = x ^ (x >> s). The step leaves the top s bits of x unchanged,
// so they can be read straight from y. Each iteration then recovers the next s
// bits below the ones already known.
constexpr std::uint64_t unxorshift(std::uint64_t y, int s) noexcept {
    std::uint64_t x = y;
    for (int known = s; known < 64; known += s) {
        x = y ^ (x >> s);
    }
    return x;
}

// Inverse of an odd constant mod 2^64 by Newton's iteration. Starting from
// inv = c is correct to 3 bits, since c * c == 1 mod 8 for any odd c. Each step
// doubles the number of correct bits: 3, 6, 12, 24, 48, 96.
constexpr std::uint64_t inverse_mod_2_64(std::uint64_t c) noexcept {
    std::uint64_t inv = c;
    for (int i = 0; i < 5; ++i) {
        inv *= 2 - c * inv;
    }
    return inv;
}

}  // namespace detail

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
    x = (x ^ (x >> 30)) * detail::kSplitMixMul1;
    x = (x ^ (x >> 27)) * detail::kSplitMixMul2;
    return x ^ (x >> 31);
}

// Undoes splitmix64_mix, step by step in reverse order. Used to prove the
// bijection in tests and to build keys with chosen hashes, such as keys that
// all collide.
constexpr std::uint64_t splitmix64_unmix(std::uint64_t z) noexcept {
    z = detail::unxorshift(z, 31);
    z = detail::unxorshift(z * detail::inverse_mod_2_64(detail::kSplitMixMul2), 27);
    z = detail::unxorshift(z * detail::inverse_mod_2_64(detail::kSplitMixMul1), 30);
    return z;
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
