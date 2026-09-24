# Hash-quality experiment: phase 1 results

**Run:** 2026-09-24. The hypotheses in [hash-quality.md](hash-quality.md) were committed first, in `17ffa96`, before the tool had ever run. Drafted by Claude. **The author's review is pending.**

**Data:** `results/hash_quality/distribution.csv`, `avalanche.csv` and `bench_hash.json`. Reproduce with `./build/swiss_hash_quality results/hash_quality`. The distribution and avalanche numbers are deterministic counts, fixed by seed. The timings are indicative only: power plan Balanced, clock not pinned. Environment as in the README.

## Verdicts

| # | Hypothesis | Stated confidence | Verdict |
|---|---|---|---|
| 1 | Uniform keys hide hash quality | High | **Confirmed.** Every hash, identity included, gives random-hash values |
| 2 | Identity fails through H2 | High | **Confirmed exactly.** For example, sequential at 2^10: 896 starts, max 1, chi2 0.125, one H2 value, false-match rate 1 |
| 3 | Multiplicative clusters on strided keys | High for starts, medium for H2 | **Starts confirmed exactly.** H2 on sequential keys diverged: the false-match rate was about 0, not 1/128 |
| 4 | Folded multiply looks random | Low | **Wrong at small capacities.** See finding 3 |
| 5 | Tabulation looks random, pair metrics included | High | **Confirmed** |
| 6 | splitmix64 looks random except on its colliding keys | High / certain | **Confirmed.** Colliding keys: 1 start, 1 H2 value, false-match rate 1 |
| 7 | Avalanche | Mixed | Identity and splitmix64 confirmed. **Multiplicative wrong despite high confidence**, see finding 5. Tabulation's mean confirmed and its max underestimated, see finding 6. Folded multiply wrong |
| 8 | Latency order | Medium, low for tabulation | **Order confirmed.** Tabulation is the slowest |
| 9 | Tabulation costs more inside a table | n/a | Deferred to phase 2 |

## Selected numbers

Load 7/8. A random hash gives starts ≈ 0.583 × capacity, chi2 ≈ 1 and a false-match rate ≈ 0.0078.

| Hash, key set | Capacity | Starts | Max per start | chi2 ratio | H2 values | False-match rate |
|---|---|---|---|---|---|---|
| identity, sequential | 2^20 | 917,504 | 1 | 0.125 | 1 | 1.0000 |
| identity, stride 4096 | 2^20 | 256 | 3,584 | 3,583 | 1 | 1.0000 |
| multiplicative, sequential | 2^20 | 917,504 | 1 | 0.125 | 128 | 0.0000 |
| multiplicative, stride 128 | 2^20 | 8,192 | 112 | 111 | 128 | 0.0067 |
| folded multiply, stride 4096 | 2^10 | 691 | 2 | 0.583 | 128 | 0.0260 |
| folded multiply, stride 128 | 2^20 | 626,447 | 8 | 0.925 | 128 | 0.0119 |
| tabulation, stride 4096 | 2^20 | 612,409 | 9 | 0.997 | 128 | 0.0077 |
| splitmix64, colliding | 2^20 | 1 | 917,504 | 917,504 | 1 | 1.0000 |

Avalanche, mean bias per output bit, averaged over the bits each part of the split reads. 0 is ideal, and the noise floor is about 0.0013.

| Hash | H2 bits 57–63 | H1 bits 0–19 | Worst single cell |
|---|---|---|---|
| identity | 0.500 | 0.500 | 0.500 |
| multiplicative | 0.271 | 0.462 | 0.500 |
| folded multiply | 0.252 | 0.252 | 0.495 |
| tabulation | 0.036 | 0.035 | 0.164 |
| splitmix64 | 0.0012 | 0.0012 | 0.0065 |

Cost, median of 5 repetitions. The cycle counts assume about 4.1 GHz, the rate implied by the identity loop running one iteration per cycle.

| Hash | Latency | ≈ cycles | Throughput |
|---|---|---|---|
| identity | 0.24 ns | 1, loop overhead | 7.98 G/s |
| multiplicative | 0.70 ns | 3, one multiply | 3.97 G/s |
| folded multiply | 1.18 ns | 5, 128-bit multiply + xor | 2.26 G/s |
| splitmix64 | 2.82 ns | 12, 2 multiplies + 3 shift-xor pairs | 1.09 G/s |
| tabulation | 3.25 ns | 13 | 0.61 G/s |

## Findings

**1. Multiplicative's H1 is identity's H1 with the slots renumbered.** Every H1 metric is identical for the two hashes on every key set. On uniform keys, for example, both give 583, 38,393 and 611,309 starts. The reason: (k·C) mod 2^m depends only on k mod 2^m, and multiplying by an odd C permutes the residues. Under the top-7 split, a multiplicative hash cannot spread H1 any better than no hash at all. It only improves H2.

**2. Structured keys through a multiplicative hash can beat random.** Multiplicative on sequential keys gives a false-match rate between 0.0000 and 0.0006, against 0.0078 for a random hash. Folded multiply on strided keys gives a chi2 ratio as low as 0.48, more even than random. Likely mechanism, not verified: k·C for consecutive k forms a Weyl sequence, whose points are spread more evenly than random points. Two keys that land close together in the H1 bits then rarely also agree in the H2 bits. This only holds for regular key sets like these.

**3. Folded multiply is not random-looking at small capacities.** The false-match rate reaches 0.026, 3.3 times random, for stride 4096 at 2^10. The chi2 ratio ranges from 0.48 to 0.94. At 2^20 most values are close to random, but stride 128 still has a false-match rate 1.5 times random. Its failures are milder than those of identity or multiplicative, but they are real.

**4. Avalanche and table quality disagree.**
- Folded multiply has a mean bias of 0.25 in every bit, the worst of any hash except identity. Yet its H1/H2 spread is close to random.
- Tabulation has a clear bias, mean 0.035. Yet its spread cannot be told apart from random on any key set.

Avalanche measures how one fixed function responds to one-bit changes in its input. It is neither necessary nor sufficient for spreading a particular key set well.

**5. A single multiply fails avalanche even in its strong top bits.** Hypothesis 7 expected low bias in the top bits. It measured 0.26 to 0.28.
- Flipping input bit i adds the fixed constant 2^i·C to the product.
- Whether output bit j then flips depends only on that constant and on one carry. So it flips with a fixed probability, not a probability of 1/2.
- If that fixed probability is spread like a random number between 0 and 1, the expected |p − 0.5| is 0.25, which matches the measurement.

Folded multiply adds a fixed constant to the 128-bit product in the same way, and shows the same 0.25.

**6. Tabulation's worst cell: predicted about 0.1, measured 0.16.** The per-cell spread of 0.5 / √128 ≈ 0.044 was right. The mean |deviation| it implies, 0.044 × √(2/π) ≈ 0.035, matches the measured 0.0354. The error was in the maximum. The worst of 4096 cells lies about 3.7 standard deviations out, which is 0.16, not 0.1.

**7. Tabulation is the slowest hash even at its best.** Its lookup tables stay hot in L1 here, yet its throughput is 1.8 times lower than splitmix64's. A likely cause, not verified: each of the 8 key bytes needs a shift, a byte extract, a load and an xor, which is more instructions per key than splitmix64. Phase 2 will add the cache pressure on top.
