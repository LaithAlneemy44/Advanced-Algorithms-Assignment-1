#pragma once

#include <swiss/growth.hpp>
#include <swiss/hash.hpp>
#include <swiss/map_interface.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace swiss {

// R0: linear probing, the first step of the build. Keys and values sit directly
// in the slots, with no metadata of any kind. Every probe reads a whole 16-byte
// slot to learn whether it is empty, deleted, or a match.
//
// Invariant: every stored key sits at its home slot or further along its probe
// sequence, with no EMPTY slot in between.
//   - Established by insert, which fills the first free slot from home.
//   - Relied on by find and erase, which stop at the first EMPTY.
//   - Preserved by erase, which writes a tombstone, never EMPTY.
//   - Re-established by rehash, which rebuilds the table from scratch.
//
// Empty and deleted slots are marked by two reserved key values, as in Google's
// dense_hash_map, so those two keys can never be stored.
template <class Hash = SplitMix64Hash>
class LinearProbingMap {
public:
    static constexpr std::uint64_t kEmpty = ~std::uint64_t{0};
    static constexpr std::uint64_t kDeleted = ~std::uint64_t{0} - 1;
    static_assert(is_reserved_key(kEmpty) && is_reserved_key(kDeleted));

    bool insert(std::uint64_t key, std::uint64_t value) {
        assert(!is_reserved_key(key));
        // One pass: stop if the key is present, otherwise remember the first
        // reusable slot. The search cannot stop at a tombstone, because the key
        // may still be stored further along.
        std::size_t target = kNone;
        if (!slots_.empty()) {
            for (std::size_t i = home(key);; i = next(i)) {
                const std::uint64_t k = slots_[i].key;
                if (k == key) {
                    return false;
                }
                if (k == kDeleted && target == kNone) {
                    target = i;
                } else if (k == kEmpty) {
                    if (target == kNone) {
                        target = i;
                    }
                    break;
                }
            }
        }
        // Reusing a tombstone leaves the load unchanged. Taking an EMPTY slot
        // raises it, and may need a rehash first.
        if (target == kNone || slots_[target].key == kEmpty) {
            if (used_ + 1 > growth::max_used(capacity())) {
                rehash(growth::next_capacity(capacity(), size_ + 1));
                target = first_free(key);
            }
            ++used_;
        }
        slots_[target] = {key, value};
        ++size_;
        return true;
    }

    const std::uint64_t* find(std::uint64_t key) const {
        const std::size_t i = index_of(key);
        return i == kNone ? nullptr : &slots_[i].value;
    }

    bool erase(std::uint64_t key) {
        const std::size_t i = index_of(key);
        if (i == kNone) {
            return false;
        }
        // A tombstone, not EMPTY: keys further along may have probed past this
        // slot. used_ stays the same, since the tombstone still occupies it.
        slots_[i].key = kDeleted;
        --size_;
        return true;
    }

    void reserve(std::size_t n) {
        if (n > 0 && growth::capacity_for(n) > capacity()) {
            rehash(growth::capacity_for(n));
        }
    }

    std::size_t size() const noexcept { return size_; }
    std::size_t capacity() const noexcept { return slots_.size(); }

private:
    struct Slot {
        std::uint64_t key;
        std::uint64_t value;
    };

    static constexpr std::size_t kNone = ~std::size_t{0};

    std::size_t home(std::uint64_t key) const noexcept { return h1(hash_(key), mask_); }
    std::size_t next(std::size_t i) const noexcept { return (i + 1) & mask_; }

    // Slot holding the key, or kNone. Terminates because at least 1/8 of the
    // slots are EMPTY (growth::max_used).
    std::size_t index_of(std::uint64_t key) const {
        assert(!is_reserved_key(key));
        if (slots_.empty()) {
            return kNone;  // capacity 0: touch no memory
        }
        for (std::size_t i = home(key);; i = next(i)) {
            const std::uint64_t k = slots_[i].key;
            if (k == key) {
                return i;
            }
            if (k == kEmpty) {
                return kNone;
            }
        }
    }

    // First EMPTY or deleted slot from the key's home. Only used when the key is
    // known to be absent.
    std::size_t first_free(std::uint64_t key) const noexcept {
        std::size_t i = home(key);
        while (!is_reserved_key(slots_[i].key)) {
            i = next(i);
        }
        return i;
    }

    // Rebuilds into a fresh array. This drops every tombstone.
    void rehash(std::size_t new_capacity) {
        std::vector<Slot> old = std::exchange(slots_, std::vector<Slot>(new_capacity, Slot{kEmpty, 0}));
        mask_ = new_capacity - 1;
        for (const Slot& slot : old) {
            if (!is_reserved_key(slot.key)) {
                slots_[first_free(slot.key)] = slot;
            }
        }
        used_ = size_;
    }

    std::vector<Slot> slots_;
    std::size_t mask_ = 0;
    std::size_t size_ = 0;  // live keys
    std::size_t used_ = 0;  // live keys + tombstones
    Hash hash_{};
};

static_assert(Map<LinearProbingMap<>>);

}  // namespace swiss
