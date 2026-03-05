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
  NetworkControlUpdate CreateUpdate(Timestamp now,
                                    DataRate target_rate,
                                    TimeDelta rtt);
  PacerConfig CreatePacerConfig(DataRate target_rate);

  Environment env_;
  const ScreamV2Parameters params_;
  const TimeDelta default_pacing_window_;
  TimeDelta current_pacing_window_;
  std::optional<ScreamV2> scream_;
  TargetRateConstraints target_rate_constraints_;
  StreamsConfig streams_config_;

  Timestamp last_padding_interval_started_;
};

}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_SCREAM_SCREAM_NETWORK_CONTROLLER_H_
