/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/network/simulated_network_node.h"

#include <utility>

#include "absl/memory/memory.h"

namespace webrtc {
namespace test {

SimulatedNetworkNode::Builder::Builder() {}

SimulatedNetworkNode::Builder::Builder(NetworkEmulationManager* net)
    : net_(net) {}

SimulatedNetworkNode::Builder& SimulatedNetworkNode::Builder::config(
    SimulatedNetwork::Config config) {
  config_ = config;
  return *this;
}

SimulatedNetworkNode::Builder& SimulatedNetworkNode::Builder::delay_ms(
    int queue_delay_ms) {
  config_.queue_delay_ms = queue_delay_ms;
  return *this;
}

SimulatedNetworkNode::Builder& SimulatedNetworkNode::Builder::capacity_kbps(
    int link_capacity_kbps) {
  config_.link_capacity_kbps = link_capacity_kbps;
  return *this;
}

SimulatedNetworkNode::Builder& SimulatedNetworkNode::Builder::capacity_Mbps(
    int link_capacity_Mbps) {
  config_.link_capacity_kbps = link_capacity_Mbps * 1000;
  return *this;
}

SimulatedNetworkNode::Builder& SimulatedNetworkNode::Builder::loss(
    double loss_rate) {
  config_.loss_percent = std::round(loss_rate * 100);
  return *this;
}

SimulatedNetworkNode SimulatedNetworkNode::Builder::Build() const {
  RTC_DCHECK(net_);
  return Build(net_);
}

SimulatedNetworkNode SimulatedNetworkNode::Builder::Build(
    NetworkEmulationManager* net) const {
  SimulatedNetworkNode res;
  auto behavior = absl::make_unique<SimulatedNetwork>(config_);
  res.simulation = behavior.get();
  res.node = net->CreateEmulatedNode(std::move(behavior));
  return res;
}

}  // namespace test
}  // namespace webrtc
