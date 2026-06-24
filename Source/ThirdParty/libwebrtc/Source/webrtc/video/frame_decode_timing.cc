/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/frame_decode_timing.h"

#include <algorithm>
#include <cstdint>
#include <optional>

#include "api/field_trials_view.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/video_coding/timing/timing.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace {

// Default pacing that is used for the low-latency renderer path.
constexpr TimeDelta kZeroPlayoutDelayDefaultMinPacing = TimeDelta::Millis(8);

}  // namespace

FrameDecodeTiming::FrameDecodeTiming(Clock* clock,
                                     VCMTiming const* timing,
                                     const FieldTrialsView& field_trials)
    : clock_(clock),
      timing_(timing),
      zero_playout_delay_min_pacing_("min_pacing",
                                     kZeroPlayoutDelayDefaultMinPacing) {
  RTC_DCHECK(clock_);
  RTC_DCHECK(timing_);
  ParseFieldTrial({&zero_playout_delay_min_pacing_},
                  field_trials.Lookup("WebRTC-ZeroPlayoutDelay"));
}

std::optional<FrameDecodeTiming::FrameSchedule>
FrameDecodeTiming::OnFrameBufferUpdated(uint32_t next_temporal_unit_rtp,
                                        uint32_t last_temporal_unit_rtp,
                                        TimeDelta max_wait_for_frame,
                                        bool too_many_frames_queued) {
  RTC_DCHECK_GE(max_wait_for_frame, TimeDelta::Zero());
  const Timestamp now = clock_->CurrentTime();
  Timestamp render_time = timing_->RenderTime(next_temporal_unit_rtp, now);
  TimeDelta max_wait = MaxWaitingTime(render_time, now, too_many_frames_queued);

  // If the delay is not too far in the past, or this is the last decodable
  // frame then it is the best frame to be decoded. Otherwise, fast-forward
  // to the next frame in the buffer.
  if (max_wait <= -kMaxAllowedFrameDelay &&
      next_temporal_unit_rtp != last_temporal_unit_rtp) {
    RTC_DLOG(LS_VERBOSE) << "Fast-forwarded frame " << next_temporal_unit_rtp
                         << " render time " << render_time << " with delay "
                         << max_wait;
    return std::nullopt;
  }

  max_wait = std::clamp(max_wait, TimeDelta::Zero(), max_wait_for_frame);
  RTC_DLOG(LS_VERBOSE) << "Selected frame with rtp " << next_temporal_unit_rtp
                       << " render time " << render_time
                       << " with a max wait of " << max_wait_for_frame
                       << " clamped to " << max_wait;
  Timestamp latest_decode_time = now + max_wait;
  return FrameSchedule{.latest_decode_time = latest_decode_time,
                       .render_time = render_time};
}

void FrameDecodeTiming::SetLastDecodeScheduledTimestamp(
    Timestamp last_decode_scheduled) {
  last_decode_scheduled_ = last_decode_scheduled;
}

TimeDelta FrameDecodeTiming::MaxWaitingTime(Timestamp render_time,
                                            Timestamp now,
                                            bool too_many_frames_queued) const {
  const VCMTiming::VideoDelayTimings timings = timing_->GetTimings();
  if (render_time.IsZero() && zero_playout_delay_min_pacing_->us() > 0 &&
      timings.min_playout_delay.IsZero() &&
      timings.max_playout_delay > TimeDelta::Zero()) {
    // `render_time` == 0 indicates that the frame should be decoded and
    // rendered as soon as possible. However, the decoder can be choked if too
    // many frames are sent at once. Therefore, limit the interframe delay to
    // `zero_playout_delay_min_pacing_` unless too many frames are queued in
    // which case the frames are sent to the decoder at once.
    if (too_many_frames_queued) {
      return TimeDelta::Zero();
    }
    Timestamp earliest_next_decode_start_time =
        last_decode_scheduled_ + zero_playout_delay_min_pacing_;
    TimeDelta max_wait_time = now >= earliest_next_decode_start_time
                                  ? TimeDelta::Zero()
                                  : earliest_next_decode_start_time - now;
    return max_wait_time;
  }
  return render_time - now - timings.estimated_max_decode_time -
         timings.render_delay;
}

}  // namespace webrtc
