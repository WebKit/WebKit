/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/test/network_emulation_manager.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/test/network_emulation/leaky_bucket_network_queue.h"
#include "api/test/network_emulation/network_emulation_interfaces.h"
#include "api/test/network_emulation/network_queue.h"
#include "api/test/simulated_network.h"
#include "api/units/data_rate.h"
#include "rtc_base/checks.h"
#include "test/network/simulated_network.h"

namespace webrtc {

bool AbslParseFlag(absl::string_view text, TimeMode* mode, std::string* error) {
  if (text == "realtime") {
    *mode = TimeMode::kRealTime;
    return true;
  }
  if (text == "simulated") {
    *mode = TimeMode::kSimulated;
    return true;
  }
  *error =
      "Unknown value for TimeMode enum. Options are 'realtime' or 'simulated'";
  return false;
}

std::string AbslUnparseFlag(TimeMode mode) {
  switch (mode) {
    case TimeMode::kRealTime:
      return "realtime";
    case TimeMode::kSimulated:
      return "simulated";
  }
  RTC_CHECK_NOTREACHED();
  return "unknown";
}

NetworkEmulationManager::SimulatedNetworkNode::Builder&
NetworkEmulationManager::SimulatedNetworkNode::Builder::config(
    BuiltInNetworkBehaviorConfig config) {
  config_ = config;
  return *this;
}

NetworkEmulationManager::SimulatedNetworkNode::Builder&
NetworkEmulationManager::SimulatedNetworkNode::Builder::queue_factory(
    NetworkQueueFactory& queue_factory) {
  queue_factory_ = &queue_factory;
  return *this;
}

NetworkEmulationManager::SimulatedNetworkNode::Builder&
NetworkEmulationManager::SimulatedNetworkNode::Builder::delay_ms(
    int queue_delay_ms) {
  config_.queue_delay_ms = queue_delay_ms;
  return *this;
}

NetworkEmulationManager::SimulatedNetworkNode::Builder&
NetworkEmulationManager::SimulatedNetworkNode::Builder::capacity(
    DataRate link_capacity) {
  config_.link_capacity = link_capacity;
  return *this;
}

NetworkEmulationManager::SimulatedNetworkNode::Builder&
NetworkEmulationManager::SimulatedNetworkNode::Builder::capacity_kbps(
    int link_capacity_kbps) {
  if (link_capacity_kbps > 0) {
    config_.link_capacity = DataRate::KilobitsPerSec(link_capacity_kbps);
  } else {
    config_.link_capacity = DataRate::Infinity();
  }
  return *this;
}

NetworkEmulationManager::SimulatedNetworkNode::Builder&
NetworkEmulationManager::SimulatedNetworkNode::Builder::capacity_Mbps(
    int link_capacity_Mbps) {
  if (link_capacity_Mbps > 0) {
    config_.link_capacity = DataRate::KilobitsPerSec(link_capacity_Mbps * 1000);
  } else {
    config_.link_capacity = DataRate::Infinity();
  }
  return *this;
}

NetworkEmulationManager::SimulatedNetworkNode::Builder&
NetworkEmulationManager::SimulatedNetworkNode::Builder::loss(double loss_rate) {
  config_.loss_percent = loss_rate * 100;
  return *this;
}

NetworkEmulationManager::SimulatedNetworkNode::Builder&
NetworkEmulationManager::SimulatedNetworkNode::Builder::packet_queue_length(
    int max_queue_length_in_packets) {
  config_.queue_length_packets = max_queue_length_in_packets;
  return *this;
}

NetworkEmulationManager::SimulatedNetworkNode::Builder&
NetworkEmulationManager::SimulatedNetworkNode::Builder::
    delay_standard_deviation_ms(int delay_standard_deviation_ms) {
  config_.delay_standard_deviation_ms = delay_standard_deviation_ms;
  return *this;
}

NetworkEmulationManager::SimulatedNetworkNode::Builder&
NetworkEmulationManager::SimulatedNetworkNode::Builder::allow_reordering() {
  config_.allow_reordering = true;
  return *this;
}

NetworkEmulationManager::SimulatedNetworkNode::Builder&
NetworkEmulationManager::SimulatedNetworkNode::Builder::avg_burst_loss_length(
    int avg_burst_loss_length) {
  config_.avg_burst_loss_length = avg_burst_loss_length;
  return *this;
}

NetworkEmulationManager::SimulatedNetworkNode::Builder&
NetworkEmulationManager::SimulatedNetworkNode::Builder::packet_overhead(
    int packet_overhead) {
  config_.packet_overhead = packet_overhead;
  return *this;
}

NetworkEmulationManager::SimulatedNetworkNode
NetworkEmulationManager::SimulatedNetworkNode::Builder::Build(
    uint64_t random_seed) const {
  RTC_CHECK(net_);
  std::unique_ptr<NetworkQueue> network_queue =
      queue_factory_ ? queue_factory_->CreateQueue()
                     : std::make_unique<LeakyBucketNetworkQueue>();
  auto behavior = std::make_unique<SimulatedNetwork>(config_, random_seed,
                                                     std::move(network_queue));
  return SimulatedNetworkNode{
      .simulation = behavior.get(),
      .node = net_->CreateEmulatedNode(std::move(behavior))};
}

std::pair<EmulatedNetworkManagerInterface*, EmulatedNetworkManagerInterface*>
NetworkEmulationManager::CreateEndpointPairWithTwoWayRoutes(
    const BuiltInNetworkBehaviorConfig& config,
    int alice_interface_count,
    int bob_interface_count) {
  auto* alice_node = CreateEmulatedNode(config);
  auto* bob_node = CreateEmulatedNode(config);

  std::vector<EmulatedEndpoint*> alice_endpoints(
      alice_interface_count, CreateEndpoint(EmulatedEndpointConfig()));
  std::vector<EmulatedEndpoint*> bob_endpoints(
      bob_interface_count, CreateEndpoint(EmulatedEndpointConfig()));

  for (auto alice_endpoint : alice_endpoints) {
    for (auto bob_endpoint : bob_endpoints) {
      CreateRoute(alice_endpoint, {alice_node}, bob_endpoint);
      CreateRoute(bob_endpoint, {bob_node}, alice_endpoint);
    }
  }

  return {
      CreateEmulatedNetworkManagerInterface(alice_endpoints),
      CreateEmulatedNetworkManagerInterface(bob_endpoints),
  };
}
}  // namespace webrtc
