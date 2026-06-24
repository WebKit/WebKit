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

#include <string.h>

#include <benchmark/benchmark.h>

#include <openssl/base.h>
#include <openssl/bytestring.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/mlkem.h>

#include "./internal.h"

namespace {

// generate_key + decap (same as TLS server side)
void BM_SpeedMLKEM768KeyGenDecap(benchmark::State &state) {
  uint8_t ciphertext[MLKEM768_CIPHERTEXT_BYTES];
  // This ciphertext is nonsense, but decap is constant-time so, for the
  // purposes of timing, it's fine.
  memset(ciphertext, 42, sizeof(ciphertext));

  for (auto _ : state) {
    MLKEM768_private_key priv;
    uint8_t encoded_public_key[MLKEM768_PUBLIC_KEY_BYTES];
    MLKEM768_generate_key(encoded_public_key, nullptr, &priv);
    uint8_t shared_secret[MLKEM_SHARED_SECRET_BYTES];
    if (!MLKEM768_decap(shared_secret, ciphertext, sizeof(ciphertext), &priv)) {
      state.SkipWithError("MLKEM768_decap failed");
      return;
    }
    benchmark::DoNotOptimize(shared_secret);
  }
}

// parse + encap (same as TLS client side)
void BM_SpeedMLKEM768ParseEncap(benchmark::State &state) {
  for (auto _ : state) {
    state.PauseTiming();
    uint8_t encoded_public_key[MLKEM768_PUBLIC_KEY_BYTES];
    {
      MLKEM768_private_key priv;
      MLKEM768_generate_key(encoded_public_key, nullptr, &priv);
    }
    benchmark::DoNotOptimize(encoded_public_key);
    state.ResumeTiming();

    MLKEM768_public_key pub;
    CBS encoded_public_key_cbs;
    CBS_init(&encoded_public_key_cbs, encoded_public_key,
             sizeof(encoded_public_key));
    if (!MLKEM768_parse_public_key(&pub, &encoded_public_key_cbs)) {
      state.SkipWithError("Failure in MLKEM768_parse_public_key.");
      return;
    }
    uint8_t shared_secret[MLKEM_SHARED_SECRET_BYTES];
    uint8_t ciphertext[MLKEM768_CIPHERTEXT_BYTES];
    MLKEM768_encap(ciphertext, shared_secret, &pub);
    benchmark::DoNotOptimize(ciphertext);
    benchmark::DoNotOptimize(shared_secret);
  }
}

// generate_key + decap (same as TLS server side)
void BM_SpeedMLKEM1024KeyGenDecap(benchmark::State &state) {
  uint8_t ciphertext[MLKEM1024_CIPHERTEXT_BYTES];
  // This ciphertext is nonsense, but decap is constant-time so, for the
  // purposes of timing, it's fine.
  memset(ciphertext, 42, sizeof(ciphertext));
  auto priv = std::make_unique<MLKEM1024_private_key>();

  for (auto _ : state) {
    uint8_t encoded_public_key[MLKEM1024_PUBLIC_KEY_BYTES];
    MLKEM1024_generate_key(encoded_public_key, nullptr, &*priv);
    uint8_t shared_secret[MLKEM_SHARED_SECRET_BYTES];
    if (!MLKEM1024_decap(shared_secret, ciphertext, sizeof(ciphertext),
                         &*priv)) {
      state.SkipWithError("MLKEM1024_decap failed");
      return;
    }
    benchmark::DoNotOptimize(shared_secret);
  }
}

// parse + encap (same as TLS client side)
void BM_SpeedMLKEM1024ParseEncap(benchmark::State &state) {
  for (auto _ : state) {
    state.PauseTiming();
    uint8_t encoded_public_key[MLKEM1024_PUBLIC_KEY_BYTES];
    {
      // On heap to avoid stack frame size limit.
      auto priv = std::make_unique<MLKEM1024_private_key>();
      MLKEM1024_generate_key(encoded_public_key, nullptr, &*priv);
    }
    benchmark::DoNotOptimize(encoded_public_key);
    state.ResumeTiming();

    MLKEM1024_public_key pub;
    CBS encoded_public_key_cbs;
    CBS_init(&encoded_public_key_cbs, encoded_public_key,
             sizeof(encoded_public_key));
    if (!MLKEM1024_parse_public_key(&pub, &encoded_public_key_cbs)) {
      state.SkipWithError("Failure in MLKEM1024_parse_public_key.");
      return;
    }
    uint8_t shared_secret[MLKEM_SHARED_SECRET_BYTES];
    uint8_t ciphertext[MLKEM1024_CIPHERTEXT_BYTES];
    MLKEM1024_encap(ciphertext, shared_secret, &pub);
    benchmark::DoNotOptimize(ciphertext);
    benchmark::DoNotOptimize(shared_secret);
  }
}

// Microbenchmarks follow

void BM_SpeedMLKEM768KeyGenOnly(benchmark::State &state) {
  for (auto _ : state) {
    MLKEM768_private_key priv;
    uint8_t encoded_public_key[MLKEM768_PUBLIC_KEY_BYTES];
    MLKEM768_generate_key(encoded_public_key, nullptr, &priv);
    benchmark::DoNotOptimize(encoded_public_key);
    benchmark::DoNotOptimize(priv);
  }
}

void BM_SpeedMLKEM768DecapOnly(benchmark::State &state) {
  uint8_t ciphertext[MLKEM768_CIPHERTEXT_BYTES];
  // This ciphertext is nonsense, but decap is constant-time so, for the
  // purposes of timing, it's fine.
  memset(ciphertext, 42, sizeof(ciphertext));

  for (auto _ : state) {
    state.PauseTiming();
    MLKEM768_private_key priv;
    {
      uint8_t encoded_public_key[MLKEM768_PUBLIC_KEY_BYTES];
      MLKEM768_generate_key(encoded_public_key, nullptr, &priv);
    }
    benchmark::DoNotOptimize(priv);
    state.ResumeTiming();

    uint8_t shared_secret[MLKEM_SHARED_SECRET_BYTES];
    if (!MLKEM768_decap(shared_secret, ciphertext, sizeof(ciphertext), &priv)) {
      state.SkipWithError("MLKEM768_decap failed");
      return;
    }
    benchmark::DoNotOptimize(shared_secret);
  }
}

void BM_SpeedMLKEM768ParseOnly(benchmark::State &state) {
  for (auto _ : state) {
    state.PauseTiming();
    uint8_t encoded_public_key[MLKEM768_PUBLIC_KEY_BYTES];
    {
      MLKEM768_private_key priv;
      MLKEM768_generate_key(encoded_public_key, nullptr, &priv);
    }
    benchmark::DoNotOptimize(encoded_public_key);
    state.ResumeTiming();

    MLKEM768_public_key pub;
    CBS encoded_public_key_cbs;
    CBS_init(&encoded_public_key_cbs, encoded_public_key,
             sizeof(encoded_public_key));
    if (!MLKEM768_parse_public_key(&pub, &encoded_public_key_cbs)) {
      state.SkipWithError("Failure in MLKEM768_parse_public_key.");
      return;
    }
    benchmark::DoNotOptimize(pub);
  }
}

void BM_SpeedMLKEM768EncapOnly(benchmark::State &state) {
  for (auto _ : state) {
    state.PauseTiming();
    MLKEM768_public_key pub;
    {
      MLKEM768_private_key priv;
      uint8_t encoded_public_key[MLKEM768_PUBLIC_KEY_BYTES];
      MLKEM768_generate_key(encoded_public_key, nullptr, &priv);
      MLKEM768_public_from_private(&pub, &priv);
    }
    benchmark::DoNotOptimize(pub);
    state.ResumeTiming();

    uint8_t shared_secret[MLKEM_SHARED_SECRET_BYTES];
    uint8_t ciphertext[MLKEM768_CIPHERTEXT_BYTES];
    MLKEM768_encap(ciphertext, shared_secret, &pub);
    benchmark::DoNotOptimize(ciphertext);
    benchmark::DoNotOptimize(shared_secret);
  }
}

void BM_SpeedMLKEM1024KeyGenOnly(benchmark::State &state) {
  for (auto _ : state) {
    MLKEM1024_private_key priv;
    uint8_t encoded_public_key[MLKEM1024_PUBLIC_KEY_BYTES];
    MLKEM1024_generate_key(encoded_public_key, nullptr, &priv);
    benchmark::DoNotOptimize(encoded_public_key);
    benchmark::DoNotOptimize(priv);
  }
}

void BM_SpeedMLKEM1024DecapOnly(benchmark::State &state) {
  uint8_t ciphertext[MLKEM1024_CIPHERTEXT_BYTES];
  // This ciphertext is nonsense, but decap is constant-time so, for the
  // purposes of timing, it's fine.
  memset(ciphertext, 42, sizeof(ciphertext));

  for (auto _ : state) {
    state.PauseTiming();
    MLKEM1024_private_key priv;
    {
      uint8_t encoded_public_key[MLKEM1024_PUBLIC_KEY_BYTES];
      MLKEM1024_generate_key(encoded_public_key, nullptr, &priv);
    }
    benchmark::DoNotOptimize(priv);
    state.ResumeTiming();

    uint8_t shared_secret[MLKEM_SHARED_SECRET_BYTES];
    if (!MLKEM1024_decap(shared_secret, ciphertext, sizeof(ciphertext),
                         &priv)) {
      state.SkipWithError("MLKEM1024_decap failed");
      return;
    }
    benchmark::DoNotOptimize(shared_secret);
  }
}

void BM_SpeedMLKEM1024ParseOnly(benchmark::State &state) {
  for (auto _ : state) {
    state.PauseTiming();
    uint8_t encoded_public_key[MLKEM1024_PUBLIC_KEY_BYTES];
    {
      // On heap to avoid stack frame size limit.
      auto priv = std::make_unique<MLKEM1024_private_key>();
      MLKEM1024_generate_key(encoded_public_key, nullptr, &*priv);
    }
    benchmark::DoNotOptimize(encoded_public_key);
    state.ResumeTiming();

    MLKEM1024_public_key pub;
    CBS encoded_public_key_cbs;
    CBS_init(&encoded_public_key_cbs, encoded_public_key,
             sizeof(encoded_public_key));
    if (!MLKEM1024_parse_public_key(&pub, &encoded_public_key_cbs)) {
      state.SkipWithError("Failure in MLKEM1024_parse_public_key.");
      return;
    }
    benchmark::DoNotOptimize(pub);
  }
}

void BM_SpeedMLKEM1024EncapOnly(benchmark::State &state) {
  for (auto _ : state) {
    state.PauseTiming();
    MLKEM1024_public_key pub;
    {
      // On heap to avoid stack frame size limit.
      auto priv = std::make_unique<MLKEM1024_private_key>();
      uint8_t encoded_public_key[MLKEM1024_PUBLIC_KEY_BYTES];
      MLKEM1024_generate_key(encoded_public_key, nullptr, &*priv);
      MLKEM1024_public_from_private(&pub, &*priv);
    }
    benchmark::DoNotOptimize(pub);
    state.ResumeTiming();

    uint8_t shared_secret[MLKEM_SHARED_SECRET_BYTES];
    uint8_t ciphertext[MLKEM1024_CIPHERTEXT_BYTES];
    MLKEM1024_encap(ciphertext, shared_secret, &pub);
    benchmark::DoNotOptimize(ciphertext);
    benchmark::DoNotOptimize(shared_secret);
  }
}

BSSL_BENCH_LAZY_REGISTER() {
  BENCHMARK(BM_SpeedMLKEM768KeyGenDecap)->Apply(bssl::bench::SetThreads);
  BENCHMARK(BM_SpeedMLKEM768ParseEncap)->Apply(bssl::bench::SetThreads);
  BENCHMARK(BM_SpeedMLKEM1024KeyGenDecap)->Apply(bssl::bench::SetThreads);
  BENCHMARK(BM_SpeedMLKEM1024ParseEncap)->Apply(bssl::bench::SetThreads);

  BENCHMARK(BM_SpeedMLKEM768KeyGenOnly)->Apply(bssl::bench::SetThreads);
  BENCHMARK(BM_SpeedMLKEM768DecapOnly)->Apply(bssl::bench::SetThreads);
  BENCHMARK(BM_SpeedMLKEM768ParseOnly)->Apply(bssl::bench::SetThreads);
  BENCHMARK(BM_SpeedMLKEM768EncapOnly)->Apply(bssl::bench::SetThreads);
  BENCHMARK(BM_SpeedMLKEM1024KeyGenOnly)->Apply(bssl::bench::SetThreads);
  BENCHMARK(BM_SpeedMLKEM1024DecapOnly)->Apply(bssl::bench::SetThreads);
  BENCHMARK(BM_SpeedMLKEM1024ParseOnly)->Apply(bssl::bench::SetThreads);
  BENCHMARK(BM_SpeedMLKEM1024EncapOnly)->Apply(bssl::bench::SetThreads);
}

}  // namespace
