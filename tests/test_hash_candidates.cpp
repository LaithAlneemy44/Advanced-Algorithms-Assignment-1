#include <swiss/hash_candidates.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>
#include <unordered_map>

namespace {

using swiss::candidates::FoldedMultiply;
using swiss::candidates::Identity;
using swiss::candidates::Multiplicative;
using swiss::candidates::SimpleTabulation;

// Expected values computed in Python with arbitrary-precision integers, so no
// 64-bit wrap-around or __int128 is involved. Python's splitmix64 was first
// checked against the published seed-0 outputs of Vigna's reference.
struct Expected {
    std::uint64_t key;
    std::uint64_t multiplicative;
    std::uint64_t folded;
    std::uint64_t tabulation;
};

constexpr Expected kExpected[] = {
    {0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0xA0397C19904DD913ULL},
    {0x0000000000000001ULL, 0x9E3779B97F4A7C15ULL, 0x9E3779B97F4A7C15ULL, 0x2C614A4A4AE97148ULL},
    {0x0000000000000002ULL, 0x3C6EF372FE94F82AULL, 0x3C6EF372FE94F82BULL, 0x44DD89386B5951F3ULL},
    {0x000000000000002AULL, 0xF519F86EE2385B72ULL, 0xF519F86EE2385B6BULL, 0x45C5472BDB726C14ULL},
    {0x0000000000000080ULL, 0x1BBCDCBFA53E0A80ULL, 0x1BBCDCBFA53E0ACFULL, 0xDA80F40F395F1CFDULL},
    {0x0000000000001000ULL, 0x779B97F4A7C15000ULL, 0x779B97F4A7C159E3ULL, 0x39F87BC1714A9925ULL},
    {0xFFFFFFFFFFFFFFFFULL, 0x61C8864680B583EBULL, 0xFFFFFFFFFFFFFFFFULL, 0xE2F0DFC9287F9026ULL},
    {0x0123456789ABCDEFULL, 0x0C93A7B79AEDA89BULL, 0x0C27A443D5FF218EULL, 0x8A803901EA902741ULL},
};

TEST(Candidates, MatchIndependentlyComputedValues) {
    for (const auto& e : kExpected) {
        EXPECT_EQ(Identity{}(e.key), e.key);
        EXPECT_EQ(Multiplicative{}(e.key), e.multiplicative) << std::hex << e.key;
        EXPECT_EQ(FoldedMultiply{}(e.key), e.folded) << std::hex << e.key;
        EXPECT_EQ(SimpleTabulation{}(e.key), e.tabulation) << std::hex << e.key;
    }
}

// Output bit i of a product depends only on key bits 0 to i. So a key that is a
// multiple of 2^j gives a product whose low j bits are 0, whatever the multiplier.
TEST(Multiplicative, LowBitsOfStridedKeysAreZero) {
    for (std::uint64_t i = 1; i < 1000; ++i) {
        EXPECT_EQ(Multiplicative{}(i * 128) & 0x7F, 0u);
        EXPECT_EQ(Multiplicative{}(i * 4096) & 0xFFF, 0u);
    }
}

// With k = 2^64 - 1, the product is C * 2^64 - C. The high half is C - 1 and the
// low half is 2^64 - C, which is the bitwise complement of C - 1. So the fold
// gives all ones for every multiplier C: a fixed point.
TEST(FoldedMultiply, AllOnesIsAFixedPoint) {
    EXPECT_EQ(FoldedMultiply{}(~0ULL), ~0ULL);
}

// The tables come from the splitmix64 generator with seed 0, so the first entry
// is that generator's first published output.
TEST(SimpleTabulation, TablesStartWithGeneratorOutput) {
    EXPECT_EQ(swiss::candidates::detail::kTabulationTables[0][0], 0xE220A8397B1DCDAFULL);
    EXPECT_EQ(swiss::candidates::detail::kTabulationTables[0][1], 0x6E789E6AA1B965F4ULL);
}

// Four keys that form a "rectangle" over two bytes: byte 0 takes values a or b,
// byte 1 takes values c or d, the other bytes are shared. Each table entry
// appears exactly twice across the four hashes, so they xor to 0. Given three
// of the hashes, the fourth is fixed. That is why simple tabulation is
// 3-independent but not 4-independent.
TEST(SimpleTabulation, RectangleXorsToZero) {
    const SimpleTabulation hash;
    const std::uint64_t shared = 0x0123456789000000ULL;
    const std::uint64_t a = 0x11, b = 0x22, c = 0x33, d = 0x44;
    const std::uint64_t k1 = shared | a | (c << 8);
    const std::uint64_t k2 = shared | b | (c << 8);
    const std::uint64_t k3 = shared | a | (d << 8);
    const std::uint64_t k4 = shared | b | (d << 8);
    EXPECT_EQ(hash(k1) ^ hash(k2) ^ hash(k3) ^ hash(k4), 0u);
}

// Same reason as SplitMix64Hash: a throwing hasher would change the layout of
// libstdc++'s std::unordered_map nodes.
template <class Hash>
constexpr bool keeps_unordered_map_layout() {
    static_assert(std::is_nothrow_invocable_v<const Hash&, const std::uint64_t&>);
#if defined(__GLIBCXX__)
    static_assert(!std::__cache_default<std::uint64_t, Hash>::value);
#endif
    return true;
}

TEST(Candidates, AreNoexcept) {
    static_assert(keeps_unordered_map_layout<Identity>());
    static_assert(keeps_unordered_map_layout<Multiplicative>());
    static_assert(keeps_unordered_map_layout<FoldedMultiply>());
    static_assert(keeps_unordered_map_layout<SimpleTabulation>());
    SUCCEED();
}

}  // namespace
