/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_NETWORK_NETWORK_EMULATION_MANAGER_H_
#define TEST_NETWORK_NETWORK_EMULATION_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "api/test/network_emulation_manager.h"
#include "api/test/simulated_network.h"
#include "api/test/time_controller.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/logging.h"
#include "rtc_base/network.h"
#include "rtc_base/task_queue_for_test.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "rtc_base/thread.h"
#include "system_wrappers/include/clock.h"
#include "test/network/cross_traffic.h"
#include "test/network/emulated_network_manager.h"
#include "test/network/fake_network_socket_server.h"
#include "test/network/network_emulation.h"
#include "test/network/traffic_route.h"

namespace webrtc {
namespace test {

class NetworkEmulationManagerImpl : public NetworkEmulationManager {
 public:
  explicit NetworkEmulationManagerImpl(TimeMode mode);
  ~NetworkEmulationManagerImpl();

  EmulatedNetworkNode* CreateEmulatedNode(
      BuiltInNetworkBehaviorConfig config) override;
  EmulatedNetworkNode* CreateEmulatedNode(
      std::unique_ptr<NetworkBehaviorInterface> network_behavior) override;

  SimulatedNetworkNode::Builder NodeBuilder() override;

  EmulatedEndpoint* CreateEndpoint(EmulatedEndpointConfig config) override;
  void EnableEndpoint(EmulatedEndpoint* endpoint) override;
  void DisableEndpoint(EmulatedEndpoint* endpoint) override;

  EmulatedRoute* CreateRoute(EmulatedEndpoint* from,
                             const std::vector<EmulatedNetworkNode*>& via_nodes,
                             EmulatedEndpoint* to) override;

  EmulatedRoute* CreateRoute(
      const std::vector<EmulatedNetworkNode*>& via_nodes) override;

  void ClearRoute(EmulatedRoute* route) override;

  TrafficRoute* CreateTrafficRoute(
      const std::vector<EmulatedNetworkNode*>& via_nodes);
  RandomWalkCrossTraffic* CreateRandomWalkCrossTraffic(
      TrafficRoute* traffic_route,
      RandomWalkConfig config);
  PulsedPeaksCrossTraffic* CreatePulsedPeaksCrossTraffic(
      TrafficRoute* traffic_route,
      PulsedPeaksConfig config);
  FakeTcpCrossTraffic* StartFakeTcpCrossTraffic(
      std::vector<EmulatedNetworkNode*> send_link,
      std::vector<EmulatedNetworkNode*> ret_link,
      FakeTcpConfig config);

  TcpMessageRoute* CreateTcpRoute(EmulatedRoute* send_route,
                                  EmulatedRoute* ret_route) override;

  void StopCrossTraffic(FakeTcpCrossTraffic* traffic);

  EmulatedNetworkManagerInterface* CreateEmulatedNetworkManagerInterface(
      const std::vector<EmulatedEndpoint*>& endpoints) override;

  TimeController* time_controller() override { return time_controller_.get(); }

  Timestamp Now() const;

 private:
  absl::optional<rtc::IPAddress> GetNextIPv4Address();
  const std::unique_ptr<TimeController> time_controller_;
  Clock* const clock_;
  int next_node_id_;

  RepeatingTaskHandle process_task_handle_;

  uint32_t next_ip4_address_;
  std::set<rtc::IPAddress> used_ip_addresses_;

  // All objects can be added to the manager only when it is idle.
  std::vector<std::unique_ptr<EmulatedEndpoint>> endpoints_;
  std::vector<std::unique_ptr<EmulatedNetworkNode>> network_nodes_;
  std::vector<std::unique_ptr<EmulatedRoute>> routes_;
  std::vector<std::unique_ptr<TrafficRoute>> traffic_routes_;
  std::vector<std::unique_ptr<RandomWalkCrossTraffic>> random_cross_traffics_;
  std::vector<std::unique_ptr<PulsedPeaksCrossTraffic>> pulsed_cross_traffics_;
  std::list<std::unique_ptr<FakeTcpCrossTraffic>> tcp_cross_traffics_;
  std::list<std::unique_ptr<TcpMessageRouteImpl>> tcp_message_routes_;
  std::vector<std::unique_ptr<EndpointsContainer>> endpoints_containers_;
  std::vector<std::unique_ptr<EmulatedNetworkManager>> network_managers_;

  std::map<EmulatedEndpoint*, EmulatedNetworkManager*>
      endpoint_to_network_manager_;

  // Must be the last field, so it will be deleted first, because tasks
  // in the TaskQueue can access other fields of the instance of this class.
  TaskQueueForTest task_queue_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_NETWORK_NETWORK_EMULATION_MANAGER_H_
