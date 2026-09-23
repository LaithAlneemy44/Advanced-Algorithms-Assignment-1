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
