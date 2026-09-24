# CLAUDE.md

Context for Claude Code when working in this repository. Read this before making changes.

## Project summary

- **What:** An implementation of a Swiss-table hash map, the open-addressing design used by Abseil's `flat_hash_map` and Rust's `hashbrown`.
- **Why:** Programming Assignment 1 for an Advanced Algorithms subject at UTS. Individual project.
- **Topic approval:** Swiss table is off the suggested topic list and was approved by the tutor, Troy. The earlier choice was a van Emde Boas tree, now dropped.
- **Due:** 27 September 2026.
- **Secondary goal:** The repo should stand up as a GitHub portfolio piece aimed at quantitative development roles, not just satisfy the marking rubric. Performance awareness, clean benchmarking and clear engineering decisions matter more than feature count.

## Author

- Laith, fourth-year Software Engineering student at UTS, Sydney. Final semester.
- Strong C++ algorithms background from competitive programming: around 500 LeetCode problems, then Codeforces.
- Prefers abstract technical work such as algorithms, maths and low-level systems.
- Works on a **Windows PC** for this project. Previous C++ setup used VS Code with MSYS2, so assume GCC via MSYS2 unless told otherwise. Commands and scripts must work on Windows.

## Open decisions

These have not been confirmed. Ask rather than assume if a task depends on them.

- **Track.** A Swiss table fits Track A best, an empirical study against baselines. Track C is possible if the focus shifts to correctness of probing, deletion and resizing.
- **Language.** C++ is the likely choice given the CP background and the quant target. Not yet confirmed.
- **Baselines.** Likely `std::unordered_map`, plus possibly a simple linear-probing table and `absl::flat_hash_map` as a reference point.

## Assignment requirements

Effort is roughly 20 hours over three weeks. The implementation is the foundation, not the whole project. Most of the mark comes from what is built on top of it.

### Deliverables

1. A working implementation in one repository, with a README explaining how to build and run it.
2. The track-specific artifact.
   - Track A: benchmarking harness, data generation and results.
   - Track B: a working tool with a real CLI or GUI.
   - Track C: a correctness commentary.
3. A written report with four sections.
   - **What I built:** algorithm, language, data representation, simplifications or extensions relative to the textbook version, and how to navigate the code.
   - **Track writeup:** for Track A, what was compared, hypotheses, data generation, measurement method, results with plots, and analysis of why results came out as they did.
   - **What I learned:** specific surprises and initial mistakes. Concrete examples carry far more weight than general statements.
   - **AI use:** see below.
4. A 3–5 minute screencast, single take, no editing. It must cover:
   - where the core data structure lives and how it is organised
   - the hardest part to get right, and why
   - one invariant: stated, where it is established, where it is relied on
   - one "what breaks" example: a specific line or step and what goes wrong if it changes

### Grading

| Component | Weight |
|---|---|
| Implementation correctness and quality, including edge cases | 25% |
| Track-specific work | 45% |
| Report quality, especially depth of "what I learned" | 20% |
| AI use disclosure | 10% |

## AI use rules for this repo

AI use is permitted and expected, but the report must disclose it honestly and the author must be able to explain every part of the code on video and in a possible live follow-up. This changes how Claude should work here.

- **Explain, don't just produce.** For any non-trivial code, state the invariant it maintains and why each tricky step is needed. Keep this brief in code comments and fuller in chat.
- **Flag uncertainty.** Separate what is known to be correct, what is assumed, and what is a guess. Say so when a benchmark might not measure what it claims to.
- **Maintain `ai-failures.md`.** Append an entry whenever Claude or any other AI tool:
  - produces a bug, a wrong explanation, a hallucinated API or a flawed benchmark that later gets caught
  - is corrected by the author
  - suggests something the author rejects, with the reason
  Each entry: date, what was asked, what went wrong, how it was found, what was done. The report needs at least two such examples, so do not hide or smooth over mistakes.
- **Do not write the "What I learned" section.** It must reflect the author's own understanding. Claude may help structure it or check it for accuracy.

## Technical notes on Swiss tables

Reference guidance, not settled design. Verify against the Abseil design notes and the CppCon 2017 talk by Matt Kulukundis before relying on specifics.

- **Layout:** a control-byte array alongside a slot array. Each control byte is empty, deleted, a sentinel, or holds the 7-bit H2 fragment of a full slot's hash.
- **Hash split:** H1 selects the starting slot, not a group: the hash masked to the capacity, with 16 control bytes read from there. H2 is stored in the control byte for fast filtering. **Decided:** top-7 split, H2 = bits 57 to 63 and H1 = the whole hash. Current Abseil, since release 20250814, and hashbrown both use it. Abseil up to release 20250512 used H2 = low 7 bits and H1 = hash >> 7, checked in the source. Defined once in `include/swiss/hash.hpp`.
- **Group probing:** scan a group of 16 control bytes at once with SSE2, or 8 with a portable SWAR fallback. Compare all bytes against H2 in parallel, then check keys only for matching positions.
- **Probe sequence:** triangular probing over groups, which visits every group when capacity is a power of two.
- **Deletion:** use a tombstone unless the group was never full, in which case the slot can go straight back to empty. Getting this condition wrong breaks lookups for keys further along the probe sequence. This is a strong candidate for the "what breaks" video segment.
- **Growth:** maximum load factor around 7/8. Rehash when full plus deleted slots exceed the threshold, so tombstones cannot degrade probing indefinitely.
- **Hash quality:** the design depends on well-mixed low and high bits. `std::hash<int>` is the identity on common implementations and will break the H1/H2 split. Use a mixing step.

## Benchmarking guidance for Track A

- Separate workloads: successful lookup, unsuccessful lookup, insert, erase, mixed, and erase-heavy churn to stress tombstones.
- Vary key type and size, table size relative to L1, L2, L3 and RAM, and load factor.
- Use a proper harness such as Google Benchmark or nanobench. Guard against dead-code elimination and warm-up effects. Report variance, not just means.
- Record compiler, flags, CPU and OS with every result set. Windows timer and power-state behaviour can distort results, so note the power plan.
- State a hypothesis before each experiment and check it afterwards. Divergence from theory is the interesting part and feeds the "what I learned" section.

## Portfolio standards

- Clean build with CMake. One command to build, one to test, one to benchmark.
- Unit tests covering edge cases: empty table, single element, repeated insert of the same key, erase then reinsert, erase of a missing key, growth across several resizes, adversarial collisions, many tombstones.
- Property-based or randomised differential testing against `std::unordered_map`.
- Sanitiser runs where the toolchain supports them.
- README with results summary and plots, not just build instructions.

## Communication preferences

Apply these to chat replies, comments, docs and any report text.

- Australian English spelling.
- Direct answers with logical structure. No fluff, no reassurance, no patronising tone.
- Avoid semicolons, heavy use of parentheses, long run-on sentences, overly formal language and unnecessary complex vocabulary. Avoid first-person framing in written material.
- Explain from first principles. Derive rather than state. Explain why alternatives are worse and what the tradeoffs are.
- Keep facts, assumptions, speculation and probabilities clearly separated.
- Disagree when justified rather than going along with a flawed idea.
- When rewriting text: preserve meaning, markdown and quoted text exactly. Improve grammar and clarity and cut repetition.
