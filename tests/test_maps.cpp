#include "adapters.hpp"
#include "keys.hpp"

#include <swiss/growth.hpp>
#include <swiss/linear_probing_map.hpp>
#include <swiss/map_interface.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <random>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Suite shared by every table in the study. std::unordered_map runs it too, so
// the suite itself is checked against a known-correct table.
// ---------------------------------------------------------------------------

template <class M>
class MapTest : public ::testing::Test {};

using Maps = ::testing::Types<swiss::LinearProbingMap<>, swiss::adapters::StdUnorderedMap<>>;

struct MapNames {
    template <class M>
    static std::string GetName(int) {
        if constexpr (std::is_same_v<M, swiss::LinearProbingMap<>>) {
            return "R0_linear";
        } else {
            return "std_unordered";
        }
    }
};

TYPED_TEST_SUITE(MapTest, Maps, MapNames);

TYPED_TEST(MapTest, EmptyTable) {
    TypeParam m;
    EXPECT_EQ(m.size(), 0u);
    EXPECT_EQ(m.find(42), nullptr);
    EXPECT_FALSE(m.erase(42));
    EXPECT_EQ(m.size(), 0u);
}

TYPED_TEST(MapTest, SingleElement) {
    TypeParam m;
    EXPECT_TRUE(m.insert(7, 70));
    EXPECT_EQ(m.size(), 1u);
    ASSERT_NE(m.find(7), nullptr);
    EXPECT_EQ(*m.find(7), 70u);
    EXPECT_EQ(m.find(8), nullptr);
    EXPECT_TRUE(m.erase(7));
    EXPECT_EQ(m.find(7), nullptr);
    EXPECT_EQ(m.size(), 0u);
}

// Key 0 hashes to 0 under splitmix64: home slot 0, H2 0. The largest key that
// is not reserved sits right next to the R0 marker values.
TYPED_TEST(MapTest, BoundaryKeys) {
    TypeParam m;
    constexpr std::uint64_t largest = swiss::kFirstReservedKey - 1;
    EXPECT_TRUE(m.insert(0, 1));
    EXPECT_TRUE(m.insert(largest, 2));
    EXPECT_EQ(*m.find(0), 1u);
    EXPECT_EQ(*m.find(largest), 2u);
    EXPECT_TRUE(m.erase(0));
    EXPECT_EQ(m.find(0), nullptr);
    EXPECT_EQ(*m.find(largest), 2u);
}

TYPED_TEST(MapTest, RepeatedInsertKeepsFirstValue) {
    TypeParam m;
    EXPECT_TRUE(m.insert(5, 1));
    EXPECT_FALSE(m.insert(5, 2));
    EXPECT_EQ(*m.find(5), 1u);
    EXPECT_EQ(m.size(), 1u);
}

TYPED_TEST(MapTest, EraseThenReinsert) {
    TypeParam m;
    EXPECT_TRUE(m.insert(5, 1));
    EXPECT_TRUE(m.erase(5));
    EXPECT_TRUE(m.insert(5, 2));
    EXPECT_EQ(*m.find(5), 2u);
    EXPECT_EQ(m.size(), 1u);
}

TYPED_TEST(MapTest, EraseMissingKey) {
    TypeParam m;
    EXPECT_TRUE(m.insert(1, 1));
    EXPECT_FALSE(m.erase(2));
    EXPECT_TRUE(m.erase(1));
    EXPECT_FALSE(m.erase(1));
    EXPECT_EQ(m.size(), 0u);
}

// From empty through many doublings, with every key checked at the end.
TYPED_TEST(MapTest, GrowthAcrossResizes) {
    TypeParam m;
    constexpr std::uint64_t n = 100'000;
    for (std::uint64_t k = 0; k < n; ++k) {
        ASSERT_TRUE(m.insert(k, k * 3));
        ASSERT_EQ(m.size(), k + 1);
    }
    for (std::uint64_t k = 0; k < n; ++k) {
        const std::uint64_t* v = m.find(k);
        ASSERT_NE(v, nullptr) << k;
        ASSERT_EQ(*v, k * 3);
    }
    for (std::uint64_t k = n; k < n + 1000; ++k) {
        ASSERT_EQ(m.find(k), nullptr) << k;
    }
}

TYPED_TEST(MapTest, ReserveAvoidsRehash) {
    TypeParam m;
    m.reserve(1000);
    const std::size_t capacity = m.capacity();
    for (std::uint64_t k = 0; k < 1000; ++k) {
        ASSERT_TRUE(m.insert(k, k));
    }
    EXPECT_EQ(m.capacity(), capacity);
}

// Keys whose splitmix64 hashes all share one starting slot and one H2, for every
// capacity up to 2^16: the worst case for the open-addressing tables. Erasing
// every other key then leaves the survivors behind a run of tombstones.
TYPED_TEST(MapTest, AdversarialCollisions) {
    const auto keys = swiss::keys::colliding(2000, 1 << 16, 7);
    TypeParam m;
    for (std::uint64_t i = 0; i < keys.size(); ++i) {
        ASSERT_TRUE(m.insert(keys[i], i));
    }
    for (std::uint64_t i = 0; i < keys.size(); i += 2) {
        ASSERT_TRUE(m.erase(keys[i]));
    }
    for (std::uint64_t i = 0; i < keys.size(); ++i) {
        const std::uint64_t* v = m.find(keys[i]);
        if (i % 2 == 0) {
            ASSERT_EQ(v, nullptr) << i;
        } else {
            ASSERT_NE(v, nullptr) << i;
            ASSERT_EQ(*v, i);
        }
    }
}

