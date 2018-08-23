/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_RTP_PACER_CONTROLLER_H_
#define MODULES_CONGESTION_CONTROLLER_RTP_PACER_CONTROLLER_H_

#include <memory>

#include "api/transport/network_types.h"
#include "modules/pacing/paced_sender.h"
#include "rtc_base/sequenced_task_checker.h"

namespace webrtc {
class Clock;

// Wrapper class to control pacer using task queues. Note that this class is
// only designed to be used from a single task queue and has no built in
// concurrency safety.
// TODO(srte): Integrate this interface directly into PacedSender.
class PacerController {
 public:
  explicit PacerController(PacedSender* pacer);
  ~PacerController();
  void OnCongestionWindow(DataSize msg);
  void OnNetworkAvailability(NetworkAvailability msg);
  void OnNetworkRouteChange(NetworkRouteChange msg);
  void OnOutstandingData(DataSize in_flight_data);
  void OnPacerConfig(PacerConfig msg);
  void OnProbeClusterConfig(ProbeClusterConfig msg);

 private:
  void SetPacerState(bool paused);
  PacedSender* const pacer_;

  absl::optional<PacerConfig> current_pacer_config_;
  bool pacer_paused_ = false;
  bool network_available_ = true;

  rtc::SequencedTaskChecker sequenced_checker_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(PacerController);
};
}  // namespace webrtc
#endif  // MODULES_CONGESTION_CONTROLLER_RTP_PACER_CONTROLLER_H_
