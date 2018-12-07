/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_OPENSSL_KEY_DERIVATION_HKDF_H_
#define RTC_BASE_OPENSSL_KEY_DERIVATION_HKDF_H_

#include "rtc_base/constructormagic.h"
#include "rtc_base/key_derivation.h"

namespace rtc {

// OpenSSLKeyDerivationHKDF provides a concrete implementation of the
// KeyDerivation interface to support the HKDF algorithm using the
// OpenSSL/BoringSSL internal implementation.
class OpenSSLKeyDerivationHKDF final : public KeyDerivation {
 public:
  OpenSSLKeyDerivationHKDF();
  ~OpenSSLKeyDerivationHKDF() override;

  // General users shouldn't be generating keys smaller than 128 bits.
  static const size_t kMinKeyByteSize;
  // The maximum available derivation size 255*DIGEST_LENGTH
  static const size_t kMaxKeyByteSize;
  // The minimum acceptable secret size.
  static const size_t kMinSecretByteSize;

  // Derives a new key from existing key material using HKDF.
  // secret - The random secret value you wish to derive a key from.
  // salt - Optional (non secret) cryptographically random value.
  // label - A non secret but unique label value to determine the derivation.
  // derived_key_byte_size - The size of the derived key.
  // return - A ZeroOnFreeBuffer containing the derived key or an error
  // condition. Checking error codes is explicit in the API and error should
  // never be ignored.
  absl::optional<ZeroOnFreeBuffer<uint8_t>> DeriveKey(
      rtc::ArrayView<const uint8_t> secret,
      rtc::ArrayView<const uint8_t> salt,
      rtc::ArrayView<const uint8_t> label,
      size_t derived_key_byte_size) override;

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(OpenSSLKeyDerivationHKDF);
};

}  // namespace rtc

#endif  // RTC_BASE_OPENSSL_KEY_DERIVATION_HKDF_H_
