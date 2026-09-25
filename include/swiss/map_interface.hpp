#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>

namespace swiss {

// Keys from this value up are reserved. R0 uses the top two key values as its
// empty and deleted markers. Every key set avoids them, so all tables see
// exactly the same keys.
inline constexpr std::uint64_t kFirstReservedKey = ~std::uint64_t{0} - 1;

constexpr bool is_reserved_key(std::uint64_t key) noexcept {
    return key >= kFirstReservedKey;
}

// The interface every table in the study provides: the R0, R1 and R2 steps and
// the adapters around the baselines. Tests and benchmarks are written against
// it, so every table runs exactly the same code.
template <class M>
concept Map = requires(M m, const M cm, std::uint64_t key, std::uint64_t value, std::size_t n) {
    // false if the key was already present. Its value is then left unchanged,
    // as with std::unordered_map::insert.
    { m.insert(key, value) } -> std::same_as<bool>;
    // nullptr if the key is absent.
    { cm.find(key) } -> std::same_as<const std::uint64_t*>;
    // false if the key was absent.
    { m.erase(key) } -> std::same_as<bool>;
    { cm.size() } -> std::same_as<std::size_t>;
    // Slots for the open-addressing tables, buckets for std::unordered_map.
    { cm.capacity() } -> std::same_as<std::size_t>;
    // Makes room for n keys without a rehash.
    { m.reserve(n) };
};

}  // namespace swiss
