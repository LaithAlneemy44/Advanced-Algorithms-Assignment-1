#include "keys.hpp"

#include <swiss/hash.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace {

bool all_distinct(std::vector<std::uint64_t> keys) {
    std::sort(keys.begin(), keys.end());
    return std::adjacent_find(keys.begin(), keys.end()) == keys.end();
}

TEST(Keys, UniformAreDistinctAndReproducible) {
    const auto a = swiss::keys::uniform(10'000, 7);
    EXPECT_EQ(a.size(), 10'000u);
    EXPECT_TRUE(all_distinct(a));
    EXPECT_EQ(a, swiss::keys::uniform(10'000, 7));
}

TEST(Keys, SequentialAndStrided) {
    EXPECT_EQ(swiss::keys::sequential(4), (std::vector<std::uint64_t>{0, 1, 2, 3}));
    EXPECT_EQ(swiss::keys::strided(4, 128), (std::vector<std::uint64_t>{0, 128, 256, 384}));
}

// The adversarial set: every key must start at the same slot with the same H2
// under SplitMix64Hash, and the keys themselves must all differ.
TEST(Keys, CollidingShareStartSlotAndH2) {
    constexpr std::size_t capacity = 1 << 16;
    const auto keys = swiss::keys::colliding(5'000, capacity, 99);
    ASSERT_EQ(keys.size(), 5'000u);
    EXPECT_TRUE(all_distinct(keys));

    const swiss::SplitMix64Hash hash;
    const std::size_t slot = swiss::h1(hash(keys[0]), capacity - 1);
    const std::uint8_t tag = swiss::h2(hash(keys[0]));
    for (const std::uint64_t key : keys) {
        ASSERT_EQ(swiss::h1(hash(key), capacity - 1), slot);
        ASSERT_EQ(swiss::h2(hash(key)), tag);
    }
}

TEST(Keys, CollidingRejectsBadCapacity) {
    EXPECT_THROW(swiss::keys::colliding(10, 1000, 1), std::invalid_argument);
}

}  // namespace
