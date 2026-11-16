// Copyright 2018 The BoringSSL Authors
// Copyright (c) 2020, Arm Ltd.
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

#include "internal.h"

#if defined(OPENSSL_AARCH64) && defined(OPENSSL_WINDOWS) && \
    !defined(OPENSSL_STATIC_ARMCAP) && !defined(OPENSSL_NO_ASM)

#include <windows.h>


#if !defined(PF_ARM_SHA3_INSTRUCTIONS_AVAILABLE)
#define PF_ARM_SHA3_INSTRUCTIONS_AVAILABLE 64
#endif
#if !defined(PF_ARM_SHA512_INSTRUCTIONS_AVAILABLE)
#define PF_ARM_SHA512_INSTRUCTIONS_AVAILABLE 65
#endif

using namespace bssl;

void bssl::OPENSSL_cpuid_setup() {
  // We do not need to check for the presence of NEON, as Armv8-A always has it
  OPENSSL_armcap_P |= ARMV7_NEON;

  if (IsProcessorFeaturePresent(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE)) {
    // These are all covered by one call in Windows
    OPENSSL_armcap_P |= ARMV8_AES;
    OPENSSL_armcap_P |= ARMV8_PMULL;
    OPENSSL_armcap_P |= ARMV8_SHA1;
    OPENSSL_armcap_P |= ARMV8_SHA256;
  }
  if (IsProcessorFeaturePresent(PF_ARM_SHA512_INSTRUCTIONS_AVAILABLE)) {
    OPENSSL_armcap_P |= ARMV8_SHA512;
  }
  if (IsProcessorFeaturePresent(PF_ARM_SHA3_INSTRUCTIONS_AVAILABLE)) {
    OPENSSL_armcap_P |= ARMV8_SHA3;
  }
}

#endif  // OPENSSL_AARCH64 && OPENSSL_WINDOWS && !OPENSSL_STATIC_ARMCAP
