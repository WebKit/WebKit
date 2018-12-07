/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/sslfingerprint.h"

#include <ctype.h>
#include <algorithm>
#include <cstdint>
#include <string>

#include "absl/memory/memory.h"
#include "rtc_base/logging.h"
#include "rtc_base/messagedigest.h"
#include "rtc_base/rtccertificate.h"
#include "rtc_base/sslcertificate.h"
#include "rtc_base/sslidentity.h"
#include "rtc_base/stringencode.h"

namespace rtc {

SSLFingerprint* SSLFingerprint::Create(const std::string& algorithm,
                                       const rtc::SSLIdentity* identity) {
  return CreateUnique(algorithm, *identity).release();
}

std::unique_ptr<SSLFingerprint> SSLFingerprint::CreateUnique(
    const std::string& algorithm,
    const rtc::SSLIdentity& identity) {
  return Create(algorithm, identity.certificate());
}

std::unique_ptr<SSLFingerprint> SSLFingerprint::Create(
    const std::string& algorithm,
    const rtc::SSLCertificate& cert) {
  uint8_t digest_val[64];
  size_t digest_len;
  bool ret = cert.ComputeDigest(algorithm, digest_val, sizeof(digest_val),
                                &digest_len);
  if (!ret) {
    return nullptr;
  }
  return absl::make_unique<SSLFingerprint>(
      algorithm, ArrayView<const uint8_t>(digest_val, digest_len));
}

SSLFingerprint* SSLFingerprint::CreateFromRfc4572(
    const std::string& algorithm,
    const std::string& fingerprint) {
  return CreateUniqueFromRfc4572(algorithm, fingerprint).release();
}

std::unique_ptr<SSLFingerprint> SSLFingerprint::CreateUniqueFromRfc4572(
    const std::string& algorithm,
    const std::string& fingerprint) {
  if (algorithm.empty() || !rtc::IsFips180DigestAlgorithm(algorithm))
    return nullptr;

  if (fingerprint.empty())
    return nullptr;

  char value[rtc::MessageDigest::kMaxSize];
  size_t value_len = rtc::hex_decode_with_delimiter(
      value, sizeof(value), fingerprint.c_str(), fingerprint.length(), ':');
  if (!value_len)
    return nullptr;

  return absl::make_unique<SSLFingerprint>(
      algorithm,
      ArrayView<const uint8_t>(reinterpret_cast<uint8_t*>(value), value_len));
}

std::unique_ptr<SSLFingerprint> SSLFingerprint::CreateFromCertificate(
    const RTCCertificate& cert) {
  std::string digest_alg;
  if (!cert.GetSSLCertificate().GetSignatureDigestAlgorithm(&digest_alg)) {
    RTC_LOG(LS_ERROR)
        << "Failed to retrieve the certificate's digest algorithm";
    return nullptr;
  }

  std::unique_ptr<SSLFingerprint> fingerprint =
      CreateUnique(digest_alg, *cert.identity());
  if (!fingerprint) {
    RTC_LOG(LS_ERROR) << "Failed to create identity fingerprint, alg="
                      << digest_alg;
  }
  return fingerprint;
}

SSLFingerprint::SSLFingerprint(const std::string& algorithm,
                               ArrayView<const uint8_t> digest_view)
    : algorithm(algorithm), digest(digest_view.data(), digest_view.size()) {}

SSLFingerprint::SSLFingerprint(const std::string& algorithm,
                               const uint8_t* digest_in,
                               size_t digest_len)
    : SSLFingerprint(algorithm, MakeArrayView(digest_in, digest_len)) {}

SSLFingerprint::SSLFingerprint(const SSLFingerprint& from)
    : algorithm(from.algorithm), digest(from.digest) {}

bool SSLFingerprint::operator==(const SSLFingerprint& other) const {
  return algorithm == other.algorithm && digest == other.digest;
}

std::string SSLFingerprint::GetRfc4572Fingerprint() const {
  std::string fingerprint =
      rtc::hex_encode_with_delimiter(digest.data<char>(), digest.size(), ':');
  std::transform(fingerprint.begin(), fingerprint.end(), fingerprint.begin(),
                 ::toupper);
  return fingerprint;
}

std::string SSLFingerprint::ToString() const {
  std::string fp_str = algorithm;
  fp_str.append(" ");
  fp_str.append(GetRfc4572Fingerprint());
  return fp_str;
}

}  // namespace rtc
