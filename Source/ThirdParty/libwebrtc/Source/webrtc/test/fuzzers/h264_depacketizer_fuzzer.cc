/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <cstddef>

#include "modules/rtp_rtcp/source/video_rtp_depacketizer_h264.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {
void FuzzOneInput(FuzzDataHelper fuzz_data) {
  if (fuzz_data.size() > 200'000)
    return;
  VideoRtpDepacketizerH264 depacketizer;
  depacketizer.Parse(webrtc::CopyOnWriteBuffer(fuzz_data.ReadRemaining()));
}
}  // namespace webrtc
