/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_REMOTE_BITRATE_ESTIMATOR_AIMD_RATE_CONTROL_H_
#define MODULES_REMOTE_BITRATE_ESTIMATOR_AIMD_RATE_CONTROL_H_

#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

// A rate control implementation based on additive increases of
// bitrate when no over-use is detected and multiplicative decreases when
// over-uses are detected. When we think the available bandwidth has changes or
// is unknown, we will switch to a "slow-start mode" where we increase
// multiplicatively.
class AimdRateControl {
 public:
  AimdRateControl();
  ~AimdRateControl();

  // Returns true if the target bitrate has been initialized. This happens
  // either if it has been explicitly set via SetStartBitrate/SetEstimate, or if
  // we have measured a throughput.
  bool ValidEstimate() const;
  void SetStartBitrate(int start_bitrate_bps);
  void SetMinBitrate(int min_bitrate_bps);
  int64_t GetFeedbackInterval() const;

  // Returns true if the bitrate estimate hasn't been changed for more than
  // an RTT, or if the estimated_throughput is less than half of the current
  // estimate. Should be used to decide if we should reduce the rate further
  // when over-using.
  bool TimeToReduceFurther(int64_t now_ms,
                           uint32_t estimated_throughput_bps) const;
  // As above. To be used if overusing before we have measured a throughput.
  bool InitialTimeToReduceFurther(int64_t now_ms) const;

  uint32_t LatestEstimate() const;
  void SetRtt(int64_t rtt);
  uint32_t Update(const RateControlInput* input, int64_t now_ms);
  void SetEstimate(int bitrate_bps, int64_t now_ms);

  // Returns the increase rate when used bandwidth is near the link capacity.
  int GetNearMaxIncreaseRateBps() const;
  // Returns the expected time between overuse signals (assuming steady state).
  int GetExpectedBandwidthPeriodMs() const;

 private:
  // Update the target bitrate based on, among other things, the current rate
  // control state, the current target bitrate and the estimated throughput.
  // When in the "increase" state the bitrate will be increased either
  // additively or multiplicatively depending on the rate control region. When
  // in the "decrease" state the bitrate will be decreased to slightly below the
  // current throughput. When in the "hold" state the bitrate will be kept
  // constant to allow built up queues to drain.
  uint32_t ChangeBitrate(uint32_t current_bitrate,
                         const RateControlInput& input,
                         int64_t now_ms);
  // Clamps new_bitrate_bps to within the configured min bitrate and a linear
  // function of the throughput, so that the new bitrate can't grow too
  // large compared to the bitrate actually being received by the other end.
  uint32_t ClampBitrate(uint32_t new_bitrate_bps,
                        uint32_t estimated_throughput_bps) const;
  uint32_t MultiplicativeRateIncrease(int64_t now_ms,
                                      int64_t last_ms,
                                      uint32_t current_bitrate_bps) const;
  uint32_t AdditiveRateIncrease(int64_t now_ms, int64_t last_ms) const;
  void UpdateChangePeriod(int64_t now_ms);
  void UpdateMaxThroughputEstimate(float estimated_throughput_kbps);
  void ChangeState(const RateControlInput& input, int64_t now_ms);
  void ChangeRegion(RateControlRegion region);

  uint32_t min_configured_bitrate_bps_;
  uint32_t max_configured_bitrate_bps_;
  uint32_t current_bitrate_bps_;
  uint32_t latest_estimated_throughput_bps_;
  float avg_max_bitrate_kbps_;
  float var_max_bitrate_kbps_;
  RateControlState rate_control_state_;
  RateControlRegion rate_control_region_;
  int64_t time_last_bitrate_change_;
  int64_t time_last_bitrate_decrease_;
  int64_t time_first_throughput_estimate_;
  bool bitrate_is_initialized_;
  float beta_;
  int64_t rtt_;
  const bool in_experiment_;
  const bool smoothing_experiment_;
  const bool in_initial_backoff_interval_experiment_;
  int64_t initial_backoff_interval_ms_;
  absl::optional<int> last_decrease_;
};
}  // namespace webrtc

#endif  // MODULES_REMOTE_BITRATE_ESTIMATOR_AIMD_RATE_CONTROL_H_
