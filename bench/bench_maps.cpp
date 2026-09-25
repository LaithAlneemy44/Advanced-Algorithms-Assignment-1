// Benchmarks every table in the study on the same workloads, sizes and keys.
//
// Benchmark names are <workload>/<table>/<n>. Each iteration performs n
// operations on a table holding n keys, so time per operation = real_time / n.
// Only the operations are timed (manual timing): building tables, copying them
// and destroying them are excluded, since destroying a std::unordered_map frees
// every node and would otherwise dominate.
//
// Lookups run in shuffled order and are independent of each other, so the CPU
// can overlap several at once. The numbers are throughput, not the latency of a
// single lookup.
//
// Usage: swiss_bench_maps [Google Benchmark flags]
//   e.g. --benchmark_repetitions=5 --benchmark_out=results/maps/run.json

#include "adapters.hpp"
#include "keys.hpp"

#include <swiss/hash.hpp>
#include <swiss/linear_probing_map.hpp>

#include <benchmark/benchmark.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <random>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
constexpr std::uint64_t kSeed = 20260925;

// Uniform keys, generated once per size. The first n are inserted, the second n
// are guaranteed absent and serve as misses. `order` is the first n shuffled.
struct KeySet {
    std::vector<std::uint64_t> present;
    std::vector<std::uint64_t> absent;
    std::vector<std::uint64_t> order;
};

const KeySet& key_set(std::size_t n) {
    static std::map<std::size_t, KeySet> cache;
    KeySet& set = cache[n];
    if (set.present.empty()) {
        const auto all = swiss::keys::uniform(2 * n, kSeed);
        set.present.assign(all.begin(), all.begin() + static_cast<std::ptrdiff_t>(n));
        set.absent.assign(all.begin() + static_cast<std::ptrdiff_t>(n), all.end());
        set.order = set.present;
        std::shuffle(set.order.begin(), set.order.end(), std::mt19937_64(kSeed + 1));
    }
    return set;
}

// A stream of fresh keys for the churn and mixed workloads. splitmix64 is a
// bijection, so distinct counters give distinct keys and the stream never
// repeats. A clash with one of key_set()'s n random keys has probability about
// (keys drawn) * n / 2^64, which is negligible at these sizes.
class FreshKeys {
public:
    std::uint64_t next() {
        std::uint64_t key;
        do {
            key = swiss::splitmix64_mix(counter_++);
        } while (swiss::is_reserved_key(key));
        return key;
    }

private:
    std::uint64_t counter_ = 0;
};

double seconds(Clock::duration d) {
    return std::chrono::duration<double>(d).count();
}

std::size_t size_arg(const benchmark::State& state) {
    return static_cast<std::size_t>(state.range(0));
}

void finish(benchmark::State& state, std::size_t n) {
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(n));
}

template <class Map>
Map build(const std::vector<std::uint64_t>& keys) {
    Map m;
    for (const std::uint64_t k : keys) {
        m.insert(k, k);
    }
    return m;
}

// ---------------------------------------------------------------------------
// Workloads
// ---------------------------------------------------------------------------

// Insert n keys into an empty table, including every rehash on the way.
template <class Map>
void insert_grow(benchmark::State& state) {
    const std::size_t n = size_arg(state);
    const KeySet& keys = key_set(n);
    for (auto _ : state) {
        Map m;
        const auto t0 = Clock::now();
        for (const std::uint64_t k : keys.present) {
            m.insert(k, k);
        }
        benchmark::ClobberMemory();
        state.SetIterationTime(seconds(Clock::now() - t0));
        benchmark::DoNotOptimize(m.size());
    }
    finish(state, n);
}

// Insert n keys after reserve(n): no rehash, so pure insert cost.
template <class Map>
void insert_reserved(benchmark::State& state) {
    const std::size_t n = size_arg(state);
    const KeySet& keys = key_set(n);
    for (auto _ : state) {
        Map m;
        m.reserve(n);
        const auto t0 = Clock::now();
        for (const std::uint64_t k : keys.present) {
            m.insert(k, k);
        }
        benchmark::ClobberMemory();
        state.SetIterationTime(seconds(Clock::now() - t0));
        benchmark::DoNotOptimize(m.size());
    }
    finish(state, n);
}

// Look up every present key, in shuffled order.
template <class Map>
void find_hit(benchmark::State& state) {
    const std::size_t n = size_arg(state);
    const KeySet& keys = key_set(n);
    const Map m = build<Map>(keys.present);
    for (auto _ : state) {
        const auto t0 = Clock::now();
        std::uint64_t sum = 0;
        for (const std::uint64_t k : keys.order) {
            sum += *m.find(k);
        }
        benchmark::DoNotOptimize(sum);
        state.SetIterationTime(seconds(Clock::now() - t0));
    }
    finish(state, n);
}

