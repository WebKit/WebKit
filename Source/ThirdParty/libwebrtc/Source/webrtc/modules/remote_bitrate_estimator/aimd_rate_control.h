/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_AIMD_RATE_CONTROL_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_AIMD_RATE_CONTROL_H_

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/remote_bitrate_estimator/include/bwe_defines.h"

namespace webrtc {

// A rate control implementation based on additive increases of
// bitrate when no over-use is detected and multiplicative decreases when
// over-uses are detected. When we think the available bandwidth has changes or
// is unknown, we will switch to a "slow-start mode" where we increase
// multiplicatively.
class AimdRateControl {
 public:
  AimdRateControl();
  virtual ~AimdRateControl() {}

  // Returns true if there is a valid estimate of the incoming bitrate, false
  // otherwise.
  bool ValidEstimate() const;
  void SetMinBitrate(int min_bitrate_bps);
  int64_t GetFeedbackInterval() const;
  // Returns true if the bitrate estimate hasn't been changed for more than
  // an RTT, or if the incoming_bitrate is more than 5% above the current
  // estimate. Should be used to decide if we should reduce the rate further
  // when over-using.
  bool TimeToReduceFurther(int64_t time_now,
                           uint32_t incoming_bitrate_bps) const;
  uint32_t LatestEstimate() const;
  uint32_t UpdateBandwidthEstimate(int64_t now_ms);
  void SetRtt(int64_t rtt);
  void Update(const RateControlInput* input, int64_t now_ms);
  void SetEstimate(int bitrate_bps, int64_t now_ms);

 private:
  // Update the target bitrate according based on, among other things,
  // the current rate control state, the current target bitrate and the incoming
  // bitrate. When in the "increase" state the bitrate will be increased either
  // additively or multiplicatively depending on the rate control region. When
  // in the "decrease" state the bitrate will be decreased to slightly below the
  // incoming bitrate. When in the "hold" state the bitrate will be kept
  // constant to allow built up queues to drain.
  uint32_t ChangeBitrate(uint32_t current_bit_rate,
                         uint32_t incoming_bit_rate,
                         int64_t now_ms);
  uint32_t MultiplicativeRateIncrease(int64_t now_ms, int64_t last_ms,
                                      uint32_t current_bitrate_bps) const;
  uint32_t AdditiveRateIncrease(int64_t now_ms, int64_t last_ms,
                                int64_t response_time_ms) const;
  void UpdateChangePeriod(int64_t now_ms);
  void UpdateMaxBitRateEstimate(float incoming_bit_rate_kbps);
  void ChangeState(const RateControlInput& input, int64_t now_ms);
  void ChangeState(RateControlState new_state);
  void ChangeRegion(RateControlRegion region);

  uint32_t min_configured_bitrate_bps_;
  uint32_t max_configured_bitrate_bps_;
  uint32_t current_bitrate_bps_;
  float avg_max_bitrate_kbps_;
  float var_max_bitrate_kbps_;
  RateControlState rate_control_state_;
  RateControlRegion rate_control_region_;
  int64_t time_last_bitrate_change_;
  RateControlInput current_input_;
  bool updated_;
  int64_t time_first_incoming_estimate_;
  bool bitrate_is_initialized_;
  float beta_;
  int64_t rtt_;
  bool in_experiment_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_AIMD_RATE_CONTROL_H_
