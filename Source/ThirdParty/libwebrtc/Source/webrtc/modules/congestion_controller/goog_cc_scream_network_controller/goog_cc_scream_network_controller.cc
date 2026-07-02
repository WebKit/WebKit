/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/goog_cc_scream_network_controller/goog_cc_scream_network_controller.h"

#include <memory>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "api/environment/environment.h"
#include "api/transport/network_control.h"
#include "api/transport/network_types.h"
#include "modules/congestion_controller/goog_cc/goog_cc_network_control.h"
#include "modules/congestion_controller/scream/scream_network_controller.h"
#include "rtc_base/logging.h"

namespace webrtc {

NetworkControlUpdate GoogCcScreamNetworkController::MaybeRunOnAllControllers(
    absl::AnyInvocable<NetworkControlUpdate(NetworkControllerInterface&)>
        update) {
  if (scream_in_use_) {
    return update(*scream_);
  }
  if (mode_ == Mode::kScreamAfterCe) {
    update(*scream_);
  }
  return update(*goog_cc_);
}

GoogCcScreamNetworkController::GoogCcScreamNetworkController(
    NetworkControllerConfig config,
    GoogCcConfig goog_cc_config,
    Mode mode)
    : env_(config.env), mode_(mode) {
  if (mode_ != Mode::kGoogCcWithEct1) {
    scream_ = std::make_unique<ScreamNetworkController>(config);
    scream_in_use_ = false;
  }
  goog_cc_ = std::make_unique<GoogCcNetworkController>(
      config, std::move(goog_cc_config));
}

GoogCcScreamNetworkController::~GoogCcScreamNetworkController() = default;

NetworkControlUpdate GoogCcScreamNetworkController::OnNetworkAvailability(
    NetworkAvailability msg) {
  return MaybeRunOnAllControllers(
      [&msg](webrtc::NetworkControllerInterface& controller) {
        return controller.OnNetworkAvailability(msg);
      });
}

NetworkControlUpdate GoogCcScreamNetworkController::OnNetworkRouteChange(
    NetworkRouteChange msg) {
  return MaybeRunOnAllControllers(
      [&msg](webrtc::NetworkControllerInterface& controller) {
        return controller.OnNetworkRouteChange(msg);
      });
}

NetworkControlUpdate GoogCcScreamNetworkController::OnProcessInterval(
    ProcessInterval msg) {
  return MaybeRunOnAllControllers(
      [&msg](webrtc::NetworkControllerInterface& controller) {
        return controller.OnProcessInterval(msg);
      });
}

NetworkControlUpdate GoogCcScreamNetworkController::OnRemoteBitrateReport(
    RemoteBitrateReport msg) {
  return MaybeRunOnAllControllers(
      [&msg](webrtc::NetworkControllerInterface& controller) {
        return controller.OnRemoteBitrateReport(msg);
      });
}

NetworkControlUpdate GoogCcScreamNetworkController::OnRoundTripTimeUpdate(
    RoundTripTimeUpdate msg) {
  return MaybeRunOnAllControllers(
      [&msg](webrtc::NetworkControllerInterface& controller) {
        return controller.OnRoundTripTimeUpdate(msg);
      });
}

NetworkControlUpdate GoogCcScreamNetworkController::OnSentPacket(
    SentPacket msg) {
  return MaybeRunOnAllControllers(
      [&msg](webrtc::NetworkControllerInterface& controller) {
        return controller.OnSentPacket(msg);
      });
}

NetworkControlUpdate GoogCcScreamNetworkController::OnReceivedPacket(
    ReceivedPacket msg) {
  return MaybeRunOnAllControllers(
      [&msg](webrtc::NetworkControllerInterface& controller) {
        return controller.OnReceivedPacket(msg);
      });
}

NetworkControlUpdate GoogCcScreamNetworkController::OnStreamsConfig(
    StreamsConfig msg) {
  return MaybeRunOnAllControllers(
      [&msg](webrtc::NetworkControllerInterface& controller) {
        return controller.OnStreamsConfig(msg);
      });
}

NetworkControlUpdate GoogCcScreamNetworkController::OnTargetRateConstraints(
    TargetRateConstraints msg) {
  return MaybeRunOnAllControllers(
      [&msg](webrtc::NetworkControllerInterface& controller) {
        return controller.OnTargetRateConstraints(msg);
      });
}

NetworkControlUpdate GoogCcScreamNetworkController::OnTransportLossReport(
    TransportLossReport msg) {
  return MaybeRunOnAllControllers(
      [&msg](webrtc::NetworkControllerInterface& controller) {
        return controller.OnTransportLossReport(msg);
      });
}

NetworkControlUpdate GoogCcScreamNetworkController::OnTransportPacketsFeedback(
    TransportPacketsFeedback msg) {
  if (msg.HasPacketWithEcnCe()) {
    ecn_ce_seen_ = true;
  }
  if (mode_ == Mode::kScreamAfterCe && !scream_in_use_ && ecn_ce_seen_) {
    scream_in_use_ = true;
    RTC_LOG(LS_INFO) << "Switching to ScreamV2 due to ECN CE";
  }
  return MaybeRunOnAllControllers(
      [&msg](webrtc::NetworkControllerInterface& controller) {
        return controller.OnTransportPacketsFeedback(msg);
      });
}

NetworkControlUpdate GoogCcScreamNetworkController::OnNetworkStateEstimate(
    NetworkStateEstimate msg) {
  return MaybeRunOnAllControllers(
      [&msg](webrtc::NetworkControllerInterface& controller) {
        return controller.OnNetworkStateEstimate(msg);
      });
}

}  // namespace webrtc
