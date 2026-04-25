/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_UTILITY_FRAME_SAMPLER_H_
#define MODULES_VIDEO_CODING_UTILITY_FRAME_SAMPLER_H_

#include <cstdint>
#include <optional>

#include "api/units/time_delta.h"
#include "api/video/video_frame.h"

namespace webrtc {

// Determine whether the frame should be sampled for operations
// not done for every frame but only some of them. An example strategy
// would be to require a minimum time elapsed between two frames based
// on the RTP timestamp difference.
class FrameSampler {
 public:
  explicit FrameSampler(TimeDelta interval);
  FrameSampler(const FrameSampler&) = delete;
  FrameSampler& operator=(const FrameSampler&) = delete;

  bool ShouldBeSampled(const VideoFrame& frame);

 private:
  const TimeDelta sampling_interval_;
  std::optional<uint32_t> last_rtp_timestamp_sampled_;
  std::optional<uint32_t> last_rtp_timestamp_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_UTILITY_FRAME_SAMPLER_H_
