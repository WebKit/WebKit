/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/key_derivation.h"

#include "absl/memory/memory.h"
#include "rtc_base/openssl_key_derivation_hkdf.h"

namespace rtc {

KeyDerivation::KeyDerivation() = default;
KeyDerivation::~KeyDerivation() = default;

// static
std::unique_ptr<KeyDerivation> KeyDerivation::Create(
    KeyDerivationAlgorithm key_derivation_algorithm) {
  switch (key_derivation_algorithm) {
    case KeyDerivationAlgorithm::HKDF_SHA256:
      return absl::make_unique<OpenSSLKeyDerivationHKDF>();
  }
  RTC_NOTREACHED();
}

}  // namespace rtc
