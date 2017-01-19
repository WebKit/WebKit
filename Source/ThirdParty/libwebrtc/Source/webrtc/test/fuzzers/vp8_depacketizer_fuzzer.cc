/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/modules/rtp_rtcp/source/rtp_format_vp8.h"

namespace webrtc {
void FuzzOneInput(const uint8_t* data, size_t size) {
  RtpDepacketizerVp8 depacketizer;
  RtpDepacketizer::ParsedPayload parsed_payload;
  depacketizer.Parse(&parsed_payload, data, size);
}
}  // namespace webrtc
