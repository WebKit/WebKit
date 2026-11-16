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
#include <openssl/hrss.h>
#include <openssl/rand.h>

#include "./internal.h"

namespace {
void BM_SpeedHRSSGenerate(benchmark::State &state) {
  uint8_t entropy[HRSS_GENERATE_KEY_BYTES];
  for (auto _ : state) {
    struct HRSS_public_key pub;
    struct HRSS_private_key priv;
    RAND_bytes(entropy, sizeof(entropy));
    if (!HRSS_generate_key(&pub, &priv, entropy)) {
      state.SkipWithError("Failed to time HRSS_generate_key.");
      return;
    }
  }
}

void BM_SpeedHRSSEncap(benchmark::State &state) {
  struct HRSS_public_key pub;
  struct HRSS_private_key priv;
  uint8_t key_entropy[HRSS_GENERATE_KEY_BYTES];
  RAND_bytes(key_entropy, sizeof(key_entropy));
  HRSS_generate_key(&pub, &priv, key_entropy);

  uint8_t entropy[HRSS_ENCAP_BYTES];
  uint8_t shared_key[HRSS_KEY_BYTES];
  uint8_t ciphertext[HRSS_CIPHERTEXT_BYTES];
  for (auto _ : state) {
    RAND_bytes(entropy, sizeof(entropy));
    if (!HRSS_encap(ciphertext, shared_key, &pub, entropy)) {
      state.SkipWithError("Failed to HRSS_encap.");
      return;
    }
  }
}

void BM_SpeedHRSSDecap(benchmark::State &state) {
  struct HRSS_public_key pub;
  struct HRSS_private_key priv;
  uint8_t key_entropy[HRSS_GENERATE_KEY_BYTES];
  uint8_t entropy[HRSS_ENCAP_BYTES];
  uint8_t shared_key[HRSS_KEY_BYTES];
  uint8_t ciphertext[HRSS_CIPHERTEXT_BYTES];
  RAND_bytes(key_entropy, sizeof(key_entropy));
  RAND_bytes(entropy, sizeof(entropy));
  HRSS_generate_key(&pub, &priv, key_entropy);
  HRSS_encap(ciphertext, shared_key, &pub, entropy);

  uint8_t shared_key2[HRSS_KEY_BYTES];
  for (auto _ : state) {
    if (!HRSS_decap(shared_key2, &priv, ciphertext, sizeof(ciphertext))) {
      state.SkipWithError("Failed to HRSS_decap.");
      return;
    }
  }
}

BSSL_BENCH_LAZY_REGISTER() {
  BENCHMARK(BM_SpeedHRSSGenerate)->Apply(bssl::bench::SetThreads);
  BENCHMARK(BM_SpeedHRSSEncap)->Apply(bssl::bench::SetThreads);
  BENCHMARK(BM_SpeedHRSSDecap)->Apply(bssl::bench::SetThreads);
}

}  // namespace
