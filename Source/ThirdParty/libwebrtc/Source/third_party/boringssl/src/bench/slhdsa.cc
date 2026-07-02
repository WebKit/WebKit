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
#include <openssl/slhdsa.h>

#include "./internal.h"

namespace {
void BM_SpeedSLHDSA(benchmark::State &state) {
  for (auto _ : state) {
    uint8_t public_key[SLHDSA_SHA2_128S_PUBLIC_KEY_BYTES],
        private_key[SLHDSA_SHA2_128S_PRIVATE_KEY_BYTES];
    SLHDSA_SHA2_128S_generate_key(public_key, private_key);
    benchmark::DoNotOptimize(private_key);
  }
}

const uint8_t kMessage[] = {0, 1, 2, 3, 4, 5};
void BM_SpeedSLHDSASign(benchmark::State &state) {
  uint8_t public_key[SLHDSA_SHA2_128S_PUBLIC_KEY_BYTES],
      private_key[SLHDSA_SHA2_128S_PRIVATE_KEY_BYTES];
  SLHDSA_SHA2_128S_generate_key(public_key, private_key);

  for (auto _ : state) {
    uint8_t signature[SLHDSA_SHA2_128S_SIGNATURE_BYTES];
    SLHDSA_SHA2_128S_sign(signature, private_key, kMessage, sizeof(kMessage),
                          nullptr, 0);
    benchmark::DoNotOptimize(signature);
  }
}

void BM_SpeedSLHDSAVerify(benchmark::State &state) {
  uint8_t public_key[SLHDSA_SHA2_128S_PUBLIC_KEY_BYTES],
      private_key[SLHDSA_SHA2_128S_PRIVATE_KEY_BYTES];
  SLHDSA_SHA2_128S_generate_key(public_key, private_key);

  uint8_t signature[SLHDSA_SHA2_128S_SIGNATURE_BYTES];
  SLHDSA_SHA2_128S_sign(signature, private_key, kMessage, sizeof(kMessage),
                        nullptr, 0);

  for (auto _ : state) {
    if (!SLHDSA_SHA2_128S_verify(signature, sizeof(signature), public_key,
                                 kMessage, sizeof(kMessage), nullptr, 0)) {
      state.SkipWithError("SLHDSA-SHA2-128s verify failed.");
      return;
    }
  }
}

BSSL_BENCH_LAZY_REGISTER() {
  BENCHMARK(BM_SpeedSLHDSA)->Apply(bssl::bench::SetThreads);
  BENCHMARK(BM_SpeedSLHDSASign)->Apply(bssl::bench::SetThreads);
  BENCHMARK(BM_SpeedSLHDSAVerify)->Apply(bssl::bench::SetThreads);
}

}  // namespace
