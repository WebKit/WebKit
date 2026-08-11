// Copyright 2016 The BoringSSL Authors
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

#include <openssl/rand.h>

#include "../bcm_support.h"
#include "internal.h"

#if defined(OPENSSL_RAND_DETERMINISTIC)

#include <string.h>

#include <openssl/chacha.h>

#include "../internal.h"


using namespace bssl;

// g_num_calls is the number of calls to |CRYPTO_sysrand| that have occurred.
//
// This is intentionally not thread-safe. If the fuzzer mode is ever used in a
// multi-threaded program, replace this with a thread-local. (A mutex would not
// be deterministic.)
static uint64_t g_num_calls = 0;
static StaticMutex g_num_calls_lock;

void RAND_reset_for_fuzzing() { g_num_calls = 0; }

void bssl::CRYPTO_init_sysrand() {}

void bssl::CRYPTO_sysrand(uint8_t *out, size_t requested) {
  static const uint8_t kZeroKey[32] = {0};

  g_num_calls_lock.LockWrite();
  uint64_t num_calls = g_num_calls++;
  g_num_calls_lock.UnlockWrite();

  uint8_t nonce[12];
  OPENSSL_memset(nonce, 0, sizeof(nonce));
  OPENSSL_memcpy(nonce, &num_calls, sizeof(num_calls));

  OPENSSL_memset(out, 0, requested);
  CRYPTO_chacha_20(out, out, requested, kZeroKey, nonce, 0);
}

#endif  // OPENSSL_RAND_DETERMINISTIC
