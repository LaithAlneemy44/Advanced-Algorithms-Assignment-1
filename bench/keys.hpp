#pragma once

#include <swiss/hash.hpp>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <unordered_set>
#include <vector>

// Key sets for the hash-quality experiment. Uniform random keys hide hash
// quality: they are already well spread, so even the identity hash does well on
// them. The structured sets are the ones that expose weak bits.
namespace swiss::keys {

// Distinct uniform random 64-bit keys. Duplicates are skipped, so a table built
// from them really holds n keys.
inline std::vector<std::uint64_t> uniform(std::size_t n, std::uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::unordered_set<std::uint64_t> seen;
    std::vector<std::uint64_t> keys;
    keys.reserve(n);
    while (keys.size() < n) {
        const std::uint64_t key = rng();
        if (seen.insert(key).second) {
            keys.push_back(key);
        }
    }
    return keys;
}

// 0, 1, 2, ... Typical of IDs and counters.
inline std::vector<std::uint64_t> sequential(std::size_t n) {
    std::vector<std::uint64_t> keys(n);
    for (std::size_t i = 0; i < n; ++i) {
        keys[i] = i;
    }
    return keys;
}

// 0, stride, 2 * stride, ... Typical of aligned pointers and scaled IDs. The low
// log2(stride) bits of every key are 0.
inline std::vector<std::uint64_t> strided(std::size_t n, std::uint64_t stride) {
    std::vector<std::uint64_t> keys(n);
    for (std::size_t i = 0; i < n; ++i) {
        keys[i] = i * stride;
    }
    return keys;
}

// Keys whose SplitMix64Hash values all share one H2 and one starting slot in a
// table of the given capacity. Built backwards: choose the hashes, then invert.
// The middle bits hold a counter, so the hashes are distinct, and so are the
// keys, since the inverse is a bijection.
//
// These keys collide only under SplitMix64Hash. Under any other hash they are
// ordinary keys.
inline std::vector<std::uint64_t> colliding(std::size_t n, std::size_t capacity, std::uint64_t seed) {
    if (!std::has_single_bit(capacity) || capacity > (std::size_t{1} << 57)) {
        throw std::invalid_argument("capacity must be a power of two, at most 2^57");
    }
    const int slot_bits = std::countr_zero(capacity);
    const int counter_bits = 57 - slot_bits;  // the bits between H1's and H2's
    if (counter_bits < 64 && n > (std::uint64_t{1} << counter_bits)) {
        throw std::invalid_argument("too many keys for the free hash bits");
    }

    std::mt19937_64 rng(seed);
    const std::uint64_t shared_h2 = rng() & 0x7F;
    const std::uint64_t shared_slot = rng() & (capacity - 1);

    std::vector<std::uint64_t> keys(n);
    for (std::uint64_t i = 0; i < n; ++i) {
        const std::uint64_t hash = (shared_h2 << 57) | (i << slot_bits) | shared_slot;
        keys[i] = splitmix64_unmix(hash);
    }
    return keys;
}

}  // namespace swiss::keys
