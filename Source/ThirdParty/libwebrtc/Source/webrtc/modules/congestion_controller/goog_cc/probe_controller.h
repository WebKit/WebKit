/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_GOOG_CC_PROBE_CONTROLLER_H_
#define MODULES_CONGESTION_CONTROLLER_GOOG_CC_PROBE_CONTROLLER_H_

#include <stdint.h>

#include <initializer_list>
#include <vector>

#include "absl/types/optional.h"
#include "api/transport/network_control.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/system/unused.h"

namespace webrtc {

class Clock;

// This class controls initiation of probing to estimate initial channel
// capacity. There is also support for probing during a session when max
// bitrate is adjusted by an application.
class ProbeController {
 public:
  ProbeController();
  ~ProbeController();

  RTC_WARN_UNUSED_RESULT std::vector<ProbeClusterConfig> SetBitrates(
      int64_t min_bitrate_bps,
      int64_t start_bitrate_bps,
      int64_t max_bitrate_bps,
      int64_t at_time_ms);

  // The total bitrate, as opposed to the max bitrate, is the sum of the
  // configured bitrates for all active streams.
  RTC_WARN_UNUSED_RESULT std::vector<ProbeClusterConfig>
  OnMaxTotalAllocatedBitrate(int64_t max_total_allocated_bitrate,
                             int64_t at_time_ms);

  RTC_WARN_UNUSED_RESULT std::vector<ProbeClusterConfig> OnNetworkAvailability(
      NetworkAvailability msg);

  RTC_WARN_UNUSED_RESULT std::vector<ProbeClusterConfig> SetEstimatedBitrate(
      int64_t bitrate_bps,
      int64_t at_time_ms);

  void EnablePeriodicAlrProbing(bool enable);

  void SetAlrStartTimeMs(absl::optional<int64_t> alr_start_time);
  void SetAlrEndedTimeMs(int64_t alr_end_time);

  RTC_WARN_UNUSED_RESULT std::vector<ProbeClusterConfig> RequestProbe(
      int64_t at_time_ms);

  // Resets the ProbeController to a state equivalent to as if it was just
  // created EXCEPT for |enable_periodic_alr_probing_|.
  void Reset(int64_t at_time_ms);

  RTC_WARN_UNUSED_RESULT std::vector<ProbeClusterConfig> Process(
      int64_t at_time_ms);

 private:
  enum class State {
    // Initial state where no probing has been triggered yet.
    kInit,
    // Waiting for probing results to continue further probing.
    kWaitingForProbingResult,
    // Probing is complete.
    kProbingComplete,
  };

  RTC_WARN_UNUSED_RESULT std::vector<ProbeClusterConfig>
  InitiateExponentialProbing(int64_t at_time_ms);
  RTC_WARN_UNUSED_RESULT std::vector<ProbeClusterConfig> InitiateProbing(
      int64_t now_ms,
      std::initializer_list<int64_t> bitrates_to_probe,
      bool probe_further);

  bool network_available_;
  State state_;
  int64_t min_bitrate_to_probe_further_bps_;
  int64_t time_last_probing_initiated_ms_;
  int64_t estimated_bitrate_bps_;
  int64_t start_bitrate_bps_;
  int64_t max_bitrate_bps_;
  int64_t last_bwe_drop_probing_time_ms_;
  absl::optional<int64_t> alr_start_time_ms_;
  absl::optional<int64_t> alr_end_time_ms_;
  bool enable_periodic_alr_probing_;
  int64_t time_of_last_large_drop_ms_;
  int64_t bitrate_before_last_large_drop_bps_;
  int64_t max_total_allocated_bitrate_;

  bool in_rapid_recovery_experiment_;
  // For WebRTC.BWE.MidCallProbing.* metric.
  bool mid_call_probing_waiting_for_result_;
  int64_t mid_call_probing_bitrate_bps_;
  int64_t mid_call_probing_succcess_threshold_;

  RTC_DISALLOW_COPY_AND_ASSIGN(ProbeController);
};

}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_GOOG_CC_PROBE_CONTROLLER_H_
