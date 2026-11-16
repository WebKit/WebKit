// Copyright 2025 The BoringSSL Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <charconv>
#include <string_view>

#include <benchmark/benchmark.h>
#include <openssl/base.h>

#include "./internal.h"

BSSL_NAMESPACE_BEGIN

namespace bench {
namespace {
bool flags_parsed = false;

std::vector<void (*)()> &GetBenchmarkRegistry() {
  static std::vector<void (*)()> registry;
  return registry;
}

std::vector<int64_t> &GetInputSizesMut() {
  static std::vector<int64_t> input_sizes;
  return input_sizes;
}

std::vector<int> &GetNumThreadsList() {
  static std::vector<int> num_threads;
  return num_threads;
}
}  // namespace

void SetThreads(benchmark::Benchmark *bench) {
  if (!flags_parsed) {
    fprintf(stderr,
            "Benchmark %s attempts to set thread count before flag parsing is "
            "complete. Please use `BSSL_BENCH_LAZY_REGISTER` to register "
            "benchmarks so that the flag parsing happens before benchmark "
            "registrations.\n",
            bench->GetName());
    abort();
  }
  const auto &num_threads = GetNumThreadsList();
  if (num_threads.empty()) {
    bench->Threads(1)->ThreadPerCpu();
  } else {
    for (auto thread_count : num_threads) {
      bench->Threads(thread_count);
    }
  }
}

int RegisterBenchmark(void (*handle)()) {
  GetBenchmarkRegistry().push_back(handle);
  return 0;
}

Span<const int64_t> GetInputSizes(benchmark::Benchmark *bench) {
  if (!flags_parsed) {
    fprintf(stderr,
            "Benchmark %s attempts to set thread count before flag parsing is "
            "complete. Please use `BSSL_BENCH_LAZY_REGISTER` to register "
            "benchmarks so that the flag parsing happens before benchmark "
            "registrations.\n",
            bench->GetName());
    abort();
  }
  return GetInputSizesMut();
}
}  // namespace bench
BSSL_NAMESPACE_END

namespace {
void PrintHelp() {
  printf(
      "Usage: benchmark [[-t <N>]|[--threads <N>]...] "
      "[[-i <N>]|[--input-size <N>]...]\n\n"
      "Flags:\n"
      "\t-i <N> (input size in bytes), --input-size <N>\t\tset the size of "
      "input to process, in bytes\n"
      "\t-t <N>, --threads <N>\t\t\t\t\tsets the thread count in parallel "
      "tests\n\n\n"
      "Here follow the Google Benchmark flags.\n\n");
  benchmark::PrintDefaultHelp();
}
}  // namespace

int main(int argc, char **argv) {
  benchmark::MaybeReenterWithoutASLR(argc, argv);
  if (argc < 1) {
    fprintf(stderr, "FATAL: insufficient number of program arguments");
    return 1;
  }
  benchmark::Initialize(&argc, argv, PrintHelp);
  // Parse our benchmark-specific flags.
  for (int i = 1; i < argc; ++i) {
    auto carg = argv[i];
    auto arg = std::string_view(carg);
    if (arg == "--threads" || arg == "-t") {
      if (++i >= argc) {
        fprintf(stderr,
                "%s: `--threads` expected an integer argument as the number "
                "of threads.\n",
                argv[0]);
        return 1;
      }
      int threads;
      carg = argv[i];
      size_t arglen = strlen(carg);
      auto result = std::from_chars(carg, carg + arglen, threads);
      if (result.ec != std::errc() || result.ptr != carg + arglen ||
          threads <= 0) {
        fprintf(stderr, "%s: expected a positive integer, got %s.\n", argv[0],
                carg);
        return 1;
      }

      fprintf(stderr, "%s: use %d threads.\n", argv[0], threads);
      bssl::bench::GetNumThreadsList().push_back(threads);
    } else if (arg == "--input-size" || arg == "-i") {
      if (++i >= argc) {
        fprintf(stderr,
                "%s: `--input-size` expected a positive integer argument as "
                "the input size as bytes.\n ",
                argv[0]);
        return 1;
      }
      carg = argv[i];
      size_t arglen = strlen(carg);
      int64_t input_size;
      auto result = std::from_chars(carg, carg + arglen, input_size);
      if (result.ec != std::errc() || result.ptr != carg + arglen ||
          input_size <= 0) {
        fprintf(stderr, "%s: expected a positive integer, got %s.\n", argv[0],
                carg);
        return 1;
      }

      fprintf(stderr, "%s: use input size %" PRId64 " bytes.\n", argv[0],
              input_size);
      bssl::bench::GetInputSizesMut().push_back(input_size);
    } else {
      fprintf(stderr, "%s: unexpected flag `%s`.\n", argv[0], carg);
      return 1;
    }
  }
  bssl::bench::flags_parsed = true;

  auto &registry = bssl::bench::GetBenchmarkRegistry();
  for (auto register_handle : registry) {
    register_handle();
  }
  registry.clear();
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
  return 0;
}
