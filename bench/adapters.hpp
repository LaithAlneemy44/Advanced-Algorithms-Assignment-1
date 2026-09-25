#pragma once

#include <swiss/hash.hpp>
#include <swiss/map_interface.hpp>

#include <cstddef>
#include <cstdint>
#include <unordered_map>

// Thin wrappers that give the baseline tables the study's Map interface, so
// tests and benchmarks run exactly the same code on every table.
namespace swiss::adapters {

// std::unordered_map: node-based. Each key is a separate heap allocation, and a
// bucket is a linked list. Left at its default max load factor of 1.0, since
// that is how it is normally used. Tables are compared at equal key counts, not
// equal load factors.
template <class Hash = SplitMix64Hash>
class StdUnorderedMap {
public:
    bool insert(std::uint64_t key, std::uint64_t value) {
        return map_.try_emplace(key, value).second;
    }

    const std::uint64_t* find(std::uint64_t key) const {
        const auto it = map_.find(key);
        return it == map_.end() ? nullptr : &it->second;
    }

    bool erase(std::uint64_t key) { return map_.erase(key) != 0; }
    void reserve(std::size_t n) { map_.reserve(n); }
    std::size_t size() const noexcept { return map_.size(); }
    std::size_t capacity() const noexcept { return map_.bucket_count(); }

private:
    std::unordered_map<std::uint64_t, std::uint64_t, Hash> map_;
};

static_assert(Map<StdUnorderedMap<>>);

}  // namespace swiss::adapters
