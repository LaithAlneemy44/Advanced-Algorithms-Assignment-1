// Hash-level quality analysis for the hash-quality experiment.
//
// Needs no hash table. For each hash and key set it computes where each key
// would start probing (H1) and which tag it would carry (H2), then measures how
// well those are spread. It predicts the two failure modes before the table
// exists:
//   - clustering: many keys share few starting slots
//   - false H2 matches: keys that start near each other share H2, so the group
//     filter lets them through and they cost extra key comparisons
//
// Usage: swiss_hash_quality [output_dir]
// Writes avalanche.csv and distribution.csv, and prints a summary.

#include "keys.hpp"

#include <swiss/hash.hpp>
#include <swiss/hash_candidates.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr std::uint64_t kSeed = 20260924;

template <class F>
void for_each_hash(F&& f) {
    f.template operator()<swiss::candidates::Identity>("identity");
    f.template operator()<swiss::candidates::Multiplicative>("multiplicative");
    f.template operator()<swiss::candidates::FoldedMultiply>("folded_multiply");
    f.template operator()<swiss::candidates::SimpleTabulation>("tabulation");
    f.template operator()<swiss::SplitMix64Hash>("splitmix64");
}

// ---------------------------------------------------------------------------
// Avalanche
// ---------------------------------------------------------------------------
// For random keys x, flip input bit i and record whether output bit j flips.
// An ideal hash flips every output bit with probability 1/2 for every input
// bit. bias(i, j) = |P(flip) - 0.5|, from 0 (ideal) to 0.5 (bit j never, or
// always, responds to bit i).
//
// Per output bit, two summaries over the 64 input bits:
//   mean_bias: how well bit j mixes the key overall
//   max_bias:  the worst single input bit
// Sampling noise: the standard error of each P(flip) is 0.5 / sqrt(samples).

constexpr int kAvalancheSamples = 100'000;

template <class Hash>
void avalanche(const char* name, std::ofstream& csv) {
    const Hash hash;
    std::mt19937_64 rng(kSeed);
    // counts[i][j]: how often output bit j flipped when input bit i flipped.
    std::vector<std::array<std::uint32_t, 64>> counts(64);
    for (auto& row : counts) {
        row.fill(0);
    }

    for (int s = 0; s < kAvalancheSamples; ++s) {
        const std::uint64_t x = rng();
        const std::uint64_t hx = hash(x);
        for (int i = 0; i < 64; ++i) {
            std::uint64_t diff = hx ^ hash(x ^ (std::uint64_t{1} << i));
            for (int j = 0; j < 64; ++j, diff >>= 1) {
                counts[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] +=
                    static_cast<std::uint32_t>(diff & 1);
            }
        }
    }

    for (std::size_t j = 0; j < 64; ++j) {
        double sum = 0;
        double worst = 0;
        for (std::size_t i = 0; i < 64; ++i) {
            const double bias = std::abs(counts[i][j] / double{kAvalancheSamples} - 0.5);
            sum += bias;
            worst = std::max(worst, bias);
        }
        csv << name << ',' << j << ',' << sum / 64 << ',' << worst << '\n';
    }
}

// ---------------------------------------------------------------------------
// Distribution of H1 and H2 over a key set
// ---------------------------------------------------------------------------

struct Distribution {
    std::size_t keys = 0;
    std::size_t distinct_starts = 0;
    std::size_t max_per_start = 0;
    double chi2_ratio = 0;       // ~1 for random placement, < 1 more even, >> 1 clustered
    std::size_t distinct_h2 = 0;
    double h2_false_match = 0;   // ~1/128 for a good hash, 1 if H2 is constant
};

