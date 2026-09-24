# AI failures

Record of mistakes by any AI tool, corrections and rejected suggestions. Appended as they happen, never reconstructed later. Feeds the "AI use" section of the report.

## 2026-09-23: Google Benchmark failed to build under GCC 16

- **Asked:** write a `CMakeLists.txt` for the project, pulling in GoogleTest and Google Benchmark with FetchContent.
- **What went wrong:** Claude pinned Google Benchmark to v1.9.1 and left its default of building with `-Werror`. It never tried the build, because CMake was not installed at the time. Updating MSYS2 to install CMake also moved GCC from 14.2 to 16.2. GCC 16 raises a `-Wswitch` warning in Benchmark's `src/sysinfo.cc`, a missing `CacheUnknown` case, and `-Werror` turned that warning into a build failure.
- **How it was found:** a test build with placeholder source files, run by Claude after CMake was installed. The first attempt failed for an unrelated reason, a build path over Windows' 260-character limit caused by Claude's long scratch directory. The second attempt showed the real error.
- **What was done:** set `BENCHMARK_ENABLE_WERROR OFF` in `CMakeLists.txt`. The warning now appears but no longer stops the build. Configure, build, `ctest` and the benchmark binary all ran successfully afterwards.
- **Lesson:** a pinned dependency is only known to work with the compilers it was tested against. The build file should have been tested before being presented as done.

## 2026-09-23: Unsupported claim written into the README

- **Asked:** record the toolchain choice, MinGW GCC or MSVC, in the README.
- **What went wrong:** Claude's first draft justified GCC by saying results would "compare more directly with published Abseil and hashbrown numbers". There was no basis for this. hashbrown is Rust, compiled through LLVM, and Abseil's published benchmarks are commonly built with Clang, not GCC.
- **How it was found:** Claude's own re-read of the draft, before commit.
- **What was done:** the clause was removed. The README now gives only the supported reason, that flags and code generation carry over to a Linux GCC run.
- **Lesson:** plausible-sounding justifications get written in passing. Every factual claim in a document needs a source or a check.

## 2026-09-23: Miscounted references in the plan

- **Asked:** plan the day-1 tasks, including renaming the AI log.
- **What went wrong:** the plan said `CLAUDE.md` had three references to `AI_USE_LOG.md`. It had one. The count was stated without searching the file.
- **How it was found:** a search of `CLAUDE.md` during implementation.
- **What was done:** updated the single reference. No consequence beyond the wrong statement.

## 2026-09-24: Out-of-date description of Abseil's H1/H2 split

- **Asked:** how to choose a hash function, and which hash bits the table should use for H2.
- **What went wrong:** two errors, both presented as current fact.
  1. Claude said Abseil takes H2 from the low 7 bits and H1 from `hash >> 7`. That was true up to release 20250512. Since release 20250814, Abseil uses H1 = the whole hash and H2 = the top 7 bits, the same split as hashbrown. The author chose "Abseil's split" partly because Claude said it matched the Abseil design notes, and that reason rested on the out-of-date fact.
  2. Claude said H1 is masked to the number of groups. In old Abseil, current Abseil and hashbrown, H1 is masked to the number of slots. Probing starts at any slot and reads 16 control bytes from there.
- **How it was found:** the verification step written into the plan. It read `raw_hash_set.h` at Abseil master and at eight release tags, plus hashbrown's `src/control/tag.rs` and `src/raw.rs`. The Abseil commit that made the change explains it: "change H2 to use the most significant 7 bits - saving 1 cycle in H1. Using Mix instead of WeakMix means that the entire 64 bits of hash are expected to have good entropy."
- **What was done:** the split decision was reopened with the author before any code depending on it was written. The plan's colliding-key generator and hash-quality metrics were changed to use slot indices, not group indices.
- **Lesson:** knowledge of fast-moving libraries goes out of date. A claim about a library's internals needs a version attached, and it should be checked before a decision rests on it.
