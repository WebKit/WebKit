/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_RTP_CONTROL_HANDLER_H_
#define MODULES_CONGESTION_CONTROLLER_RTP_CONTROL_HANDLER_H_

#include <algorithm>
#include <memory>

#include "api/transport/network_control.h"
#include "modules/congestion_controller/include/network_changed_observer.h"
#include "modules/pacing/paced_sender.h"
#include "rtc_base/sequenced_task_checker.h"

namespace webrtc {
// This is used to observe the network controller state and route calls to
// the proper handler. It also keeps cached values for safe asynchronous use.
// This makes sure that things running on the worker queue can't access state
// in SendSideCongestionController, which would risk causing data race on
// destruction unless members are properly ordered.
class CongestionControlHandler {
 public:
  CongestionControlHandler(NetworkChangedObserver* observer,
                           PacedSender* pacer);
  ~CongestionControlHandler();

  void PostUpdates(NetworkControlUpdate update);

  void OnNetworkAvailability(NetworkAvailability msg);
  void OnOutstandingData(DataSize in_flight_data);
  void OnPacerQueueUpdate(TimeDelta expected_queue_time);

  absl::optional<TargetTransferRate> last_transfer_rate();

 private:
  void SetPacerState(bool paused);
  void OnNetworkInvalidation();
  bool IsSendQueueFull() const;
  bool HasNetworkParametersToReportChanged(int64_t bitrate_bps,
                                           float loss_rate_ratio,
                                           TimeDelta rtt);

  bool HasNetworkParametersToReportChanged(int64_t bitrate_bps,
                                           uint8_t fraction_loss,
                                           int64_t rtt);
  NetworkChangedObserver* observer_ = nullptr;
  PacedSender* const pacer_;

  absl::optional<TargetTransferRate> current_target_rate_msg_;
  bool network_available_ = true;
  bool pacer_paused_ = false;
  int64_t last_reported_target_bitrate_bps_ = 0;
  uint8_t last_reported_fraction_loss_ = 0;
  int64_t last_reported_rtt_ms_ = 0;
  const bool pacer_pushback_experiment_;
  const bool disable_pacer_emergency_stop_;
  uint32_t min_pushback_target_bitrate_bps_;
  int64_t pacer_expected_queue_ms_ = 0;
  double encoding_rate_ratio_ = 1.0;

  rtc::SequencedTaskChecker sequenced_checker_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(CongestionControlHandler);
};
}  // namespace webrtc
#endif  // MODULES_CONGESTION_CONTROLLER_RTP_CONTROL_HANDLER_H_
