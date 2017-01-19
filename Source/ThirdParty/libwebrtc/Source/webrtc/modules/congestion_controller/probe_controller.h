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

  void SetBitrates(int min_bitrate_bps,
                   int start_bitrate_bps,
                   int max_bitrate_bps);
  void SetEstimatedBitrate(int bitrate_bps);

 private:
  void InitiateProbing(std::initializer_list<int> bitrates_to_probe,
                       int min_bitrate_to_probe_further_bps)
      EXCLUSIVE_LOCKS_REQUIRED(critsect_);
  enum class State {
    // Initial state where no probing has been triggered yet.
    kInit,
    // Waiting for probing results to continue further probing.
    kWaitingForProbingResult,
    // Probing is complete.
    kProbingComplete,
  };
  rtc::CriticalSection critsect_;
  PacedSender* const pacer_;
  Clock* const clock_;
  State state_ GUARDED_BY(critsect_);
  int min_bitrate_to_probe_further_bps_ GUARDED_BY(critsect_);
  int64_t time_last_probing_initiated_ms_ GUARDED_BY(critsect_);
  int estimated_bitrate_bps_ GUARDED_BY(critsect_);
  int max_bitrate_bps_ GUARDED_BY(critsect_);
  int64_t last_alr_probing_time_ GUARDED_BY(critsect_);

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(ProbeController);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_CONGESTION_CONTROLLER_PROBE_CONTROLLER_H_
