/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_SCREAM_SCREAM_NETWORK_CONTROLLER_H_
#define MODULES_CONGESTION_CONTROLLER_SCREAM_SCREAM_NETWORK_CONTROLLER_H_

#include <optional>

#include "api/environment/environment.h"
#include "api/transport/network_control.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/congestion_controller/scream/scream_v2.h"
#include "modules/congestion_controller/scream/scream_v2_parameters.h"

namespace webrtc {

class ScreamNetworkController : public NetworkControllerInterface {
 public:
  explicit ScreamNetworkController(NetworkControllerConfig config);
  ~ScreamNetworkController() override = default;

  NetworkControlUpdate OnNetworkAvailability(NetworkAvailability msg) override;
  NetworkControlUpdate OnNetworkRouteChange(NetworkRouteChange msg) override;
  NetworkControlUpdate OnProcessInterval(ProcessInterval msg) override;
  NetworkControlUpdate OnRemoteBitrateReport(RemoteBitrateReport msg) override;
  NetworkControlUpdate OnRoundTripTimeUpdate(RoundTripTimeUpdate msg) override;
  NetworkControlUpdate OnSentPacket(SentPacket msg) override;
  NetworkControlUpdate OnReceivedPacket(ReceivedPacket) override;
  NetworkControlUpdate OnStreamsConfig(StreamsConfig msg) override;
  NetworkControlUpdate OnTargetRateConstraints(
      TargetRateConstraints msg) override;
  NetworkControlUpdate OnTransportLossReport(TransportLossReport msg) override;
  NetworkControlUpdate OnTransportPacketsFeedback(
      TransportPacketsFeedback msg) override;
  NetworkControlUpdate OnNetworkStateEstimate(NetworkStateEstimate) override;

  bool SupportsEcnAdaptation() const override { return true; }

 private:
  void UpdateScreamTargetBitrateConstraints();
  NetworkControlUpdate CreateFirstUpdate(Timestamp now);
  NetworkControlUpdate CreateUpdate(Timestamp now);
  std::optional<PacerConfig> MaybeCreatePacerConfig(Timestamp now);
  // Calculates a ratio in [0.0, 1.0] indicating how much the video encoder
  // should reduce its target bitrate (pushback) due to network or pacer queue
  // build-up. Returns 1.0 if data in flight exceeds max_data_in_flight.
  // Otherwise, if pacer queue delay exceeds min_pacing_delay_for_pushback,
  // the ratio scales linearly up to 1.0 at max_pacing_delay_for_pushback.
  double CalculateCwndReduceRatio() const;

  Environment env_;
  const ScreamV2Parameters params_;
  const TimeDelta default_pacing_window_;
  const bool allow_initial_bwe_before_media_ = false;
  bool first_update_created_ = false;
  bool network_available_ = false;
  TimeDelta current_pacing_window_;
  std::optional<ScreamV2> scream_;
  DataRate min_target_rate_;
  DataRate max_target_rate_;
  DataRate starting_rate_;
  std::optional<DataRate> remote_bitrate_report_;
  StreamsConfig streams_config_;
  DataRate max_seen_total_allocated_bitrate_ = DataRate::Zero();
  Timestamp initial_bwe_probe_end_time_ = Timestamp::MinusInfinity();
  Timestamp padding_interval_end_time_ = Timestamp::MinusInfinity();
  DataSize pacer_queue_size_ = DataSize::Zero();
  DataSize data_in_flight_ = DataSize::Zero();

  // Values last reported in a NetworkControlUpdate. Used for finding out if an
  // update needs to be reported.
  DataRate reported_target_rate_;
  DataRate reported_padding_rate_;
  DataRate reported_pacing_rate_;
  bool reported_is_bandwidth_limited_ = true;
  double reported_cwnd_reduce_ratio_ = 0.0;
};

}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_SCREAM_SCREAM_NETWORK_CONTROLLER_H_
