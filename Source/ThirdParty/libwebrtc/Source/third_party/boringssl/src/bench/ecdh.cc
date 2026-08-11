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
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/mem.h>

#include "./internal.h"

namespace {
void BM_SpeedECDH(benchmark::State &state, const EC_GROUP *group) {
  bssl::UniquePtr<EC_KEY> peer_key(EC_KEY_new());
  if (!peer_key || !EC_KEY_set_group(peer_key.get(), group) ||
      !EC_KEY_generate_key(peer_key.get())) {
    state.SkipWithError("peer keygen failed.");
    return;
  }

  size_t peer_value_len =
      EC_POINT_point2oct(group, EC_KEY_get0_public_key(peer_key.get()),
                         POINT_CONVERSION_UNCOMPRESSED, nullptr, 0, nullptr);
  if (peer_value_len == 0) {
    state.SkipWithError("EC_POINT_point2oct failed.");
    return;
  }
  std::vector<uint8_t> peer_value(peer_value_len);
  peer_value_len =
      EC_POINT_point2oct(group, EC_KEY_get0_public_key(peer_key.get()),
                         POINT_CONVERSION_UNCOMPRESSED, peer_value.data(),
                         peer_value_len, nullptr);
  if (peer_value_len == 0) {
    state.SkipWithError("peer pubkey serialisation failed.");
    return;
  }

  for (auto _ : state) {
    bssl::UniquePtr<EC_KEY> key(EC_KEY_new());
    if (!key || !EC_KEY_set_group(key.get(), group) ||
        !EC_KEY_generate_key(key.get())) {
      state.SkipWithError("self keygen failed.");
      return;
    }
    bssl::UniquePtr<EC_POINT> point(EC_POINT_new(group));
    bssl::UniquePtr<EC_POINT> peer_point(EC_POINT_new(group));
    bssl::UniquePtr<BN_CTX> ctx(BN_CTX_new());
    bssl::UniquePtr<BIGNUM> x(BN_new());
    if (!point || !peer_point || !ctx || !x ||
        !EC_POINT_oct2point(group, peer_point.get(), peer_value.data(),
                            peer_value_len, ctx.get()) ||
        !EC_POINT_mul(group, point.get(), nullptr, peer_point.get(),
                      EC_KEY_get0_private_key(key.get()), ctx.get()) ||
        !EC_POINT_get_affine_coordinates_GFp(group, point.get(), x.get(),
                                             nullptr, ctx.get())) {
      state.SkipWithError("final key agreement failed.");
      return;
    }
  }
}

BSSL_BENCH_LAZY_REGISTER() {
  BENCHMARK_CAPTURE(BM_SpeedECDH, p224, EC_group_p224())
      ->Apply(bssl::bench::SetThreads);
  BENCHMARK_CAPTURE(BM_SpeedECDH, p256, EC_group_p256())
      ->Apply(bssl::bench::SetThreads);
  BENCHMARK_CAPTURE(BM_SpeedECDH, p384, EC_group_p384())
      ->Apply(bssl::bench::SetThreads);
  BENCHMARK_CAPTURE(BM_SpeedECDH, p521, EC_group_p521())
      ->Apply(bssl::bench::SetThreads);
}

}  // namespace
