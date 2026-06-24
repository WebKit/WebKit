/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_FRAME_DECODE_TIMING_H_
#define VIDEO_FRAME_DECODE_TIMING_H_

#include <stdint.h>

#include <optional>

#include "api/field_trials_view.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/video_coding/timing/timing.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

class FrameDecodeTiming {
 public:
  FrameDecodeTiming(Clock* clock,
                    VCMTiming const* timing,
                    const FieldTrialsView& field_trials);
  ~FrameDecodeTiming() = default;
  FrameDecodeTiming(const FrameDecodeTiming&) = delete;
  FrameDecodeTiming& operator=(const FrameDecodeTiming&) = delete;

  // Any frame that has decode delay more than this in the past can be
  // fast-forwarded.
  static constexpr TimeDelta kMaxAllowedFrameDelay = TimeDelta::Millis(5);

  struct FrameSchedule {
    Timestamp latest_decode_time;
    Timestamp render_time;
  };

  std::optional<FrameSchedule> OnFrameBufferUpdated(
      uint32_t next_temporal_unit_rtp,
      uint32_t last_temporal_unit_rtp,
      TimeDelta max_wait_for_frame,
      bool too_many_frames_queued);

  // Updates the last time a frame was scheduled for decoding.
  void SetLastDecodeScheduledTimestamp(Timestamp last_decode_scheduled);

  // Returns the maximum time that we can wait for a frame to become complete
  // before it must be passed to the decoder. render_time==0 indicates that the
  // frames should be processed as quickly as possible, with possibly only a
  // small delay added to make sure that the decoder is not overloaded.
  // The parameter too_many_frames_queued is used to signal that the decode
  // queue is full and that the frame should be decoded as soon as possible.
  TimeDelta MaxWaitingTime(Timestamp render_time,
                           Timestamp now,
                           bool too_many_frames_queued) const;

 private:
  Clock* const clock_;
  VCMTiming const* const timing_;

  // Set by the field trial WebRTC-ZeroPlayoutDelay. The parameter min_pacing
  // determines the minimum delay between frames scheduled for decoding that is
  // used when min playout delay=0 and max playout delay>=0.
  FieldTrialParameter<TimeDelta> zero_playout_delay_min_pacing_;

  // Timestamp at which the last frame was scheduled to be sent to the decoder.
  // Used only when the RTP header extension playout delay is set to min=0 ms
  // which is indicated by a render time set to 0.
  Timestamp last_decode_scheduled_ = Timestamp::Zero();
};

}  // namespace webrtc

#endif  // VIDEO_FRAME_DECODE_TIMING_H_
