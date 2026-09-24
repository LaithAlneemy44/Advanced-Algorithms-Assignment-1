# Hash-quality experiment: hypotheses

**Status:** hypotheses for phase 1, the hash-level analysis. Written before any measurement and committed before any result file, so the git history shows the order. Drafted by Claude from predictions derived in discussion on 2026-09-24. **The author's review is pending.**

## Setup

- **Hashes:** identity, multiplicative (`k * 0x9E3779B97F4A7C15`, low 64 bits), folded multiply (same 128-bit product, high half xor low half), simple tabulation, splitmix64. Code: `include/swiss/hash_candidates.hpp`, `include/swiss/hash.hpp`.
- **Split:** top-7. H2 = bits 57 to 63. H1 = hash masked to the capacity, the slot where probing starts.
- **Load:** 7/8. n = 7/8 × capacity keys.
- **Capacities:** 2^10, 2^16, 2^20 slots.
- **Key sets:** uniform random, sequential from 0, stride 128, stride 4096, and keys that collide under splitmix64. Code: `bench/keys.hpp`.
- **Tool:** `swiss_hash_quality` (`bench/hash_quality.cpp`).

## Metrics and their values for an ideal random hash

| Metric | Meaning | Random hash at load 7/8 |
|---|---|---|
| `distinct_starts` | slots where at least one key starts | 1 − e^(−7/8) ≈ 0.583 × capacity |
| `max_per_start` | most keys starting at one slot | small, around 5 to 9 |
| `chi2_ratio` | spread of keys per slot against uniform | ≈ 1. Below 1 is more even than random, far above 1 is clustered |
| `h2_false_match` | of key pairs starting in the same 16-slot window, fraction sharing H2 | 1/128 ≈ 0.0078 |
| avalanche `mean_bias` | per output bit, mean over input bits of \|P(flip) − 0.5\| | noise only, standard error 0.0016 |

A derived value that comes up below: if every slot holds 0 or 1 keys, the chi-squared ratio is 1 − 7/8 = 0.125. Such keys are spread more evenly than random.

## Hypotheses

### 1. Uniform random keys hide hash quality

Every hash, identity included, gives random-hash values on uniform keys. The keys are already random, so even no mixing looks perfect. **Confidence: high.**

### 2. Identity fails through H2, not H1, under the top-7 split

- **Sequential keys:** every key gets its own slot, so `distinct_starts` = n, `max_per_start` = 1 and `chi2_ratio` ≈ 0.125. That is better than random. But every key is below 2^57, so H2 = 0 for all of them: `distinct_h2` = 1 and `h2_false_match` = 1. **Confidence: high, derived exactly.**
- **Stride 128:** starts only at multiples of 128, so `distinct_starts` = capacity / 128 with 112 keys at each. H2 = 0, so `h2_false_match` = 1. Both failure modes at once. **Confidence: high.**
- **Stride 4096:** `distinct_starts` = max(1, capacity / 4096). At capacity 2^10, every key starts at slot 0. **Confidence: high.**

### 3. Multiplicative fails through H1 on strided keys

- **Sequential keys:** since the multiplier is odd, k → k·C mod capacity is a bijection, so the spread matches identity: n distinct starts, `chi2_ratio` ≈ 0.125. H2 comes from the strong top bits, so `h2_false_match` ≈ 1/128. **Confidence: high for the starts. Medium for H2, since the top bits of a Weyl sequence k·C are evenly spread, but nearby starts may correlate with H2 in ways this derivation misses.**
- **Stride 128:** the low 7 bits of every product are 0, so `distinct_starts` = capacity / 128 with 112 keys each. That is the same clustering as identity. H2 stays good, near 1/128. **Confidence: high for the starts, medium for H2.**
- **Stride 4096:** `distinct_starts` = max(1, capacity / 4096). **Confidence: high.**

### 4. Folded multiply looks random on every key set

The high half of the product depends on every key bit through the carries, so xoring it in should repair the low bits. **Confidence: low.** For small keys the high half is about 0.618 × k, a slowly growing sequence. Its xor with the low half is not obviously uniform in the low bits.

### 5. Tabulation looks random on every key set, including in the pair metrics

`chi2_ratio` and `h2_false_match` depend only on pairs of keys, and tabulation is 3-independent, so pairs behave as under a truly random hash. **Confidence: high.**

### 6. splitmix64 looks random except on the keys built to collide with it

- Structured key sets: random-hash values. **Confidence: high.**
- Colliding keys: `distinct_starts` = 1, `max_per_start` = n, `distinct_h2` = 1, `h2_false_match` = 1. **Confidence: certain, since the unit tests already check it.**
- The same colliding keys under the other four hashes: random-hash values. The keys only look random. **Confidence: high.**

### 7. Avalanche

- **Identity:** bias 0.5 in every cell. Output bit j flips only when input bit j flips, and then always. **Certain.**
- **Multiplicative:** output bit j never responds to input bits above j, so mean bias for bit j is at least 0.5 × (63 − j) / 64. That is about 0.49 for bit 0, falling towards the top. The H2 bits, 57 to 63, have low mean bias but max bias 0.5. **Confidence: high.**
- **splitmix64:** mean and max bias at noise level, max about 0.007. **Confidence: high.**
- **Tabulation:** clear bias, mean around 0.035, max around 0.1, despite its strong guarantees. Flipping one key bit swaps one table entry for another. Over the 256 values of that byte there are only 128 distinct entry pairs, so each cell averages just 128 random bits, giving a spread of about 0.5 / √128 ≈ 0.044. Avalanche measures one fixed function. Tabulation's guarantees are about randomly chosen tables. **Confidence: medium.**
- **Folded multiply:** low mean bias, but some cells may be strongly biased, especially for the top input bits. **Confidence: low.**

### 8. Cost, from the microbenchmarks

Latency, lowest first: identity, which is loop overhead only, then multiplicative, then folded multiply, then splitmix64 at about 3 ns, which was already measured. Tabulation's position is open. Eight parallel L1 loads plus an xor tree should come to about 10 cycles, similar to splitmix64. **Confidence: medium for the order, low for tabulation.**

### 9. Deferred to phase 2, inside the table

Tabulation costs more inside a small table than in the microbenchmark, because its 16 KB of lookup tables compete with the hash table for the 32 KB L1. **Not testable in phase 1.**
