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
#include <openssl/bn.h>
#include <openssl/curve25519.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/mem.h>

#include "../crypto/internal.h"
#include "./internal.h"


BSSL_NAMESPACE_BEGIN
namespace {

void BM_SpeedEd25519(benchmark::State &state) {
  uint8_t public_key[32], private_key[64];
  for (auto _ : state) {
    ED25519_keypair(public_key, private_key);
    benchmark::DoNotOptimize(public_key);
    benchmark::DoNotOptimize(private_key);
  }
}

const uint8_t kMessage[] = {0, 1, 2, 3, 4, 5};

void BM_SpeedEd25519Sign(benchmark::State &state) {
  uint8_t public_key[32];
  uint8_t private_key[64];
  ED25519_keypair(public_key, private_key);
  for (auto _ : state) {
    uint8_t out[64];
    if (!ED25519_sign(out, kMessage, sizeof(kMessage), private_key)) {
      state.SkipWithError("ED25519_sign failed.");
      return;
    }
    benchmark::DoNotOptimize(out);
  }
}

void BM_SpeedEd25519Verify(benchmark::State &state) {
  uint8_t public_key[32];
  uint8_t private_key[64];
  uint8_t signature[64];
  ED25519_keypair(public_key, private_key);
  ED25519_sign(signature, kMessage, sizeof(kMessage), private_key);
  for (auto _ : state) {
    if (!ED25519_verify(kMessage, sizeof(kMessage), signature, public_key)) {
      state.SkipWithError("ED25519_verify failed.");
      return;
    }
  }
}

void BM_SpeedCurve25519BasePointMultiply(benchmark::State &state) {
  uint8_t out[32], in[32];
  for (auto _ : state) {
    OPENSSL_memset(in, 0, sizeof(in));

    X25519_public_from_private(out, in);
    benchmark::DoNotOptimize(out);
  }
}

void BM_SpeedCurve25519ArbitraryPointMultiply(benchmark::State &state) {
  uint8_t out[32], in1[32], in2[32];
  OPENSSL_memset(in1, 0, sizeof(in1));
  OPENSSL_memset(in2, 0, sizeof(in2));
  in1[0] = 1;
  in2[0] = 9;
  for (auto _ : state) {
    if (!X25519(out, in1, in2)) {
      state.SkipWithError("Curve25519 arbitrary point multiplication failed.");
      return;
    }
  }
}

BSSL_BENCH_LAZY_REGISTER() {
  BENCHMARK(BM_SpeedEd25519)->Apply(bench::SetThreads);
  BENCHMARK(BM_SpeedEd25519Sign)->Apply(bench::SetThreads);
  BENCHMARK(BM_SpeedEd25519Verify)->Apply(bench::SetThreads);
  BENCHMARK(BM_SpeedCurve25519BasePointMultiply)->Apply(bench::SetThreads);
  BENCHMARK(BM_SpeedCurve25519ArbitraryPointMultiply)->Apply(bench::SetThreads);
}

}  // namespace
BSSL_NAMESPACE_END
