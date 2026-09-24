#include <swiss/hash.hpp>
#include <swiss/hash_candidates.hpp>

#include <benchmark/benchmark.h>

#include <cstdint>
#include <random>
#include <vector>

// Cost of each hash on its own. Every structure pays the shared hash's cost on
// every operation, so it sets a floor under the per-operation times.
//
// Caveat for SimpleTabulation: its 16 KB of tables stay hot in L1 here. Inside
// a hash table they compete with the table's own data, so this is its best case.

// Latency: each hash depends on the previous result, so the CPU cannot overlap
// them. This is the cost on a lookup's critical path. Starts at 1 because 0 is
// a fixed point of several of these hashes. Identity's result is the loop
// overhead, the floor under every other number.
template <class Hash>
static void BM_Latency(benchmark::State& state) {
    const Hash hash;
    std::uint64_t x = 1;
    for (auto _ : state) {
        x = hash(x);
        benchmark::DoNotOptimize(x);
    }
}

// Throughput: independent keys, so consecutive hashes overlap in the pipeline.
// 2048 keys = 16 KB, half of the 32 KB L1d, so memory is not what gets measured.
template <class Hash>
static void BM_Throughput(benchmark::State& state) {
    const Hash hash;
    std::vector<std::uint64_t> keys(2048);
    std::mt19937_64 rng(1);
    for (auto& k : keys) {
        k = rng();
    }
    for (auto _ : state) {
        std::uint64_t acc = 0;
        for (const std::uint64_t k : keys) {
            acc ^= hash(k);
        }
        benchmark::DoNotOptimize(acc);
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(keys.size()));
}

#define SWISS_HASH_BENCHMARKS(Hash)       \
    BENCHMARK_TEMPLATE(BM_Latency, Hash); \
    BENCHMARK_TEMPLATE(BM_Throughput, Hash)

SWISS_HASH_BENCHMARKS(swiss::candidates::Identity);
SWISS_HASH_BENCHMARKS(swiss::candidates::Multiplicative);
SWISS_HASH_BENCHMARKS(swiss::candidates::FoldedMultiply);
SWISS_HASH_BENCHMARKS(swiss::candidates::SimpleTabulation);
SWISS_HASH_BENCHMARKS(swiss::SplitMix64Hash);
