/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_KEY_DERIVATION_H_
#define RTC_BASE_KEY_DERIVATION_H_

#include <memory>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "rtc_base/buffer.h"
#include "rtc_base/constructormagic.h"

namespace rtc {

// Defines the set of key derivation algorithms that are supported. It is ideal
// to keep this list as small as possible.
enum class KeyDerivationAlgorithm {
  // This algorithm is not suitable to generate a key from a password. Please
  // only use with a cryptographically random master secret.
  HKDF_SHA256
};

// KeyDerivation provides a generic interface for deriving keys in WebRTC. This
// class should be used over directly accessing openssl or boringssl primitives
// so that we can maintain seperate implementations.
// Example:
//   auto kd = KeyDerivation::Create(KeyDerivationAlgorithm::HDKF_SHA526);
//   if (kd == nullptr) return;
//   auto derived_key_or = kd->DeriveKey(secret, salt, label);
//   if (!derived_key_or.ok()) return;
//   DoSomethingWithKey(derived_key_or.value());
class KeyDerivation {
 public:
  KeyDerivation();
  virtual ~KeyDerivation();

  // Derives a new key from existing key material.
  // secret - The random secret value you wish to derive a key from.
  // salt - Optional but recommended (non secret) cryptographically random.
  // label - A non secret but unique label value to determine the derivation.
  // derived_key_byte_size - This must be at least 128 bits.
  // return - An optional ZeroOnFreeBuffer containing the derived key or
  // absl::nullopt. Nullopt indicates a failure in derivation.
  virtual absl::optional<ZeroOnFreeBuffer<uint8_t>> DeriveKey(
      rtc::ArrayView<const uint8_t> secret,
      rtc::ArrayView<const uint8_t> salt,
      rtc::ArrayView<const uint8_t> label,
      size_t derived_key_byte_size) = 0;

  // Static factory that will return an implementation that is capable of
  // handling the key derivation with the requested algorithm. If no
  // implementation is available nullptr will be returned.
  static std::unique_ptr<KeyDerivation> Create(
      KeyDerivationAlgorithm key_derivation_algorithm);

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(KeyDerivation);
};

}  // namespace rtc

#endif  // RTC_BASE_KEY_DERIVATION_H_
