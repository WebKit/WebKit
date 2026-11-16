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
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include "../crypto/ec/internal.h"
#include "../crypto/fipsmodule/ec/internal.h"

#include "./internal.h"


using namespace bssl;

namespace {

template <typename Out>
using HashToCurve = int (*)(const EC_GROUP *, Out *, const uint8_t *, size_t,
                            const uint8_t *, size_t);
typedef const EC_GROUP *(*Curve)();

const uint8_t kLabel[] = "label";

template <typename Out, HashToCurve<Out> hash_to_curve, Curve curve>
void BM_SpeedHashToCurve(benchmark::State &state) {
  Out out;
  uint8_t input[64];
  RAND_bytes(input, sizeof(input));
  for (auto _ : state) {
    if (!hash_to_curve(curve(), &out, kLabel, sizeof(kLabel), input,
                       sizeof(input))) {
      state.SkipWithError("hash-to-curve failed.");
      return;
    }
  }
}

BSSL_BENCH_LAZY_REGISTER() {
  BENCHMARK(
      BM_SpeedHashToCurve<EC_JACOBIAN, ec_hash_to_curve_p256_xmd_sha256_sswu,
                          EC_group_p256>)
      ->Name("BM_SpeedHashToCurve/hash-to-curve P256_XMD:SHA-256_SSWU_RO_")
      ->Apply(bench::SetThreads);
  BENCHMARK(
      BM_SpeedHashToCurve<EC_JACOBIAN, ec_hash_to_curve_p384_xmd_sha384_sswu,
                          EC_group_p384>)
      ->Name("BM_SpeedHashToCurve/hash-to-curve P384_XMD:SHA-384_SSWU_RO_")
      ->Apply(bench::SetThreads);
  BENCHMARK(
      BM_SpeedHashToCurve<EC_SCALAR, ec_hash_to_scalar_p384_xmd_sha512_draft07,
                          EC_group_p384>)
      ->Name("BM_SpeedHashToCurve/hash-to-scalar P384_XMD:SHA-512")
      ->Apply(bench::SetThreads);
}

}  // namespace
