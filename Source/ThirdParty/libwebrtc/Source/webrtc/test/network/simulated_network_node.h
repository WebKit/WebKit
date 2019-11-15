/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_NETWORK_SIMULATED_NETWORK_NODE_H_
#define TEST_NETWORK_SIMULATED_NETWORK_NODE_H_

#include "api/test/network_emulation_manager.h"
#include "call/simulated_network.h"

namespace webrtc {
namespace test {
// Helper struct to simplify creation of simulated network behaviors.
struct SimulatedNetworkNode {
  SimulatedNetwork* simulation;
  EmulatedNetworkNode* node;
  class Builder {
   public:
    Builder();
    explicit Builder(NetworkEmulationManager* net);
    Builder& config(SimulatedNetwork::Config config);
    Builder& delay_ms(int queue_delay_ms);
    Builder& capacity_kbps(int link_capacity_kbps);
    Builder& capacity_Mbps(int link_capacity_Mbps);
    Builder& loss(double loss_rate);
    SimulatedNetworkNode Build() const;
    SimulatedNetworkNode Build(NetworkEmulationManager* net) const;

   private:
    NetworkEmulationManager* const net_ = nullptr;
    SimulatedNetwork::Config config_;
  };
};
}  // namespace test
}  // namespace webrtc

#endif  // TEST_NETWORK_SIMULATED_NETWORK_NODE_H_
