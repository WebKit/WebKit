/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/video_rtp_depacketizer_raw.h"

#include <optional>
#include <utility>

#include "modules/rtp_rtcp/source/video_rtp_depacketizer.h"
#include "rtc_base/copy_on_write_buffer.h"

namespace webrtc {

std::optional<VideoRtpDepacketizer::ParsedRtpPayload>
VideoRtpDepacketizerRaw::Parse(rtc::CopyOnWriteBuffer rtp_payload) {
  std::optional<ParsedRtpPayload> parsed(std::in_place);
  parsed->video_payload = std::move(rtp_payload);
  return parsed;
}

}  // namespace webrtc
