/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_STREAM_SYNCHRONIZATION_H_
#define VIDEO_STREAM_SYNCHRONIZATION_H_

#include <list>

#include "system_wrappers/include/rtp_to_ntp_estimator.h"

namespace webrtc {

class StreamSynchronization {
 public:
  struct Measurements {
    Measurements() : latest_receive_time_ms(0), latest_timestamp(0) {}
    RtpToNtpEstimator rtp_to_ntp;
    int64_t latest_receive_time_ms;
    uint32_t latest_timestamp;
  };

  StreamSynchronization(int video_stream_id, int audio_stream_id);

  bool ComputeDelays(int relative_delay_ms,
                     int current_audio_delay_ms,
                     int* extra_audio_delay_ms,
                     int* total_video_delay_target_ms);

  // On success |relative_delay| contains the number of milliseconds later video
  // is rendered relative audio. If audio is played back later than video a
  // |relative_delay| will be negative.
  static bool ComputeRelativeDelay(const Measurements& audio_measurement,
                                   const Measurements& video_measurement,
                                   int* relative_delay_ms);
  // Set target buffering delay - All audio and video will be delayed by at
  // least target_delay_ms.
  void SetTargetBufferingDelay(int target_delay_ms);

 private:
  struct SynchronizationDelays {
    int extra_video_delay_ms = 0;
    int last_video_delay_ms = 0;
    int extra_audio_delay_ms = 0;
    int last_audio_delay_ms = 0;
  };

  SynchronizationDelays channel_delay_;
  const int video_stream_id_;
  const int audio_stream_id_;
  int base_target_delay_ms_;
  int avg_diff_ms_;
};
}  // namespace webrtc

#endif  // VIDEO_STREAM_SYNCHRONIZATION_H_
