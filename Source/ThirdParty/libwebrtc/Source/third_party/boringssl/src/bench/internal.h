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

#ifndef OPENSSL_HEADER_BENCH_BENCH_H
#define OPENSSL_HEADER_BENCH_BENCH_H

#include <stdint.h>

#include <benchmark/benchmark.h>

#include <openssl/base.h>
#include <openssl/span.h>

BSSL_NAMESPACE_BEGIN

namespace bench {

// Register a function to register benchmarks after the runtime flags are
// parsed.
//
// It is recommended to use `BSSL_BENCH_LAZY_REGISTER` for the registration,
// so that you do not have to manually wire up static initialisation with
// this function.
int RegisterBenchmark(void (*handle)());

#define BSSL_BENCH_MERGE(a, b) a##b
#define BSSL_BENCH_LABEL(a, unique) BSSL_BENCH_MERGE(a, unique)
#define BSSL_UNIQUE_NAME(a) BSSL_BENCH_LABEL(a, __LINE__)

#define BSSL_BENCH_LAZY_REGISTER_INNER(func_name, token_name)        \
  void func_name();                                                  \
  static int token_name = bssl::bench::RegisterBenchmark(func_name); \
  void func_name()

// Lazy registration of benchmark.
//
// This macro expands into registration that resolves after runtime flag
// parsing.
// Example:
// BSSL_BENCH_LAZY_REGISTER() {
//   BENCHMARK(BM_MyBench)->Apply(SetThreads);
//   // Now BM_MyBench is properly configured with a number of threads
//   // as instructed by the `--threads` flag.
// }
#define BSSL_BENCH_LAZY_REGISTER()                           \
  BSSL_BENCH_LAZY_REGISTER_INNER(BSSL_UNIQUE_NAME(Register), \
                                 BSSL_UNIQUE_NAME(register_token))

// Override thread count in parallel tests using the `--threads` flag.
// *Note* that benchmarks must be instantiated after the main function.
// Otherwise, it will default to the number of available CPUs.
//
// This function can only be used in the context of `BSSL_BENCH_LAZY_REGISTER`.
// Otherwise, the benchmark will be aborted.
void SetThreads(benchmark::Benchmark *bench);

// For benchmark registration, get the input size from the runtime flag.
// This is an interim solution, until the next `google/benchmark` release,
// which contains https://github.com/google/benchmark/pull/2073.
//
// This function can only be used in the context of `BSSL_BENCH_LAZY_REGISTER`.
// Otherwise, the benchmark will be aborted.
Span<const int64_t> GetInputSizes(benchmark::Benchmark *bench);

}  // namespace bench

BSSL_NAMESPACE_END

#endif  // OPENSSL_HEADER_BENCH_BENCH_H
