/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/rtpsenderinterface.h"

namespace webrtc {

void RtpSenderInterface::SetFrameEncryptor(
    rtc::scoped_refptr<FrameEncryptorInterface> frame_encryptor) {}

rtc::scoped_refptr<FrameEncryptorInterface>
RtpSenderInterface::GetFrameEncryptor() const {
  return nullptr;
}

std::vector<RtpEncodingParameters> RtpSenderInterface::init_send_encodings()
    const {
  return {};
}

}  // namespace webrtc
