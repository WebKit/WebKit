/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "ssl_certificate_verifier.h"

#include "rtc_base/buffer.h"

namespace {

class SSLCertificateVerifierAdapter final : public rtc::SSLCertificateVerifier {
 public:
  SSLCertificateVerifierAdapter(
      id<RTC_OBJC_TYPE(RTCSSLCertificateVerifier)> objc_certificate_verifier)
      : objc_certificate_verifier_(objc_certificate_verifier) {
    RTC_DCHECK(objc_certificate_verifier_ != nil);
  }

  bool Verify(const rtc::SSLCertificate& certificate) override {
    @autoreleasepool {
      rtc::Buffer der_buffer;
      certificate.ToDER(&der_buffer);
      NSData* serialized_certificate = [[NSData alloc] initWithBytes:der_buffer.data()
                                                              length:der_buffer.size()];
      return [objc_certificate_verifier_ verify:serialized_certificate];
    }
  }

 private:
  id<RTC_OBJC_TYPE(RTCSSLCertificateVerifier)> objc_certificate_verifier_;
};

}

namespace webrtc {

std::unique_ptr<rtc::SSLCertificateVerifier> ObjCToNativeCertificateVerifier(
    id<RTC_OBJC_TYPE(RTCSSLCertificateVerifier)> objc_certificate_verifier) {
  return std::make_unique<SSLCertificateVerifierAdapter>(objc_certificate_verifier);
}

}  // namespace webrtc
