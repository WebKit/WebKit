/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_TIMING_DEFAULT_VIDEO_JITTER_TIMING_H_
#define VIDEO_TIMING_DEFAULT_VIDEO_JITTER_TIMING_H_

#include <cstdint>
#include <optional>
#include <span>

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

class DefaultVideoJitterTiming : public VideoJitterTimingInterface {
 public:
  explicit DefaultVideoJitterTiming(const Environment& env);
  // TODO(b/493549134): Remove once no longer used.
  DefaultVideoJitterTiming(Clock* clock, const FieldTrialsView& field_trials);
  ~DefaultVideoJitterTiming() override;

  // Resets members to its initial state.
  void Reset() override;

  // Updates the extrapolator with information of a complete frame.
  void OnCompleteFrame(const FrameInfo& info) override;

  // Returns the extrapolated local time for a given RTP timestamp.
  std::optional<Timestamp> LocalTime(uint32_t rtp_timestamp) const override;

  std::optional<TimeDelta> OnContinuousTemporalUnits(
      std::span<const uint32_t> rtp_timestamps,
      Timestamp now) override {
    return std::nullopt;
  }

  // Updates the jitter estimator with information of a decodable temporal unit.
  // Returns the current jitter delay (or nullopt if not available).
  std::optional<TimeDelta> OnDecodableTemporalUnit(
      const TemporalUnitInfo& info) override;

  // Updates the jitter estimator with network information.
  void OnNetworkUpdate(const NetworkInfo& info) override;

 private:
  Clock* const clock_;
  TimestampExtrapolator ts_extrapolator_;
  JitterEstimator jitter_estimator_;
  InterFrameDelayVariationCalculator ifdv_calculator_;
  const bool update_on_every_frame_;
};

}  // namespace webrtc

#endif  // VIDEO_TIMING_DEFAULT_VIDEO_JITTER_TIMING_H_