// Look up n keys that are not in the table.
template <class Map>
void find_miss(benchmark::State& state) {
    const std::size_t n = size_arg(state);
    const KeySet& keys = key_set(n);
    const Map m = build<Map>(keys.present);
    for (auto _ : state) {
        const auto t0 = Clock::now();
        std::size_t found = 0;
        for (const std::uint64_t k : keys.absent) {
            found += m.find(k) != nullptr;
        }
        benchmark::DoNotOptimize(found);
        state.SetIterationTime(seconds(Clock::now() - t0));
    }
    finish(state, n);
}

// Erase every key, in shuffled order, from a fresh copy of a full table.
template <class Map>
void erase_all(benchmark::State& state) {
    const std::size_t n = size_arg(state);
    const KeySet& keys = key_set(n);
    const Map full = build<Map>(keys.present);
    for (auto _ : state) {
        Map m = full;
        const auto t0 = Clock::now();
        std::size_t erased = 0;
        for (const std::uint64_t k : keys.order) {
            erased += m.erase(k);
        }
        benchmark::DoNotOptimize(erased);
        state.SetIterationTime(seconds(Clock::now() - t0));
    }
    finish(state, n);
}

// Steady size n: erase the oldest key, insert a fresh one. Tombstones build up
// until the growth policy rebuilds the table. One operation = one erase plus one
// insert. The table persists across iterations, so later iterations run on an
// aged table, as a long-running service would.
template <class Map>
void churn(benchmark::State& state) {
    const std::size_t n = size_arg(state);
    const KeySet& keys = key_set(n);
    Map m = build<Map>(keys.present);
    std::vector<std::uint64_t> ring = keys.present;  // live keys, oldest at pos
    std::size_t pos = 0;
    FreshKeys fresh;
    std::vector<std::uint64_t> incoming(n);
    for (auto _ : state) {
        for (auto& k : incoming) {
            k = fresh.next();
        }
        const auto t0 = Clock::now();
        for (const std::uint64_t k : incoming) {
            m.erase(ring[pos]);
            m.insert(k, k);
            ring[pos] = k;
            pos = pos + 1 == n ? 0 : pos + 1;
        }
        benchmark::ClobberMemory();
        state.SetIterationTime(seconds(Clock::now() - t0));
    }
    finish(state, n);
}

// Read-heavy mix at steady size n: 90% successful lookups of random live keys,
// 10% churn steps. The operation sequence is drawn once, outside the timing.
template <class Map>
void mixed(benchmark::State& state) {
    const std::size_t n = size_arg(state);
    const KeySet& keys = key_set(n);
    Map m = build<Map>(keys.present);
    std::vector<std::uint64_t> ring = keys.present;
    std::size_t pos = 0;
    FreshKeys fresh;

    // ops[i] < n: look up ring[ops[i]]. ops[i] == n: churn step.
    std::vector<std::size_t> ops(n);
    std::mt19937_64 rng(kSeed + 2);
    std::size_t churn_steps = 0;
    for (auto& op : ops) {
        if (rng() % 10 == 0) {
            op = n;
            ++churn_steps;
        } else {
            op = static_cast<std::size_t>(rng() % n);
        }
    }
    std::vector<std::uint64_t> incoming(churn_steps);

    for (auto _ : state) {
        for (auto& k : incoming) {
            k = fresh.next();
        }
        const auto t0 = Clock::now();
        std::uint64_t sum = 0;
        std::size_t next_in = 0;
        for (const std::size_t op : ops) {
            if (op < n) {
                sum += *m.find(ring[op]);
            } else {
                const std::uint64_t k = incoming[next_in++];
                m.erase(ring[pos]);
                m.insert(k, k);
                ring[pos] = k;
                pos = pos + 1 == n ? 0 : pos + 1;
            }
        }
        benchmark::DoNotOptimize(sum);
        state.SetIterationTime(seconds(Clock::now() - t0));
    }
    finish(state, n);
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

// 2^10 to 2^22 keys. At 2^22, R0 already uses 128 MB, eight times the L3.
constexpr std::int64_t kMinN = 1 << 10;
constexpr std::int64_t kMaxN = 1 << 22;

template <class Map>
void register_table(const std::string& table) {
    const auto add = [&](const std::string& workload, void (*fn)(benchmark::State&)) {
        benchmark::RegisterBenchmark((workload + "/" + table).c_str(), fn)
            ->RangeMultiplier(4)
            ->Range(kMinN, kMaxN)
            ->UseManualTime()
            ->Unit(benchmark::kNanosecond);
    };
    add("insert_grow", insert_grow<Map>);
    add("insert_reserved", insert_reserved<Map>);
    add("find_hit", find_hit<Map>);
    add("find_miss", find_miss<Map>);
    add("erase_all", erase_all<Map>);
    add("churn", churn<Map>);
    add("mixed", mixed<Map>);
}

}  // namespace

int main(int argc, char** argv) {
    register_table<swiss::LinearProbingMap<>>("R0_linear");
    register_table<swiss::adapters::StdUnorderedMap<>>("std_unordered");

    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
