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

#include <benchmark/benchmark.h>

#include <openssl/base.h>
#include <openssl/err.h>
#include <openssl/evp.h>

#include "./internal.h"

namespace {
const char kPassword[] = "password";
const uint8_t kSalt[] = "NaCl";
void BM_SpeedScrypt(benchmark::State &state, const uint64_t N, const uint64_t r,
                    const uint64_t p) {
  for (auto _ : state) {
    uint8_t out[64];
    if (!EVP_PBE_scrypt(kPassword, sizeof(kPassword) - 1, kSalt,
                        sizeof(kSalt) - 1, N, r, p, 0 /* max_mem */, out,
                        sizeof(out))) {
      state.SkipWithError("scrypt failed.");
      return;
    }
  }
}

BSSL_BENCH_LAZY_REGISTER() {
  BENCHMARK_CAPTURE(BM_SpeedScrypt, (N = 1024, r = 8, p = 16), 1024, 8, 16)
      ->Apply(bssl::bench::SetThreads);
  BENCHMARK_CAPTURE(BM_SpeedScrypt, (N = 16384, r = 8, p = 1), 1024, 8, 16)
      ->Apply(bssl::bench::SetThreads);
}

}  // namespace
