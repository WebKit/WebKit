/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_STREAM_SYNCHRONIZATION_H_
#define WEBRTC_VIDEO_STREAM_SYNCHRONIZATION_H_

#include <list>

#include "webrtc/system_wrappers/include/rtp_to_ntp.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class StreamSynchronization {
 public:
  struct Measurements {
    Measurements() : rtcp(), latest_receive_time_ms(0), latest_timestamp(0) {}
    RtcpMeasurements rtcp;
    int64_t latest_receive_time_ms;
    uint32_t latest_timestamp;
  };

  StreamSynchronization(uint32_t video_primary_ssrc, int audio_channel_id);

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
  const uint32_t video_primary_ssrc_;
  const int audio_channel_id_;
  int base_target_delay_ms_;
  int avg_diff_ms_;
};
}  // namespace webrtc

#endif  // WEBRTC_VIDEO_STREAM_SYNCHRONIZATION_H_
