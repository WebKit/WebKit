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

#include <string_view>

#include <benchmark/benchmark.h>

#include <openssl/evp.h>
#include <openssl/mldsa.h>
#include <openssl/span.h>

#include "./internal.h"

BSSL_NAMESPACE_BEGIN
namespace {

constexpr size_t kMaxPublicKeyBytes = MLDSA87_PUBLIC_KEY_BYTES;
constexpr size_t kMaxSignatureBytes = MLDSA87_SIGNATURE_BYTES;

void BM_SpeedMLDSAKeyGen(benchmark::State &state, const EVP_PKEY_ALG *alg) {
  for (auto _ : state) {
    UniquePtr<EVP_PKEY> pkey(EVP_PKEY_generate_from_alg(alg));
    if (pkey == nullptr) {
      state.SkipWithError("Error generating ML-DSA key.");
      return;
    }
  }
}

void BM_SpeedMLDSASign(benchmark::State &state, const EVP_PKEY_ALG *alg) {
  UniquePtr<EVP_PKEY> pkey(EVP_PKEY_generate_from_alg(alg));
  if (pkey == nullptr) {
    state.SkipWithError("Error generating ML-DSA key.");
    return;
  }

  const auto message = bssl::StringAsBytes("Hello world");
  for (auto _ : state) {
    uint8_t sig[kMaxSignatureBytes];
    size_t sig_len = sizeof(sig);
    ScopedEVP_MD_CTX ctx;
    if (!EVP_DigestSignInit(ctx.get(), nullptr, nullptr, nullptr, pkey.get()) ||
        !EVP_DigestSign(ctx.get(), sig, &sig_len, message.data(),
                        message.size())) {
      state.SkipWithError("Error generating ML-DSA signature.");
      return;
    }
  }
}

void BM_SpeedMLDSAParsePubKey(benchmark::State &state,
                              const EVP_PKEY_ALG *alg) {
  UniquePtr<EVP_PKEY> pkey(EVP_PKEY_generate_from_alg(alg));
  if (pkey == nullptr) {
    state.SkipWithError("Error generating ML-DSA key.");
    return;
  }

  uint8_t pub[kMaxPublicKeyBytes];
  size_t pub_len = sizeof(pub);
  if (!EVP_PKEY_get_raw_public_key(pkey.get(), pub, &pub_len)) {
    state.SkipWithError("Error serializing public key.");
    return;
  }

  for (auto _ : state) {
    UniquePtr<EVP_PKEY> parsed(
        EVP_PKEY_from_raw_public_key(alg, pub, pub_len));
    if (parsed == nullptr) {
      state.SkipWithError("Error parsing public key.");
      return;
    }
  }
}

void BM_SpeedMLDSAVerify(benchmark::State &state, const EVP_PKEY_ALG *alg) {
  UniquePtr<EVP_PKEY> priv(EVP_PKEY_generate_from_alg(alg));
  if (priv == nullptr) {
    state.SkipWithError("Error generating ML-DSA key.");
    return;
  }
  const auto message = bssl::StringAsBytes("Hello world");
  uint8_t sig[kMaxSignatureBytes];
  size_t sig_len = sizeof(sig);
  ScopedEVP_MD_CTX sign_ctx;
  if (!EVP_DigestSignInit(sign_ctx.get(), nullptr, nullptr, nullptr,
                          priv.get()) ||
      !EVP_DigestSign(sign_ctx.get(), sig, &sig_len, message.data(),
                      message.size())) {
    state.SkipWithError("Error generating ML-DSA signature.");
  }
  UniquePtr<EVP_PKEY> pub(EVP_PKEY_copy_public(priv.get()));
  if (priv == nullptr) {
    state.SkipWithError("Error copying public key.");
    return;
  }

  for (auto _ : state) {
    ScopedEVP_MD_CTX verify_ctx;
    if (!EVP_DigestVerifyInit(verify_ctx.get(), nullptr, nullptr, nullptr,
                              priv.get()) ||
        !EVP_DigestVerify(verify_ctx.get(), sig, sig_len, message.data(),
                          message.size())) {
      state.SkipWithError("Error verifying ML-DSA signature.");
      return;
    }
  }
}

void BM_SpeedMLDSAVerifyBadSignature(benchmark::State &state,
                                     const EVP_PKEY_ALG *alg) {
  UniquePtr<EVP_PKEY> priv(EVP_PKEY_generate_from_alg(alg));
  if (priv == nullptr) {
    state.SkipWithError("Error generating ML-DSA key.");
    return;
  }
  const auto message = bssl::StringAsBytes("Hello world");
  uint8_t sig[kMaxSignatureBytes];
  size_t sig_len = sizeof(sig);
  ScopedEVP_MD_CTX sign_ctx;
  if (!EVP_DigestSignInit(sign_ctx.get(), nullptr, nullptr, nullptr,
                          priv.get()) ||
      !EVP_DigestSign(sign_ctx.get(), sig, &sig_len, message.data(),
                      message.size())) {
    state.SkipWithError("Error generating ML-DSA signature.");
  }
  sig[42] ^= 0x42;
  UniquePtr<EVP_PKEY> pub(EVP_PKEY_copy_public(priv.get()));
  if (priv == nullptr) {
    state.SkipWithError("Error copying public key.");
    return;
  }

  for (auto _ : state) {
    ScopedEVP_MD_CTX verify_ctx;
    if (!EVP_DigestVerifyInit(verify_ctx.get(), nullptr, nullptr, nullptr,
                              priv.get())) {
      state.SkipWithError("EVP_DigestVerifyInit failed.");
      return;
    }
    if (EVP_DigestVerify(verify_ctx.get(), sig, sig_len, message.data(),
                         message.size())) {
      state.SkipWithError("MLDSA signature unexpectedly verified.");
      return;
    }
  }
}

BSSL_BENCH_LAZY_REGISTER() {
#define MAKE_MLDSA_BENCHMARKS(kl)                                             \
  BENCHMARK_CAPTURE(BM_SpeedMLDSAKeyGen, ml_dsa_##kl, EVP_pkey_ml_dsa_##kl()) \
      ->Apply(bssl::bench::SetThreads);                                       \
  BENCHMARK_CAPTURE(BM_SpeedMLDSASign, ml_dsa_##kl, EVP_pkey_ml_dsa_##kl())   \
      ->Apply(bssl::bench::SetThreads);                                       \
  BENCHMARK_CAPTURE(BM_SpeedMLDSAParsePubKey, ml_dsa_##kl,                    \
                    EVP_pkey_ml_dsa_##kl())                                   \
      ->Apply(bssl::bench::SetThreads);                                       \
  BENCHMARK_CAPTURE(BM_SpeedMLDSAVerify, ml_dsa_##kl, EVP_pkey_ml_dsa_##kl()) \
      ->Apply(bssl::bench::SetThreads);                                       \
  BENCHMARK_CAPTURE(BM_SpeedMLDSAVerifyBadSignature, ml_dsa_##kl,             \
                    EVP_pkey_ml_dsa_##kl())                                   \
      ->Apply(bssl::bench::SetThreads)

  MAKE_MLDSA_BENCHMARKS(44);
  MAKE_MLDSA_BENCHMARKS(65);
  MAKE_MLDSA_BENCHMARKS(87);
}

}  // namespace
BSSL_NAMESPACE_END
