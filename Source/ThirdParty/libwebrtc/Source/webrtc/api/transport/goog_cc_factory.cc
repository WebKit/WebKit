/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/transport/goog_cc_factory.h"

#include "absl/memory/memory.h"
#include "modules/congestion_controller/goog_cc/goog_cc_network_control.h"
namespace webrtc {
GoogCcNetworkControllerFactory::GoogCcNetworkControllerFactory(
    RtcEventLog* event_log)
    : event_log_(event_log) {}

std::unique_ptr<NetworkControllerInterface>
GoogCcNetworkControllerFactory::Create(NetworkControllerConfig config) {
  return absl::make_unique<GoogCcNetworkController>(event_log_, config, false);
}

TimeDelta GoogCcNetworkControllerFactory::GetProcessInterval() const {
  const int64_t kUpdateIntervalMs = 25;
  return TimeDelta::ms(kUpdateIntervalMs);
}

GoogCcFeedbackNetworkControllerFactory::GoogCcFeedbackNetworkControllerFactory(
    RtcEventLog* event_log)
    : event_log_(event_log) {}

std::unique_ptr<NetworkControllerInterface>
GoogCcFeedbackNetworkControllerFactory::Create(NetworkControllerConfig config) {
  return absl::make_unique<GoogCcNetworkController>(event_log_, config, true);
}

TimeDelta GoogCcFeedbackNetworkControllerFactory::GetProcessInterval() const {
  const int64_t kUpdateIntervalMs = 25;
  return TimeDelta::ms(kUpdateIntervalMs);
}
}  // namespace webrtc
