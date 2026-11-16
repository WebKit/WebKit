// Copyright 2023 The BoringSSL Authors
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

#if defined(OPENSSL_RAND_IOS)
#include <stdlib.h>

#include <CommonCrypto/CommonRandom.h>

using namespace bssl;

void bssl::CRYPTO_init_sysrand() {}

void bssl::CRYPTO_sysrand(uint8_t *out, size_t requested) {
  if (CCRandomGenerateBytes(out, requested) != kCCSuccess) {
    abort();
  }
}

#endif  // OPENSSL_RAND_IOS
