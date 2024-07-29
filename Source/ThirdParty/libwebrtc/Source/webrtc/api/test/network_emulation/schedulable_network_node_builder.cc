/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/test/network_emulation/schedulable_network_node_builder.h"

#include <memory>
#include <optional>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "api/test/network_emulation/network_config_schedule.pb.h"
#include "api/test/network_emulation_manager.h"
#include "api/units/timestamp.h"
#include "test/network/schedulable_network_behavior.h"

namespace webrtc {

SchedulableNetworkNodeBuilder::SchedulableNetworkNodeBuilder(
    webrtc::NetworkEmulationManager& net,
    network_behaviour::NetworkConfigSchedule schedule)
    : net_(net),
      schedule_(std::move(schedule)),
      start_condition_([](webrtc::Timestamp) { return true; }) {}

void SchedulableNetworkNodeBuilder::set_start_condition(
    absl::AnyInvocable<bool(webrtc::Timestamp)> start_condition) {
  start_condition_ = std::move(start_condition);
}

webrtc::EmulatedNetworkNode* SchedulableNetworkNodeBuilder::Build(
    std::optional<uint64_t> random_seed) {
  uint64_t seed = random_seed.has_value()
                      ? *random_seed
                      : static_cast<uint64_t>(rtc::TimeNanos());
  return net_.CreateEmulatedNode(std::make_unique<SchedulableNetworkBehavior>(
      std::move(schedule_), seed, *net_.time_controller()->GetClock(),
      std::move(start_condition_)));
}
}  // namespace webrtc
