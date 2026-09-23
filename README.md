# Advanced-Algorithms-Assignment-1

A Swiss-table hash map in C++20, the open-addressing design behind Abseil's `flat_hash_map` and Rust's `hashbrown`, benchmarked against baseline hash maps. Programming Assignment 1 for Advanced Algorithms at UTS.

**Status:** in progress. The build, the shared hash function and its tests are in place. The table itself is not written yet.

## Build, test, benchmark

Requires CMake 3.24 or later, Ninja and a C++20 compiler. GoogleTest and Google Benchmark are downloaded at configure time.

```
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
./build/swiss_bench
```

With no build type given, the build defaults to Release.

| CMake option | Default | Effect |
|---|---|---|
| `SWISS_BUILD_TESTS` | `ON` | Build `swiss_tests` |
| `SWISS_BUILD_BENCHMARKS` | `ON` | Build `swiss_bench` |
| `SWISS_SANITIZE` | `OFF` | Sanitisers for the tests. AddressSanitizer and UBSan where available. On MinGW, UBSan in trap mode only. |
| `SWISS_NATIVE` | `OFF` | Add `-march=native`. Off by default so results do not depend on one CPU's instruction set. |

## Layout

| Path | Contents |
|---|---|
| `include/swiss/hash.hpp` | `splitmix64_mix` and `SplitMix64Hash`, the one hash used by every structure |
| `tests/` | GoogleTest unit tests |
| `bench/` | Google Benchmark benchmarks |
| `ai-failures.md` | Log of AI mistakes and corrections, kept as they happen |

## Hash function

Every structure in the study uses the same hash, `swiss::SplitMix64Hash`: the splitmix64 finaliser. Hash quality is varied only in its own experiment, never as an uncontrolled difference between structures.

The finaliser is a bijection on 64-bit values, so distinct keys never share a full hash. Collisions only come from reducing the hash to a group index and a control-byte fragment. The tests check the implementation against Vigna's reference code and check the bijection by inverting it.

## Environment

Recorded with every result set.

| | |
|---|---|
| Compiler | GCC 16.2.0, MSYS2 MinGW-w64 (Rev4) |
| Build tools | CMake 4.4.3, Ninja 1.13.2 |
| Flags | `-O3 -DNDEBUG -std=c++20`, no `-march` |
| CPU | AMD Ryzen 5 5600G, 6 cores / 12 threads, 3893 MHz as reported by Google Benchmark |
| Caches | L1d 32 KB per core, L2 512 KB per core, L3 16 MB shared |
| OS | Windows 11 Pro 10.0.26200 |
| Power plan | Balanced |

**Why MinGW GCC rather than MSVC.** GCC is the same compiler family used on Linux, so flags, intrinsics and code generation carry over to a Linux run without changes. MSVC's one advantage here is AddressSanitizer on Windows. MinGW ships no ASan or UBSan runtime, so ASan runs are planned under WSL with the same CMake build instead.
