/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SSL_FINGERPRINT_H_
#define RTC_BASE_SSL_FINGERPRINT_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <span>
#include <string>

#include "absl/base/macros.h"
#include "absl/base/nullability.h"
#include "absl/strings/string_view.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/rtc_certificate.h"
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/ssl_identity.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

struct RTC_EXPORT SSLFingerprint {
  static absl_nullable std::unique_ptr<SSLFingerprint> Create(
      absl::string_view algorithm,
      const SSLIdentity& identity);

  static absl_nullable std::unique_ptr<SSLFingerprint> Create(
      absl::string_view algorithm,
      const SSLCertificate& cert);

  static absl_nullable std::unique_ptr<SSLFingerprint> CreateFromRfc4572(
      absl::string_view algorithm,
      absl::string_view fingerprint);

  static std::optional<SSLFingerprint> CreateOptionalFromRfc4572(
      absl::string_view algorithm,
      absl::string_view fingerprint);

  // Creates a fingerprint from a certificate, using the same digest algorithm
  // as the certificate's signature.
  static absl_nullable std::unique_ptr<SSLFingerprint> CreateFromCertificate(
      const RTCCertificate& cert);

  [[deprecated]] ABSL_REFACTOR_INLINE static absl_nullable
      std::unique_ptr<SSLFingerprint>
      CreateUnique(absl::string_view algorithm, const SSLIdentity& identity) {
    return Create(algorithm, identity);
  }

  [[deprecated]] ABSL_REFACTOR_INLINE static absl_nullable
      std::unique_ptr<SSLFingerprint>
      CreateUniqueFromRfc4572(absl::string_view algorithm,
                              absl::string_view fingerprint) {
    return CreateFromRfc4572(algorithm, fingerprint);
  }

  SSLFingerprint(absl::string_view algorithm,
                 std::span<const uint8_t> digest_view);

  SSLFingerprint(const SSLFingerprint& from) = default;
  SSLFingerprint& operator=(const SSLFingerprint& from) = default;

  bool operator==(const SSLFingerprint& other) const;

  std::string GetRfc4572Fingerprint() const;

  std::string ToString() const;

  std::string algorithm;
  CopyOnWriteBuffer digest;
};

}  //  namespace webrtc


#endif  // RTC_BASE_SSL_FINGERPRINT_H_
