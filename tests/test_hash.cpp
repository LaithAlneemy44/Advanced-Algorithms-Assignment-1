#include <swiss/hash.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <random>
#include <type_traits>
#include <unordered_map>

namespace {

using swiss::splitmix64_mix;

// Expected values come from Vigna's unmodified reference splitmix64.c
// (https://prng.di.unimi.it/splitmix64.c), not from this implementation.
// Method: set the reference state to x - 0x9E3779B97F4A7C15 and call next().
// next() adds the constant back before mixing, so it returns mix(x).
// Sanity check on that setup: seed 0 gave the published first outputs
// e220a8397b1dcdaf, 6e789e6aa1b965f4, 06c45d188009454f.
struct KnownAnswer {
    std::uint64_t in;
    std::uint64_t out;
};

constexpr KnownAnswer kKnownAnswers[] = {
    {0x0000000000000000ULL, 0x0000000000000000ULL},
    {0x0000000000000001ULL, 0x5692161D100B05E5ULL},
    {0x0000000000000002ULL, 0xDBD238973A2B148AULL},
    {0x0000000000000003ULL, 0x1E535EEDE31428F0ULL},
    {0x000000000000002AULL, 0xA759EA27D4727622ULL},
    {0xFFFFFFFFFFFFFFFFULL, 0xB4D055FCF2CBBD7BULL},
    {0x8000000000000000ULL, 0x25C26EA579CEA98AULL},
    {0x0123456789ABCDEFULL, 0xB2C058E4EBB5112CULL},
    {0xDEADBEEFCAFEBABEULL, 0x7AD6664F09FFE52CULL},
    {0x000000003B9ACA07ULL, 0xE853B7F5CEBF4630ULL},
};

// Also checked at compile time, so the function must stay constexpr.
static_assert(splitmix64_mix(0) == 0);
static_assert(splitmix64_mix(1) == 0x5692161D100B05E5ULL);

using swiss::splitmix64_unmix;
using swiss::detail::inverse_mod_2_64;
using swiss::detail::kSplitMixMul1;
using swiss::detail::kSplitMixMul2;

static_assert(kSplitMixMul1 * inverse_mod_2_64(kSplitMixMul1) == 1);
static_assert(kSplitMixMul2 * inverse_mod_2_64(kSplitMixMul2) == 1);

TEST(SplitMix64, MatchesReferenceImplementation) {
    for (const auto& [in, out] : kKnownAnswers) {
        EXPECT_EQ(splitmix64_mix(in), out) << "input 0x" << std::hex << in;
    }
}

// A working left inverse means no two inputs can share an output, so mix is a
// bijection on 64-bit values. A plain "no duplicates in a sample" check could
// not tell a bijection from any decent random-looking hash.
TEST(SplitMix64, IsInvertible) {
    for (const auto& [in, out] : kKnownAnswers) {
        EXPECT_EQ(splitmix64_unmix(out), in);
    }
    std::mt19937_64 rng(12345);
    for (int i = 0; i < 1'000'000; ++i) {
        const std::uint64_t x = rng();
        ASSERT_EQ(splitmix64_unmix(splitmix64_mix(x)), x) << "input 0x" << std::hex << x;
    }
}

TEST(SplitMix64Hash, AgreesWithMix) {
    const swiss::SplitMix64Hash hash;
    for (const auto& [in, out] : kKnownAnswers) {
        EXPECT_EQ(hash(in), out);
    }
}

TEST(Split, TakesTopSevenBitsForH2AndMaskedHashForH1) {
    constexpr std::size_t hash = 0xFEDCBA9876543210ULL;
    static_assert(swiss::h2(hash) == 0x7F);  // 0xFE >> 1
    static_assert(swiss::h1(hash, 0xFFF) == 0x210);
    static_assert(swiss::h2(0x01FFFFFFFFFFFFFFULL) == 0);  // bits below 57 never reach H2
    SUCCEED();
}

// If the hasher could throw, libstdc++'s std::unordered_map would store the
// hash code in every node and the baseline would change shape.
TEST(SplitMix64Hash, IsNoexcept) {
    static_assert(std::is_nothrow_invocable_v<const swiss::SplitMix64Hash&, const std::uint64_t&>);
#if defined(__GLIBCXX__)
    // Internal libstdc++ trait, checked directly because it is the property that matters.
    static_assert(!std::__cache_default<std::uint64_t, swiss::SplitMix64Hash>::value);
#endif
}

}  // namespace
