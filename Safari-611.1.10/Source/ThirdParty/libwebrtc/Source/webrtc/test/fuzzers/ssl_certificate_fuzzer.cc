/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "rtc_base/ssl_certificate.h"
#include "rtc_base/string_encode.h"

namespace webrtc {

void FuzzOneInput(const uint8_t* data, size_t size) {
  std::string pem_certificate(reinterpret_cast<const char*>(data), size);

  std::unique_ptr<rtc::SSLCertificate> cert =
      rtc::SSLCertificate::FromPEMString(pem_certificate);

  if (cert == nullptr) {
    return;
  }

  cert->Clone();
  cert->GetStats();
  cert->ToPEMString();
  cert->CertificateExpirationTime();

  std::string algorithm;
  cert->GetSignatureDigestAlgorithm(algorithm);

  unsigned char digest[rtc::MessageDigest::kMaxSize];
  size_t digest_len;
  cert->ComputeDigest(algorithm, digest, rtc::MessageDigest::kMaxSize,
                      &digest_len);

  rtc::Buffer der_buffer;
  cert->ToDER(&der_buffer);
}

}  // namespace webrtc
