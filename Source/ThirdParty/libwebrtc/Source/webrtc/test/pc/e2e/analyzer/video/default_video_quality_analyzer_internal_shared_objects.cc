/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_internal_shared_objects.h"

#include "api/video/video_frame.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {

std::string InternalStatsKey::ToString() const {
  rtc::StringBuilder out;
  out << "stream=" << stream << "_sender=" << sender
      << "_receiver=" << receiver;
  return out.str();
}

bool operator<(const InternalStatsKey& a, const InternalStatsKey& b) {
  if (a.stream != b.stream) {
    return a.stream < b.stream;
  }
  if (a.sender != b.sender) {
    return a.sender < b.sender;
  }
  return a.receiver < b.receiver;
}

bool operator==(const InternalStatsKey& a, const InternalStatsKey& b) {
  return a.stream == b.stream && a.sender == b.sender &&
         a.receiver == b.receiver;
}

FrameComparison::FrameComparison(InternalStatsKey stats_key,
                                 absl::optional<VideoFrame> captured,
                                 absl::optional<VideoFrame> rendered,
                                 FrameComparisonType type,
                                 FrameStats frame_stats,
                                 OverloadReason overload_reason)
    : stats_key(std::move(stats_key)),
      captured(std::move(captured)),
      rendered(std::move(rendered)),
      type(type),
      frame_stats(std::move(frame_stats)),
      overload_reason(overload_reason) {}

}  // namespace webrtc
