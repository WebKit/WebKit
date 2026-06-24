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

#include "./internal.h"

namespace {
const uint8_t kAliceName[] = {'A'};
const uint8_t kBobName[] = {'B'};
const uint8_t kPassword[] = "password";
void BM_SpeedSPAKE2(benchmark::State &state) {
  bssl::UniquePtr<SPAKE2_CTX> alice(
      SPAKE2_CTX_new(spake2_role_alice, kAliceName, sizeof(kAliceName),
                     kBobName, sizeof(kBobName)));
  uint8_t alice_msg[SPAKE2_MAX_MSG_SIZE];
  size_t alice_msg_len;
  if (!SPAKE2_generate_msg(alice.get(), alice_msg, &alice_msg_len,
                           sizeof(alice_msg), kPassword, sizeof(kPassword))) {
    state.SkipWithError("SPAKE2_generate_msg failed.");
    return;
  }
  for (auto _ : state) {
    bssl::UniquePtr<SPAKE2_CTX> bob(SPAKE2_CTX_new(spake2_role_bob, kBobName,
                                                   sizeof(kBobName), kAliceName,
                                                   sizeof(kAliceName)));
    uint8_t bob_msg[SPAKE2_MAX_MSG_SIZE], bob_key[64];
    size_t bob_msg_len, bob_key_len;
    if (!SPAKE2_generate_msg(bob.get(), bob_msg, &bob_msg_len, sizeof(bob_msg),
                             kPassword, sizeof(kPassword)) ||
        !SPAKE2_process_msg(bob.get(), bob_key, &bob_key_len, sizeof(bob_key),
                            alice_msg, alice_msg_len)) {
      state.SkipWithError("SPAKE2_generate_msg or SPAKE2_process_msg failed");
      return;
    }
  }
}

BSSL_BENCH_LAZY_REGISTER() {
  BENCHMARK(BM_SpeedSPAKE2)->Apply(bssl::bench::SetThreads);
}

}  // namespace
