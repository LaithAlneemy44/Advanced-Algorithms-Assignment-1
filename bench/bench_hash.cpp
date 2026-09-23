#include <swiss/hash.hpp>

#include <benchmark/benchmark.h>

#include <cstdint>
#include <random>
#include <vector>

// Cost of the shared hash on its own. Every structure pays it on every
// operation, so it sets a floor under the per-operation times.

// Latency: each hash depends on the previous result, so the CPU cannot overlap
// them. Starts at 1 because 0 is a fixed point of the mix and would stay 0.
static void BM_SplitMix64_Latency(benchmark::State& state) {
    std::uint64_t x = 1;
    for (auto _ : state) {
        x = swiss::splitmix64_mix(x);
        benchmark::DoNotOptimize(x);
    }
}
BENCHMARK(BM_SplitMix64_Latency);

// Throughput: independent keys, so consecutive hashes overlap in the pipeline.
// 2048 keys = 16 KB, half of the 32 KB L1d, so memory is not what gets measured.
static void BM_SplitMix64_Throughput(benchmark::State& state) {
    std::vector<std::uint64_t> keys(2048);
    std::mt19937_64 rng(1);
    for (auto& k : keys) {
        k = rng();
    }
    for (auto _ : state) {
        std::uint64_t acc = 0;
        for (const std::uint64_t k : keys) {
            acc ^= swiss::splitmix64_mix(k);
        }
        benchmark::DoNotOptimize(acc);
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(keys.size()));
}
BENCHMARK(BM_SplitMix64_Throughput);
