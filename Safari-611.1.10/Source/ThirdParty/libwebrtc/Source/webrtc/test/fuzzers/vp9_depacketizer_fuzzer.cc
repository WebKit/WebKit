/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/array_view.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer_vp9.h"

namespace webrtc {
void FuzzOneInput(const uint8_t* data, size_t size) {
  RTPVideoHeader video_header;
  VideoRtpDepacketizerVp9::ParseRtpPayload(rtc::MakeArrayView(data, size),
                                           &video_header);
}
}  // namespace webrtc
