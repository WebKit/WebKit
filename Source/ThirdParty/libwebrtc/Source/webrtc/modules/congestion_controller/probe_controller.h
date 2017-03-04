/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_CONGESTION_CONTROLLER_PROBE_CONTROLLER_H_
#define WEBRTC_MODULES_CONGESTION_CONTROLLER_PROBE_CONTROLLER_H_

#include <initializer_list>

#include "webrtc/base/criticalsection.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/pacing/paced_sender.h"

namespace webrtc {

class Clock;

// This class controls initiation of probing to estimate initial channel
// capacity. There is also support for probing during a session when max
// bitrate is adjusted by an application.
class ProbeController {
 public:
  ProbeController(PacedSender* pacer, Clock* clock);

  void SetBitrates(int64_t min_bitrate_bps,
                   int64_t start_bitrate_bps,
                   int64_t max_bitrate_bps);

  void OnNetworkStateChanged(NetworkState state);

  void SetEstimatedBitrate(int64_t bitrate_bps);

  void EnablePeriodicAlrProbing(bool enable);
  void Process();

 private:
  enum class State {
    // Initial state where no probing has been triggered yet.
    kInit,
    // Waiting for probing results to continue further probing.
    kWaitingForProbingResult,
    // Probing is complete.
    kProbingComplete,
  };

  void InitiateExponentialProbing() EXCLUSIVE_LOCKS_REQUIRED(critsect_);
  void InitiateProbing(int64_t now_ms,
                       std::initializer_list<int64_t> bitrates_to_probe,
                       bool probe_further) EXCLUSIVE_LOCKS_REQUIRED(critsect_);

  rtc::CriticalSection critsect_;
  PacedSender* const pacer_;
  Clock* const clock_;
  NetworkState network_state_ GUARDED_BY(critsect_);
  State state_ GUARDED_BY(critsect_);
  int64_t min_bitrate_to_probe_further_bps_ GUARDED_BY(critsect_);
  int64_t time_last_probing_initiated_ms_ GUARDED_BY(critsect_);
  int64_t estimated_bitrate_bps_ GUARDED_BY(critsect_);
  int64_t start_bitrate_bps_ GUARDED_BY(critsect_);
  int64_t max_bitrate_bps_ GUARDED_BY(critsect_);
  int64_t last_alr_probing_time_ GUARDED_BY(critsect_);
  bool enable_periodic_alr_probing_ GUARDED_BY(critsect_);

  // For WebRTC.BWE.MidCallProbing.* metric.
  bool mid_call_probing_waiting_for_result_ GUARDED_BY(&critsect_);
  int64_t mid_call_probing_bitrate_bps_ GUARDED_BY(&critsect_);
  int64_t mid_call_probing_succcess_threshold_ GUARDED_BY(&critsect_);

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(ProbeController);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_CONGESTION_CONTROLLER_PROBE_CONTROLLER_H_
