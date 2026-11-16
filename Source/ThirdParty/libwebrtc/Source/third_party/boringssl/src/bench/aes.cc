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

#include <openssl/aes.h>
#include <openssl/base.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/mem.h>

#include <benchmark/benchmark.h>

#include "./internal.h"

namespace {

const uint8_t kZero[32] = {0};

template <size_t bits>
void BM_SpeedAESBlockEncryptSetup(benchmark::State &state) {
  for (auto _ : state) {
    AES_KEY key;
    if (AES_set_encrypt_key(kZero, bits, &key)) {
      state.SkipWithError("AES_set_encrypt_key failed.");
      return;
    }
    benchmark::DoNotOptimize(key);
  }
}

template <size_t bits>
void BM_SpeedAESBlockEncrypt(benchmark::State &state) {
  AES_KEY key;
  if (AES_set_encrypt_key(kZero, bits, &key)) {
    state.SkipWithError("AES_set_encrypt_key failed.");
    return;
  }
  uint8_t block[16] = {0};
  for (auto _ : state) {
    AES_encrypt(block, block, &key);
  }
}

template <size_t bits>
void BM_SpeedAESBlockDecryptSetup(benchmark::State &state) {
  for (auto _ : state) {
    AES_KEY key;
    if (AES_set_decrypt_key(kZero, bits, &key)) {
      state.SkipWithError("AES_set_decrypt_key failed.");
      return;
    }
    benchmark::DoNotOptimize(key);
  }
}

template <size_t bits>
void BM_SpeedAESBlockDecrypt(benchmark::State &state) {
  AES_KEY key;
  if (AES_set_decrypt_key(kZero, bits, &key)) {
    state.SkipWithError("AES_set_decrypt_key failed.");
    return;
  }
  uint8_t block[16] = {0};
  for (auto _ : state) {
    AES_decrypt(block, block, &key);
  }
}

BSSL_BENCH_LAZY_REGISTER() {
  BENCHMARK(BM_SpeedAESBlockEncryptSetup<128>)->Apply(bssl::bench::SetThreads);
  BENCHMARK(BM_SpeedAESBlockEncryptSetup<256>)->Apply(bssl::bench::SetThreads);

  BENCHMARK(BM_SpeedAESBlockEncrypt<128>)->Apply(bssl::bench::SetThreads);
  BENCHMARK(BM_SpeedAESBlockEncrypt<256>)->Apply(bssl::bench::SetThreads);

  BENCHMARK(BM_SpeedAESBlockDecryptSetup<128>)->Apply(bssl::bench::SetThreads);
  BENCHMARK(BM_SpeedAESBlockDecryptSetup<256>)->Apply(bssl::bench::SetThreads);

  BENCHMARK(BM_SpeedAESBlockDecrypt<128>)->Apply(bssl::bench::SetThreads);
  BENCHMARK(BM_SpeedAESBlockDecrypt<256>)->Apply(bssl::bench::SetThreads);
}

}  // namespace
