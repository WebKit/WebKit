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

#include <memory>
#include <string>

#include "rtc_base/buffer.h"
#include "rtc_base/message_digest.h"
#include "rtc_base/ssl_certificate.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {

void FuzzOneInput(FuzzDataHelper fuzz_data) {
  std::unique_ptr<SSLCertificate> cert =
      SSLCertificate::FromPEMString(fuzz_data.ReadString());

  if (cert == nullptr) {
    return;
  }

  cert->Clone();
  cert->GetStats();
  cert->ToPEMString();
  cert->CertificateExpirationTime();

  std::string algorithm;
  cert->GetSignatureDigestAlgorithm(&algorithm);

  Buffer buffer(Buffer::CreateWithCapacity(MessageDigest::kMaxSize));
  cert->ComputeDigest(algorithm, buffer);

  Buffer der_buffer;
  cert->ToDER(&der_buffer);
}

}  // namespace webrtc
