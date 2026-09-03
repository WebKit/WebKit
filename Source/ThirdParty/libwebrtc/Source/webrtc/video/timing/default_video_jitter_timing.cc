/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/timing/default_video_jitter_timing.h"

#include <cstdint>
#include <optional>

#include "api/environment/environment.h"
#include "api/field_trials_view.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/timing/video_jitter_timing_interface.h"
#include "system_wrappers/include/clock.h"
#include "video/timing/inter_frame_delay_variation_calculator.h"
#include "video/timing/jitter_estimator.h"
#include "video/timing/timestamp_extrapolator.h"

namespace webrtc {

DefaultVideoJitterTiming::DefaultVideoJitterTiming(const Environment& env)
    : DefaultVideoJitterTiming(&env.clock(), env.field_trials()) {}

DefaultVideoJitterTiming::DefaultVideoJitterTiming(
    Clock* clock,
    const FieldTrialsView& field_trials)
    : clock_(clock),
      ts_extrapolator_(clock_->CurrentTime(), field_trials),
      jitter_estimator_(clock_, field_trials),
      update_on_every_frame_(
          field_trials.IsDisabled("WebRTC-IncomingTimestampOnMarkerBitOnly")) {}

DefaultVideoJitterTiming::~DefaultVideoJitterTiming() = default;

void DefaultVideoJitterTiming::Reset() {
  ts_extrapolator_.Reset(clock_->CurrentTime());
  jitter_estimator_.Reset();
}

void DefaultVideoJitterTiming::OnCompleteFrame(const FrameInfo& info) {
  if (!info.was_retransmitted &&
      (update_on_every_frame_ || info.last_spatial_layer)) {
    ts_extrapolator_.Update(info.time, info.rtp_timestamp);
  }
}

std::optional<Timestamp> DefaultVideoJitterTiming::LocalTime(
    uint32_t rtp_timestamp) const {
  return ts_extrapolator_.ExtrapolateLocalTime(rtp_timestamp);
}

std::optional<TimeDelta> DefaultVideoJitterTiming::OnDecodableTemporalUnit(
    const TemporalUnitInfo& info) {
  if (info.was_retransmitted) {
    jitter_estimator_.FrameNacked();
    return std::nullopt;
  }
  std::optional<TimeDelta> inter_frame_delay_variation =
      ifdv_calculator_.Calculate(info.rtp_timestamp, info.time);
  if (inter_frame_delay_variation) {
    jitter_estimator_.UpdateEstimate(*inter_frame_delay_variation, info.size);
  }
  return jitter_estimator_.GetEstimate();
}

void DefaultVideoJitterTiming::OnNetworkUpdate(const NetworkInfo& info) {
  jitter_estimator_.UpdateRtt(info.rtt);
}

}  // namespace webrtc
