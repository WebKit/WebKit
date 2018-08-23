/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_VIDEO_QUALITY_OBSERVER_H_
#define VIDEO_VIDEO_QUALITY_OBSERVER_H_

#include <stdint.h>
#include <vector>

#include "absl/types/optional.h"
#include "api/video/video_content_type.h"
#include "common_types.h"  // NOLINT(build/include)
#include "rtc_base/numerics/sample_counter.h"

namespace webrtc {

// Calculates spatial and temporal quality metrics and reports them to UMA
// stats.
class VideoQualityObserver {
 public:
  // Use either VideoQualityObserver::kBlockyQpThresholdVp8 or
  // VideoQualityObserver::kBlockyQpThresholdVp9.
  explicit VideoQualityObserver(VideoContentType content_type);
  ~VideoQualityObserver();

  void OnDecodedFrame(absl::optional<uint8_t> qp,
                      int width,
                      int height,
                      int64_t now_ms,
                      VideoCodecType codec);

  void OnStreamInactive();

 private:
  void UpdateHistograms();

  enum Resolution {
    Low = 0,
    Medium = 1,
    High = 2,
  };

  int64_t last_frame_decoded_ms_;
  int64_t num_frames_decoded_;
  int64_t first_frame_decoded_ms_;
  int64_t last_frame_pixels_;
  uint8_t last_frame_qp_;
  // Decoded timestamp of the last delayed frame.
  int64_t last_unfreeze_time_;
  rtc::SampleCounter interframe_delays_;
  // An inter-frame delay is counted as a freeze if it's significantly longer
  // than average inter-frame delay.
  rtc::SampleCounter freezes_durations_;
  // Time between freezes.
  rtc::SampleCounter smooth_playback_durations_;
  // Counters for time spent in different resolutions. Time between each two
  // Consecutive frames is counted to bin corresponding to the first frame
  // resolution.
  std::vector<int64_t> time_in_resolution_ms_;
  // Resolution of the last decoded frame. Resolution enum is used as an index.
  Resolution current_resolution_;
  int num_resolution_downgrades_;
  // Similar to resolution, time spent in high-QP video.
  int64_t time_in_blocky_video_ms_;
  // Content type of the last decoded frame.
  VideoContentType content_type_;
  bool is_paused_;
};

}  // namespace webrtc

#endif  // VIDEO_VIDEO_QUALITY_OBSERVER_H_
