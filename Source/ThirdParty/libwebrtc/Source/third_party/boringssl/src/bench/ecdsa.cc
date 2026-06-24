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
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/mem.h>

#include <benchmark/benchmark.h>

#include "../crypto/internal.h"
#include "./internal.h"


BSSL_NAMESPACE_BEGIN
namespace {

void BM_SpeedECDSASign(benchmark::State &state, const EC_GROUP *group) {
  UniquePtr<EC_KEY> key(EC_KEY_new());
  if (!key || !EC_KEY_set_group(key.get(), group) ||
      !EC_KEY_generate_key(key.get())) {
    state.SkipWithError("keygen failed");
    return;
  }

  static constexpr size_t kMaxSignature = 256;
  if (ECDSA_size(key.get()) > kMaxSignature) {
    state.SkipWithError("key is too large.");
    return;
  }
  uint8_t digest[20];
  OPENSSL_memset(digest, 42, sizeof(digest));

  uint8_t out[kMaxSignature];
  for (auto _ : state) {
    unsigned out_len;
    if (!ECDSA_sign(0, digest, sizeof(digest), out, &out_len, key.get())) {
      state.SkipWithError("signing failed.");
      return;
    }
  }
}


void BM_SpeedECDSAVerify(benchmark::State &state, const EC_GROUP *group) {
  UniquePtr<EC_KEY> key(EC_KEY_new());
  if (!key || !EC_KEY_set_group(key.get(), group) ||
      !EC_KEY_generate_key(key.get())) {
    state.SkipWithError("keygen failed");
    return;
  }

  static constexpr size_t kMaxSignature = 256;
  if (ECDSA_size(key.get()) > kMaxSignature) {
    state.SkipWithError("key is too large.");
    return;
  }
  uint8_t digest[20];
  OPENSSL_memset(digest, 42, sizeof(digest));

  uint8_t signature[kMaxSignature];
  unsigned sig_len;
  if (!ECDSA_sign(0, digest, sizeof(digest), signature, &sig_len, key.get())) {
    state.SkipWithError("signing failed");
    return;
  }

  for (auto _ : state) {
    if (!ECDSA_verify(0, digest, sizeof(digest), signature, sig_len,
                      key.get())) {
      state.SkipWithError("verification failed.");
      return;
    }
  }
}

BSSL_BENCH_LAZY_REGISTER() {
  BENCHMARK_CAPTURE(BM_SpeedECDSASign, p224, EC_group_p224())
      ->Apply(bench::SetThreads);
  BENCHMARK_CAPTURE(BM_SpeedECDSASign, p256, EC_group_p256())
      ->Apply(bench::SetThreads);
  BENCHMARK_CAPTURE(BM_SpeedECDSASign, p384, EC_group_p384())
      ->Apply(bench::SetThreads);
  BENCHMARK_CAPTURE(BM_SpeedECDSASign, p521, EC_group_p521())
      ->Apply(bench::SetThreads);

  BENCHMARK_CAPTURE(BM_SpeedECDSAVerify, p224, EC_group_p224())
      ->Apply(bench::SetThreads);
  BENCHMARK_CAPTURE(BM_SpeedECDSAVerify, p256, EC_group_p256())
      ->Apply(bench::SetThreads);
  BENCHMARK_CAPTURE(BM_SpeedECDSAVerify, p384, EC_group_p384())
      ->Apply(bench::SetThreads);
  BENCHMARK_CAPTURE(BM_SpeedECDSAVerify, p521, EC_group_p521())
      ->Apply(bench::SetThreads);
}

}  // namespace
BSSL_NAMESPACE_END
