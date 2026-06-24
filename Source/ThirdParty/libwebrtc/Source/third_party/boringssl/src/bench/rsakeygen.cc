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

#include <openssl/base.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/mem.h>
#include <openssl/rsa.h>

#include <benchmark/benchmark.h>

// This benchmark is standalone because it is time consuming.
namespace {
void BM_SpeedRSAKeyGen(benchmark::State &state, int size) {
  bssl::UniquePtr<BIGNUM> e(BN_new());
  if (!BN_set_word(e.get(), 65537)) {
    state.SkipWithError("BN_set_word failed.");
    return;
  }

  for (auto _ : state) {
    bssl::UniquePtr<RSA> rsa(RSA_new());
    if (!RSA_generate_key_ex(rsa.get(), size, e.get(), nullptr)) {
      state.SkipWithError("RSA_generate_key_ex failed.");
      return;
    }
  }
}

BENCHMARK_CAPTURE(BM_SpeedRSAKeyGen, 2048, 2048);
BENCHMARK_CAPTURE(BM_SpeedRSAKeyGen, 3072, 3072);
BENCHMARK_CAPTURE(BM_SpeedRSAKeyGen, 4096, 4096);
}  // namespace
