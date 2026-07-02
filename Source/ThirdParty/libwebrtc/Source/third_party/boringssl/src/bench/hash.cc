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

#include <benchmark/benchmark.h>

#include "./internal.h"

namespace {

void BM_SpeedHash(benchmark::State &state, const EVP_MD *md) {
  size_t input_size = static_cast<size_t>(state.range(0));
  std::vector<uint8_t> input(input_size);
  uint8_t digest[EVP_MAX_MD_SIZE];
  for (auto _ : state) {
    unsigned int md_len;

    bssl::ScopedEVP_MD_CTX ctx;
    if (!EVP_DigestInit_ex(ctx.get(), md, /* ENGINE= */ NULL) ||
        !EVP_DigestUpdate(ctx.get(), input.data(), input_size) ||
        !EVP_DigestFinal_ex(ctx.get(), digest, &md_len)) {
      state.SkipWithError("EVP_DigestInit_ex failed.");
      return;
    }
  }
  state.SetBytesProcessed(state.iterations() * input_size);
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
  BENCHMARK_CAPTURE(BM_SpeedHash, sha1, EVP_sha1())
      ->Apply(SetInputLength)
      ->Apply(bssl::bench::SetThreads);
  BENCHMARK_CAPTURE(BM_SpeedHash, sha256, EVP_sha256())
      ->Apply(SetInputLength)
      ->Apply(bssl::bench::SetThreads);
  BENCHMARK_CAPTURE(BM_SpeedHash, sha512, EVP_sha512())
      ->Apply(SetInputLength)
      ->Apply(bssl::bench::SetThreads);
  BENCHMARK_CAPTURE(BM_SpeedHash, blake2b256, EVP_blake2b256())
      ->Apply(SetInputLength)
      ->Apply(bssl::bench::SetThreads);
}

}  // namespace