// Starting slots at load 7/8. chi2_ratio compares the count of keys per slot
// with a uniform spread: sum of (count - mean)^2 / mean, divided by the degrees
// of freedom.
//
// h2_false_match: take every pair of keys whose starting slots fall in the same
// 16-slot window, the span one group load covers. It is the fraction of those
// pairs that also share H2, which is the chance the group filter fails to tell
// them apart. This ignores displacement by probing, so it approximates what a
// real table sees.
template <class Hash>
Distribution distribution(const std::vector<std::uint64_t>& keys, std::size_t capacity) {
    const Hash hash;
    const std::size_t mask = capacity - 1;
    std::vector<std::uint32_t> per_start(capacity, 0);
    std::vector<std::uint64_t> window_tag;  // (window << 7) | h2, sorted to count pairs
    window_tag.reserve(keys.size());
    std::array<bool, 128> seen_h2{};

    for (const std::uint64_t key : keys) {
        const std::size_t h = hash(key);
        const std::size_t start = swiss::h1(h, mask);
        const std::uint8_t tag = swiss::h2(h);
        ++per_start[start];
        seen_h2[tag] = true;
        window_tag.push_back((static_cast<std::uint64_t>(start >> 4) << 7) | tag);
    }

    Distribution d;
    d.keys = keys.size();
    const double mean = static_cast<double>(keys.size()) / static_cast<double>(capacity);
    double chi2 = 0;
    for (const std::uint32_t c : per_start) {
        d.distinct_starts += c > 0;
        d.max_per_start = std::max<std::size_t>(d.max_per_start, c);
        chi2 += (c - mean) * (c - mean) / mean;
    }
    d.chi2_ratio = chi2 / static_cast<double>(capacity - 1);
    d.distinct_h2 = static_cast<std::size_t>(std::count(seen_h2.begin(), seen_h2.end(), true));

    // Count pairs by runs in sorted order: C(n, 2) pairs per equal (window, h2)
    // and per equal window.
    std::sort(window_tag.begin(), window_tag.end());
    double same_window = 0;
    double same_window_and_h2 = 0;
    auto pairs = [](double n) { return n * (n - 1) / 2; };
    for (std::size_t lo = 0; lo < window_tag.size();) {
        const std::uint64_t window = window_tag[lo] >> 7;
        std::size_t hi = lo;
        while (hi < window_tag.size() && (window_tag[hi] >> 7) == window) {
            std::size_t run_end = hi;
            while (run_end < window_tag.size() && window_tag[run_end] == window_tag[hi]) {
                ++run_end;
            }
            same_window_and_h2 += pairs(static_cast<double>(run_end - hi));
            hi = run_end;
        }
        same_window += pairs(static_cast<double>(hi - lo));
        lo = hi;
    }
    d.h2_false_match = same_window > 0 ? same_window_and_h2 / same_window : 0;
    return d;
}

struct KeySet {
    std::string name;
    std::vector<std::uint64_t> keys;
};

std::vector<KeySet> key_sets(std::size_t capacity) {
    const std::size_t n = capacity / 8 * 7;  // load 7/8
    return {
        {"uniform", swiss::keys::uniform(n, kSeed)},
        {"sequential", swiss::keys::sequential(n)},
        {"stride128", swiss::keys::strided(n, 128)},
        {"stride4096", swiss::keys::strided(n, 4096)},
        {"colliding_splitmix64", swiss::keys::colliding(n, capacity, kSeed)},
    };
}

}  // namespace

int main(int argc, char** argv) {
    const std::filesystem::path out = argc > 1 ? argv[1] : ".";
    std::filesystem::create_directories(out);

    std::ofstream aval(out / "avalanche.csv");
    aval << "hash,output_bit,mean_bias,max_bias\n";
    for_each_hash([&]<class Hash>(const char* name) { avalanche<Hash>(name, aval); });

    std::ofstream dist(out / "distribution.csv");
    dist << "hash,key_set,capacity,keys,distinct_starts,max_per_start,chi2_ratio,distinct_h2,h2_false_match\n";
    std::printf("%-16s %-21s %9s %10s %9s %10s %7s %9s\n", "hash", "key_set", "capacity",
                "starts", "max/start", "chi2_ratio", "h2_vals", "h2_false");

    for (const std::size_t capacity : {std::size_t{1} << 10, std::size_t{1} << 16, std::size_t{1} << 20}) {
        const auto sets = key_sets(capacity);
        for_each_hash([&]<class Hash>(const char* name) {
            for (const auto& set : sets) {
                const Distribution d = distribution<Hash>(set.keys, capacity);
                dist << name << ',' << set.name << ',' << capacity << ',' << d.keys << ','
                     << d.distinct_starts << ',' << d.max_per_start << ',' << d.chi2_ratio << ','
                     << d.distinct_h2 << ',' << d.h2_false_match << '\n';
                std::printf("%-16s %-21s %9zu %10zu %9zu %10.3f %7zu %9.4f\n", name, set.name.c_str(),
                            capacity, d.distinct_starts, d.max_per_start, d.chi2_ratio, d.distinct_h2,
                            d.h2_false_match);
            }
        });
    }

    std::printf("\nReference for a random hash at load 7/8: starts ~ %.3f x capacity, chi2_ratio ~ 1, "
                "h2_false ~ %.4f.\nAvalanche noise: standard error %.4f per cell.\n",
                1 - std::exp(-0.875), 1.0 / 128, 0.5 / std::sqrt(double{kAvalancheSamples}));
    return 0;
}
