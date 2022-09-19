/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_TIMING_JITTER_ESTIMATOR_H_
#define MODULES_VIDEO_CODING_TIMING_JITTER_ESTIMATOR_H_

#include "absl/types/optional.h"
#include "api/field_trials_view.h"
#include "api/units/data_size.h"
#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/video_coding/timing/frame_delay_delta_kalman_filter.h"
#include "modules/video_coding/timing/rtt_filter.h"
#include "rtc_base/rolling_accumulator.h"

namespace webrtc {

class Clock;

class JitterEstimator {
 public:
  explicit JitterEstimator(Clock* clock, const FieldTrialsView& field_trials);
  ~JitterEstimator();
  JitterEstimator(const JitterEstimator&) = delete;
  JitterEstimator& operator=(const JitterEstimator&) = delete;

  // Resets the estimate to the initial state.
  void Reset();

  // Updates the jitter estimate with the new data.
  //
  // Input:
  //          - frame_delay      : Delay-delta calculated by UTILDelayEstimate.
  //          - frame_size       : Frame size of the current frame.
  void UpdateEstimate(TimeDelta frame_delay, DataSize frame_size);

  // Returns the current jitter estimate and adds an RTT dependent term in cases
  // of retransmission.
  //  Input:
  //          - rtt_multiplier   : RTT param multiplier (when applicable).
  //          - rtt_mult_add_cap : Multiplier cap from the RTTMultExperiment.
  //
  // Return value              : Jitter estimate.
  TimeDelta GetJitterEstimate(double rtt_multiplier,
                              absl::optional<TimeDelta> rtt_mult_add_cap);

  // Updates the nack counter.
  void FrameNacked();

  // Updates the RTT filter.
  //
  // Input:
  //          - rtt          : Round trip time.
  void UpdateRtt(TimeDelta rtt);

  // A constant describing the delay from the jitter buffer to the delay on the
  // receiving side which is not accounted for by the jitter buffer nor the
  // decoding delay estimate.
  static constexpr TimeDelta OPERATING_SYSTEM_JITTER = TimeDelta::Millis(10);

 private:
  double var_noise_;  // Variance of the time-deviation from the line

  // Updates the random jitter estimate, i.e. the variance of the time
  // deviations from the line given by the Kalman filter.
  //
  // Input:
  //          - d_dT              : The deviation from the kalman estimate.
  void EstimateRandomJitter(double d_dT);

  double NoiseThreshold() const;

  // Calculates the current jitter estimate.
  //
  // Return value                 : The current jitter estimate.
  TimeDelta CalculateEstimate();

  // Post process the calculated estimate.
  void PostProcessEstimate();

  Frequency GetFrameRate() const;

  // Filters the {frame_delay_delta, frame_size_delta} measurements through
  // a linear Kalman filter.
  FrameDelayDeltaKalmanFilter kalman_filter_;

  static constexpr DataSize kDefaultAvgAndMaxFrameSize = DataSize::Bytes(500);
  DataSize avg_frame_size_ = kDefaultAvgAndMaxFrameSize;  // Average frame size
  double var_frame_size_;  // Frame size variance. Unit is bytes^2.
  // Largest frame size received (descending with a factor kPsi)
  DataSize max_frame_size_ = kDefaultAvgAndMaxFrameSize;
  DataSize frame_size_sum_ = DataSize::Zero();
  uint32_t frame_size_count_;

  absl::optional<Timestamp> last_update_time_;
  // The previously returned jitter estimate
  absl::optional<TimeDelta> prev_estimate_;
  // Frame size of the previous frame
  absl::optional<DataSize> prev_frame_size_;
  // Average of the random jitter
  double avg_noise_;
  uint32_t alpha_count_;
  // The filtered sum of jitter estimates
  TimeDelta filter_jitter_estimate_ = TimeDelta::Zero();

  uint32_t startup_count_;
  // Time when the latest nack was seen
  Timestamp latest_nack_ = Timestamp::Zero();
  // Keeps track of the number of nacks received, but never goes above
  // kNackLimit.
  uint32_t nack_count_;
  RttFilter rtt_filter_;

  // Tracks frame rates in microseconds.
  rtc::RollingAccumulator<uint64_t> fps_counter_;
  Clock* clock_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_TIMING_JITTER_ESTIMATOR_H_