// Fill, erase everything, repeat with fresh keys. Lookups must still terminate
// on a table that is mostly tombstones, and the tombstones must get cleared.
TYPED_TEST(MapTest, ManyTombstones) {
    TypeParam m;
    std::uint64_t base = 0;
    for (int round = 0; round < 50; ++round, base += 1000) {
        for (std::uint64_t i = 0; i < 1000; ++i) {
            ASSERT_TRUE(m.insert(base + i, i));
        }
        for (std::uint64_t i = 0; i < 1000; ++i) {
            ASSERT_TRUE(m.erase(base + i));
        }
        ASSERT_EQ(m.size(), 0u);
    }
    EXPECT_EQ(m.find(123'456'789), nullptr);
    EXPECT_TRUE(m.insert(123'456'789, 1));
    EXPECT_EQ(*m.find(123'456'789), 1u);
}

// Random inserts, erases and lookups, each checked against std::unordered_map as
// an oracle. A small key pool makes operations collide on the same keys often,
// so reinserts, double erases and tombstone reuse all happen many times.
TYPED_TEST(MapTest, MatchesUnorderedMapOnRandomOperations) {
    for (std::uint64_t seed = 0; seed < 10; ++seed) {
        std::mt19937_64 rng(seed);
        std::vector<std::uint64_t> pool = swiss::keys::uniform(3000, seed);
        for (std::uint64_t k = 0; k < 1000; ++k) {
            pool.push_back(k);  // small keys, including 0
        }

        TypeParam m;
        std::unordered_map<std::uint64_t, std::uint64_t> oracle;
        for (int op = 0; op < 100'000; ++op) {
            const std::uint64_t key = pool[rng() % pool.size()];
            switch (rng() % 4) {
                case 0:
                case 1: {
                    const std::uint64_t value = rng();
                    ASSERT_EQ(m.insert(key, value), oracle.try_emplace(key, value).second)
                        << "seed " << seed << " op " << op;
                    break;
                }
                case 2:
                    ASSERT_EQ(m.erase(key), oracle.erase(key) != 0) << "seed " << seed << " op " << op;
                    break;
                default: {
                    const std::uint64_t* v = m.find(key);
                    const auto it = oracle.find(key);
                    ASSERT_EQ(v != nullptr, it != oracle.end()) << "seed " << seed << " op " << op;
                    if (v != nullptr) {
                        ASSERT_EQ(*v, it->second) << "seed " << seed << " op " << op;
                    }
                }
            }
            ASSERT_EQ(m.size(), oracle.size()) << "seed " << seed << " op " << op;
        }
        for (const auto& [key, value] : oracle) {
            const std::uint64_t* v = m.find(key);
            ASSERT_NE(v, nullptr) << "seed " << seed;
            ASSERT_EQ(*v, value) << "seed " << seed;
        }
    }
}

// ---------------------------------------------------------------------------
// Growth policy, shared by R0, R1 and R2
// ---------------------------------------------------------------------------

TEST(Growth, LoadLimitIsSevenEighths) {
    static_assert(swiss::growth::max_used(16) == 14);
    static_assert(swiss::growth::max_used(1024) == 896);
    static_assert(swiss::growth::capacity_for(14) == 16);
    static_assert(swiss::growth::capacity_for(15) == 32);
    SUCCEED();
}

TEST(Growth, RebuildsInPlaceWhenMostlyTombstones) {
    static_assert(swiss::growth::next_capacity(0, 1) == 16);
    static_assert(swiss::growth::next_capacity(16, 8) == 16);   // half live: clear tombstones
    static_assert(swiss::growth::next_capacity(16, 9) == 32);   // more than half: double
    SUCCEED();
}

// ---------------------------------------------------------------------------
// R0 specifics
// ---------------------------------------------------------------------------

TEST(LinearProbingMap, StartsWithoutAllocating) {
    swiss::LinearProbingMap<> m;
    EXPECT_EQ(m.capacity(), 0u);
    EXPECT_EQ(m.find(1), nullptr);
    m.reserve(0);
    EXPECT_EQ(m.capacity(), 0u);
}

TEST(LinearProbingMap, GrowsAtSevenEighths) {
    swiss::LinearProbingMap<> m;
    for (std::uint64_t k = 0; k < 14; ++k) {
        m.insert(k, k);
    }
    EXPECT_EQ(m.capacity(), 16u);
    m.insert(14, 14);
    EXPECT_EQ(m.capacity(), 32u);
}

// Churn at a steady size of 1 key: tombstones pile up and must be cleared by
// rebuilds at the same capacity, not by growing.
TEST(LinearProbingMap, ClearsTombstonesWithoutGrowing) {
    swiss::LinearProbingMap<> m;
    m.insert(0, 0);
    for (std::uint64_t k = 1; k < 10'000; ++k) {
        ASSERT_TRUE(m.insert(k, k));
        ASSERT_TRUE(m.erase(k - 1));
        ASSERT_EQ(m.capacity(), 16u) << k;
    }
    EXPECT_EQ(m.size(), 1u);
    EXPECT_EQ(*m.find(9'999), 9'999u);
}

}  // namespace
