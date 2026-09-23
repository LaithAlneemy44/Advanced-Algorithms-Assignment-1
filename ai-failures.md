# AI use log

Record of AI mistakes, corrections and rejected suggestions. Feeds the "AI use" section of the report.

## 2026-09-23: Google Benchmark failed to build under GCC 16

- **Asked:** write a `CMakeLists.txt` for the project, pulling in GoogleTest and Google Benchmark with FetchContent.
- **What went wrong:** Claude pinned Google Benchmark to v1.9.1 and left its default of building with `-Werror`. It never tried the build, because CMake was not installed at the time. Updating MSYS2 to install CMake also moved GCC from 14.2 to 16.2. GCC 16 raises a `-Wswitch` warning in Benchmark's `src/sysinfo.cc`, a missing `CacheUnknown` case, and `-Werror` turned that warning into a build failure.
- **How it was found:** a test build with placeholder source files, run by Claude after CMake was installed. The first attempt failed for an unrelated reason, a build path over Windows' 260-character limit caused by Claude's long scratch directory. The second attempt showed the real error.
- **What was done:** set `BENCHMARK_ENABLE_WERROR OFF` in `CMakeLists.txt`. The warning now appears but no longer stops the build. Configure, build, `ctest` and the benchmark binary all ran successfully afterwards.
- **Lesson:** a pinned dependency is only known to work with the compilers it was tested against. The build file should have been tested before being presented as done.
