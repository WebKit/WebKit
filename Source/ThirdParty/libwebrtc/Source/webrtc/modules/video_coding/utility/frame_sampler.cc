/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/utility/frame_sampler.h"

#include <cstdint>

#include "api/units/time_delta.h"
#include "api/video/video_frame.h"
#include "modules/include/module_common_types_public.h"

namespace webrtc {

FrameSampler::FrameSampler(TimeDelta interval) : sampling_interval_(interval) {}

bool FrameSampler::ShouldBeSampled(const VideoFrame& frame) {
  // RTP timestamps use a 90kHz clock.
  const int64_t interval_rtp = sampling_interval_.ms() * 90;
  if (!last_rtp_timestamp_sampled_) {
    // Since we can not know the frame rate from the first frame,
    // assume 30fps for the extrapolation.
    last_rtp_timestamp_ = frame.rtp_timestamp() + interval_rtp / /*fps=*/30;
    last_rtp_timestamp_sampled_ = frame.rtp_timestamp();
    return true;
  }
  // Since getStats is commonly called once per second, sample if the
  // extrapolated RTP timestamp of the next frame would be be too late for this.
  // This is not strictly necessary but makes plotting the values once per
  // second much easier.
  uint32_t extrapolated_rtp_timestamp =
      frame.rtp_timestamp() + (frame.rtp_timestamp() - *last_rtp_timestamp_);
  last_rtp_timestamp_ = frame.rtp_timestamp();

  if (IsNewerTimestamp(extrapolated_rtp_timestamp,
                       *last_rtp_timestamp_sampled_ + interval_rtp)) {
    last_rtp_timestamp_sampled_ = frame.rtp_timestamp();
    return true;
  }
  return false;
}

}  // namespace webrtc
