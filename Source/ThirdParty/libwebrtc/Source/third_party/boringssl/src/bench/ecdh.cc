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
#include <openssl/bytestring.h>
#include <openssl/ec.h>
#include <openssl/ec_key.h>
#include <openssl/ecdh.h>

#include "./internal.h"

BSSL_NAMESPACE_BEGIN
namespace {

// P-521 has degree 521, requiring ceil(521 / 8) = 66 bytes.
static constexpr size_t kMaxCoordinateSize = 66;
static constexpr size_t kMaxPointBytes = 1 + 2 * kMaxCoordinateSize;

void BM_SpeedECDHKeyGen(benchmark::State &state, const EC_GROUP *group) {
  for (auto _ : state) {
    UniquePtr<EC_KEY> key(EC_KEY_new());
    if (!key || !EC_KEY_set_group(key.get(), group) ||
        !EC_KEY_generate_key(key.get())) {
      state.SkipWithError("self keygen failed.");
      return;
    }

    benchmark::DoNotOptimize(key.get());
  }
}

void BM_SpeedECDHComputeKey(benchmark::State &state, const EC_GROUP *group) {
  UniquePtr<EC_KEY> peer_key(EC_KEY_new());
  if (!peer_key || !EC_KEY_set_group(peer_key.get(), group) ||
      !EC_KEY_generate_key(peer_key.get())) {
    state.SkipWithError("peer keygen failed.");
    return;
  }

  const EC_POINT *peer_pub = EC_KEY_get0_public_key(peer_key.get());

  UniquePtr<EC_KEY> key(EC_KEY_new());
  if (!key || !EC_KEY_set_group(key.get(), group) ||
      !EC_KEY_generate_key(key.get())) {
    state.SkipWithError("self keygen failed.");
    return;
  }

  uint8_t secret[kMaxCoordinateSize];
  size_t secret_len = (EC_GROUP_get_degree(group) + 7) / 8;
  if (secret_len > sizeof(secret)) {
    state.SkipWithError("secret length exceeds buffer size.");
    return;
  }

  for (auto _ : state) {
    benchmark::ClobberMemory();
    benchmark::DoNotOptimize(peer_pub);

    int res =
        ECDH_compute_key(secret, secret_len, peer_pub, key.get(), nullptr);
    if (res < 0) {
      state.SkipWithError("ECDH_compute_key failed.");
      return;
    }

    benchmark::DoNotOptimize(secret);
    benchmark::DoNotOptimize(res);
  }
}

// Given a public key from the peer, benchmark the combination of keygen,
// parsing the peer key, and ECDH. This is the sequence of operations done by
// the receiving side of an ephemeral ECDH exchange, as in TLS. We benchmark
// them together for an apples-to-apples comparison across other ephemeral key
// exchanges. For example, this is analogous to parse and encap in ML-KEM.
//
// This benchmark could equivalently be calling into EC_KEY_generate and
// ECDH_compute_key. We have decided to not call them to exactly mirror what
// libssl does.
void BM_SpeedECDHEphemeral(benchmark::State &state, const EC_GROUP *group) {
  size_t secret_len = (EC_GROUP_get_degree(group) + 7) / 8;
  if (secret_len > kMaxCoordinateSize) {
    state.SkipWithError("secret length exceeds buffer size.");
    return;
  }

  UniquePtr<BIGNUM> peer_priv(BN_new());
  UniquePtr<EC_POINT> peer_pub(EC_POINT_new(group));
  if (!peer_priv || !peer_pub ||
      !BN_rand_range_ex(peer_priv.get(), 1, EC_GROUP_get0_order(group)) ||
      !EC_POINT_mul(group, peer_pub.get(), peer_priv.get(), nullptr, nullptr,
                    nullptr)) {
    state.SkipWithError("peer keygen failed.");
    return;
  }

  uint8_t peer_pub_bytes[kMaxPointBytes];
  size_t peer_pub_bytes_len =
      EC_POINT_point2oct(group, peer_pub.get(), POINT_CONVERSION_UNCOMPRESSED,
                         peer_pub_bytes, sizeof(peer_pub_bytes), nullptr);
  if (peer_pub_bytes_len == 0) {
    state.SkipWithError("peer key serialization failed.");
    return;
  }

  for (auto _ : state) {
    benchmark::ClobberMemory();

    // Generate an ephemeral keypair.
    // This part is basically `EC_KEY_generate_key`.
    UniquePtr<BIGNUM> priv(BN_new());
    UniquePtr<EC_POINT> pub(EC_POINT_new(group));
    if (!priv || !pub ||
        !BN_rand_range_ex(priv.get(), 1, EC_GROUP_get0_order(group)) ||
        !EC_POINT_mul(group, pub.get(), priv.get(), nullptr, nullptr,
                      nullptr)) {
      state.SkipWithError("self keygen failed.");
      return;
    }

    // Serialize the public key to bytes.
    ScopedCBB cbb;
    if (!CBB_init(cbb.get(), kMaxPointBytes) ||
        !EC_POINT_point2cbb(cbb.get(), group, pub.get(),
                            POINT_CONVERSION_UNCOMPRESSED, nullptr)) {
      state.SkipWithError("self key serialization failed.");
      return;
    }
    benchmark::DoNotOptimize(CBB_data(cbb.get()));

    // Parse the peer's public key point.
    UniquePtr<EC_POINT> peer_point(EC_POINT_new(group));
    UniquePtr<EC_POINT> result(EC_POINT_new(group));
    UniquePtr<BIGNUM> x(BN_new());
    if (!peer_point || !result || !x) {
      state.SkipWithError("allocation failed.");
      return;
    }

    if (peer_pub_bytes_len == 0 ||
        peer_pub_bytes[0] != POINT_CONVERSION_UNCOMPRESSED ||
        !EC_POINT_oct2point(group, peer_point.get(), peer_pub_bytes,
                            peer_pub_bytes_len, nullptr)) {
      state.SkipWithError("peer key parsing failed.");
      return;
    }

    // Compute the shared secret.
    // This part is basically `ECDH_compute_key`.
    if (!EC_POINT_mul(group, result.get(), nullptr, peer_point.get(),
                      priv.get(), nullptr) ||
        !EC_POINT_get_affine_coordinates_GFp(group, result.get(), x.get(),
                                             nullptr, nullptr)) {
      state.SkipWithError("shared secret derivation failed.");
      return;
    }

    uint8_t secret[kMaxCoordinateSize];
    if (!BN_bn2bin_padded(secret, secret_len, x.get())) {
      state.SkipWithError("secret padding failed.");
      return;
    }

    benchmark::DoNotOptimize(secret);
  }
}

BSSL_BENCH_LAZY_REGISTER() {
  BENCHMARK_CAPTURE(BM_SpeedECDHKeyGen, p224, EC_group_p224())
      ->Apply(bench::SetThreads);
  BENCHMARK_CAPTURE(BM_SpeedECDHKeyGen, p256, EC_group_p256())
      ->Apply(bench::SetThreads);
  BENCHMARK_CAPTURE(BM_SpeedECDHKeyGen, p384, EC_group_p384())
      ->Apply(bench::SetThreads);
  BENCHMARK_CAPTURE(BM_SpeedECDHKeyGen, p521, EC_group_p521())
      ->Apply(bench::SetThreads);

  BENCHMARK_CAPTURE(BM_SpeedECDHComputeKey, p224, EC_group_p224())
      ->Apply(bench::SetThreads);
  BENCHMARK_CAPTURE(BM_SpeedECDHComputeKey, p256, EC_group_p256())
      ->Apply(bench::SetThreads);
  BENCHMARK_CAPTURE(BM_SpeedECDHComputeKey, p384, EC_group_p384())
      ->Apply(bench::SetThreads);
  BENCHMARK_CAPTURE(BM_SpeedECDHComputeKey, p521, EC_group_p521())
      ->Apply(bench::SetThreads);

  BENCHMARK_CAPTURE(BM_SpeedECDHEphemeral, p224, EC_group_p224())
      ->Apply(bench::SetThreads);
  BENCHMARK_CAPTURE(BM_SpeedECDHEphemeral, p256, EC_group_p256())
      ->Apply(bench::SetThreads);
  BENCHMARK_CAPTURE(BM_SpeedECDHEphemeral, p384, EC_group_p384())
      ->Apply(bench::SetThreads);
  BENCHMARK_CAPTURE(BM_SpeedECDHEphemeral, p521, EC_group_p521())
      ->Apply(bench::SetThreads);
}

}  // namespace
BSSL_NAMESPACE_END
