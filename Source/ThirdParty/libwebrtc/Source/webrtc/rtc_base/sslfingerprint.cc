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
#include <string>

#include "rtc_base/helpers.h"
#include "rtc_base/logging.h"
#include "rtc_base/messagedigest.h"
#include "rtc_base/stringencode.h"

namespace rtc {

SSLFingerprint* SSLFingerprint::Create(const std::string& algorithm,
                                       const rtc::SSLIdentity* identity) {
  if (!identity) {
    return nullptr;
  }

  return Create(algorithm, &(identity->certificate()));
}

SSLFingerprint* SSLFingerprint::Create(const std::string& algorithm,
                                       const rtc::SSLCertificate* cert) {
  uint8_t digest_val[64];
  size_t digest_len;
  bool ret = cert->ComputeDigest(algorithm, digest_val, sizeof(digest_val),
                                 &digest_len);
  if (!ret) {
    return nullptr;
  }

  return new SSLFingerprint(algorithm, digest_val, digest_len);
}

SSLFingerprint* SSLFingerprint::CreateFromRfc4572(
    const std::string& algorithm,
    const std::string& fingerprint) {
  if (algorithm.empty() || !rtc::IsFips180DigestAlgorithm(algorithm))
    return nullptr;

  if (fingerprint.empty())
    return nullptr;

  size_t value_len;
  char value[rtc::MessageDigest::kMaxSize];
  value_len = rtc::hex_decode_with_delimiter(
      value, sizeof(value), fingerprint.c_str(), fingerprint.length(), ':');
  if (!value_len)
    return nullptr;

  return new SSLFingerprint(algorithm, reinterpret_cast<uint8_t*>(value),
                            value_len);
}

SSLFingerprint* SSLFingerprint::CreateFromCertificate(
    const RTCCertificate* cert) {
  std::string digest_alg;
  if (!cert->ssl_certificate().GetSignatureDigestAlgorithm(&digest_alg)) {
    RTC_LOG(LS_ERROR)
        << "Failed to retrieve the certificate's digest algorithm";
    return nullptr;
  }

  SSLFingerprint* fingerprint = Create(digest_alg, cert->identity());
  if (!fingerprint) {
    RTC_LOG(LS_ERROR) << "Failed to create identity fingerprint, alg="
                      << digest_alg;
  }
  return fingerprint;
}

SSLFingerprint::SSLFingerprint(const std::string& algorithm,
                               const uint8_t* digest_in,
                               size_t digest_len)
    : algorithm(algorithm) {
  digest.SetData(digest_in, digest_len);
}

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
