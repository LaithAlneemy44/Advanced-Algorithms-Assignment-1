#pragma once

#include <swiss/hash.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

// Hashes compared in the hash-quality experiment, and only there. Every other
// comparison uses swiss::SplitMix64Hash, which is also the reference here.
//
// All are stateless and noexcept, for the same std::unordered_map layout reason
// as SplitMix64Hash. Each one is weak in a different, predictable place.
namespace swiss::candidates {

// Returns the key, as libstdc++'s std::hash does for integers. The hash varies
// only where the keys vary. Small keys have all-zero top bits, so H2 = 0.
struct Identity {
    constexpr std::size_t operator()(std::uint64_t key) const noexcept {
        return key;
    }
};

// floor(2^64 / golden ratio). It is odd, so multiplying by it is a bijection
// mod 2^64 and loses no key bits.
inline constexpr std::uint64_t kFibonacci = 0x9E3779B97F4A7C15ULL;

// Low 64 bits of key * C. Carries only move upward, so output bit i depends
// only on key bits 0 to i. Top bits are strong and low bits are weak: keys that
// are multiples of 2^j give products whose low j bits are all 0.
struct Multiplicative {
    constexpr std::size_t operator()(std::uint64_t key) const noexcept {
        return key * kFibonacci;
    }
};

#if !defined(__SIZEOF_INT128__)
#error "FoldedMultiply needs unsigned __int128, available in GCC and Clang"
#endif

// __extension__ silences -Wpedantic, since __int128 is a GCC and Clang extension.
__extension__ using uint128 = unsigned __int128;

// The same product at full 128 bits, high half xor low half. The high half
// depends on every key bit through the carries, so the fold repairs the weak
// low bits. It shares the multiplier with Multiplicative, so the fold is the
// only difference between the two. Not invertible.
struct FoldedMultiply {
    constexpr std::size_t operator()(std::uint64_t key) const noexcept {
        const uint128 product = static_cast<uint128>(key) * kFibonacci;
        return static_cast<std::uint64_t>(product >> 64) ^ static_cast<std::uint64_t>(product);
    }
};

namespace detail {

using TabulationTables = std::array<std::array<std::uint64_t, 256>, 8>;

// One table of 256 random words per key byte. Filled at compile time from the
// splitmix64 generator with seed 0, so the functor is stateless and every run
// uses the same tables.
constexpr TabulationTables make_tabulation_tables() noexcept {
    TabulationTables tables{};
    std::uint64_t state = 0;
    for (auto& table : tables) {
        for (auto& entry : table) {
            state += 0x9E3779B97F4A7C15ULL;  // the generator's counter step
            entry = splitmix64_mix(state);
        }
    }
    return tables;
}

inline constexpr TabulationTables kTabulationTables = make_tabulation_tables();

}  // namespace detail

// Simple tabulation (Zobrist 1970). Look up each key byte in its own table and
// xor the results. It is only 3-independent, yet Patrascu and Thorup (STOC 2011)
// proved it gives O(1) expected probes for linear probing, where 3-independence
// alone would not be enough. The tables take 16 KB, half of this machine's L1d.
struct SimpleTabulation {
    constexpr std::size_t operator()(std::uint64_t key) const noexcept {
        std::uint64_t hash = 0;
        for (std::size_t byte = 0; byte < 8; ++byte) {
            hash ^= detail::kTabulationTables[byte][(key >> (8 * byte)) & 0xFF];
        }
        return hash;
    }
};

}  // namespace swiss::candidates
