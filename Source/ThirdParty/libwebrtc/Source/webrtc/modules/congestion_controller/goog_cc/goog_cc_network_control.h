/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_GOOG_CC_GOOG_CC_NETWORK_CONTROL_H_
#define MODULES_CONGESTION_CONTROLLER_GOOG_CC_GOOG_CC_NETWORK_CONTROL_H_

#include <stdint.h>
#include <deque>
#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "api/transport/network_control.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "modules/bitrate_controller/send_side_bandwidth_estimation.h"
#include "modules/congestion_controller/goog_cc/acknowledged_bitrate_estimator.h"
#include "modules/congestion_controller/goog_cc/alr_detector.h"
#include "modules/congestion_controller/goog_cc/delay_based_bwe.h"
#include "modules/congestion_controller/goog_cc/probe_controller.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

class GoogCcNetworkController : public NetworkControllerInterface {
 public:
  GoogCcNetworkController(RtcEventLog* event_log,
                          NetworkControllerConfig config,
                          bool feedback_only);
  ~GoogCcNetworkController() override;

  // NetworkControllerInterface
  NetworkControlUpdate OnNetworkAvailability(NetworkAvailability msg) override;
  NetworkControlUpdate OnNetworkRouteChange(NetworkRouteChange msg) override;
  NetworkControlUpdate OnProcessInterval(ProcessInterval msg) override;
  NetworkControlUpdate OnRemoteBitrateReport(RemoteBitrateReport msg) override;
  NetworkControlUpdate OnRoundTripTimeUpdate(RoundTripTimeUpdate msg) override;
  NetworkControlUpdate OnSentPacket(SentPacket msg) override;
  NetworkControlUpdate OnStreamsConfig(StreamsConfig msg) override;
  NetworkControlUpdate OnTargetRateConstraints(
      TargetRateConstraints msg) override;
  NetworkControlUpdate OnTransportLossReport(TransportLossReport msg) override;
  NetworkControlUpdate OnTransportPacketsFeedback(
      TransportPacketsFeedback msg) override;

  NetworkControlUpdate GetNetworkState(Timestamp at_time) const;

 private:
  std::vector<ProbeClusterConfig> UpdateBitrateConstraints(
      TargetRateConstraints constraints,
      absl::optional<DataRate> starting_rate);
  absl::optional<DataSize> MaybeUpdateCongestionWindow();
  void MaybeTriggerOnNetworkChanged(NetworkControlUpdate* update,
                                    Timestamp at_time);
  bool GetNetworkParameters(int32_t* estimated_bitrate_bps,
                            uint8_t* fraction_loss,
                            int64_t* rtt_ms,
                            Timestamp at_time);
  PacerConfig GetPacingRates(Timestamp at_time) const;

  RtcEventLog* const event_log_;
  const bool packet_feedback_only_;

  const std::unique_ptr<ProbeController> probe_controller_;

  std::unique_ptr<SendSideBandwidthEstimation> bandwidth_estimation_;
  std::unique_ptr<AlrDetector> alr_detector_;
  std::unique_ptr<DelayBasedBwe> delay_based_bwe_;
  std::unique_ptr<AcknowledgedBitrateEstimator> acknowledged_bitrate_estimator_;

  absl::optional<NetworkControllerConfig> initial_config_;

  Timestamp next_loss_update_ = Timestamp::ms(0);
  int lost_packets_since_last_loss_update_ = 0;
  int expected_packets_since_last_loss_update_ = 0;

  std::deque<int64_t> feedback_max_rtts_;
  absl::optional<int64_t> min_feedback_max_rtt_ms_;

  DataRate last_bandwidth_;
  absl::optional<TargetTransferRate> last_target_rate_;

  int32_t last_estimated_bitrate_bps_ = 0;
  uint8_t last_estimated_fraction_loss_ = 0;
  int64_t last_estimated_rtt_ms_ = 0;

  double pacing_factor_;
  DataRate min_pacing_rate_;
  DataRate max_padding_rate_;
  DataRate max_total_allocated_bitrate_;

  bool in_cwnd_experiment_;
  int64_t accepted_queue_ms_;
  bool previously_in_alr = false;

  absl::optional<DataSize> current_data_window_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(GoogCcNetworkController);
};

}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_GOOG_CC_GOOG_CC_NETWORK_CONTROL_H_
