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

#include <vector>

#include <benchmark/benchmark.h>

#include <openssl/base.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/siphash.h>

#include "./internal.h"

namespace {

void BM_SpeedSipHash(benchmark::State &state) {
  uint64_t key[2] = {0};
  size_t input_size = state.range(0);
  std::vector<uint8_t> input(input_size);

  for (auto _ : state) {
    SIPHASH_24(key, input.data(), input.size());
    benchmark::DoNotOptimize(input);
  }
  state.SetBytesProcessed(state.iterations() * input_size);
}

BSSL_BENCH_LAZY_REGISTER() {
  BENCHMARK(BM_SpeedSipHash)
      ->ArgsProduct({{16, 256, 1350, 8192, 16384}})
      ->Apply(bssl::bench::SetThreads);
}

}  // namespace
