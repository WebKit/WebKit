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

#include <openssl/base.h>
#include <openssl/digest.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/mem.h>
#include <openssl/rand.h>

#include <benchmark/benchmark.h>

#include "./internal.h"

namespace {

void BM_SpeedRandom(benchmark::State &state) {
  size_t buf_size = static_cast<size_t>(state.range(0));
  std::vector<uint8_t> scratch(buf_size);
  for (auto _ : state) {
    RAND_bytes(scratch.data(), buf_size);
    benchmark::DoNotOptimize(scratch);
  }
  state.SetBytesProcessed(buf_size * state.iterations());
}

static const int64_t kInputSizes[] = {16, 256, 1350, 8192, 16384};

void SetInputLength(benchmark::Benchmark *bench) {
  bench->ArgName("InputSize");
  auto input_sizes = bssl::bench::GetInputSizes(bench);
  if (input_sizes.empty()) {
    bench->ArgsProduct(
        {std::vector<int64_t>(kInputSizes, std::end(kInputSizes))});
  } else {
    bench->ArgsProduct({{input_sizes.begin(), input_sizes.end()}});
  }
}

BSSL_BENCH_LAZY_REGISTER() {
  BENCHMARK(BM_SpeedRandom)
      ->Apply(SetInputLength)
      ->Apply(bssl::bench::SetThreads);
}

}  // namespace
