/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/network/network_emulation_manager.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "api/array_view.h"
#include "api/field_trials_view.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/test/network_emulation/cross_traffic.h"
#include "api/test/network_emulation/network_emulation_interfaces.h"
#include "api/test/network_emulation_manager.h"
#include "api/test/simulated_network.h"
#include "api/test/time_controller.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/task_queue_for_test.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "test/create_test_environment.h"
#include "test/network/cross_traffic.h"
#include "test/network/emulated_network_manager.h"
#include "test/network/emulated_turn_server.h"
#include "test/network/network_emulation.h"
#include "test/network/simulated_network.h"
#include "test/network/traffic_route.h"
#include "test/time_controller/real_time_controller.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace test {
namespace {

// uint32_t representation of 192.168.0.0 address
constexpr uint32_t kMinIPv4Address = 0xC0A80000;
// uint32_t representation of 192.168.255.255 address
constexpr uint32_t kMaxIPv4Address = 0xC0A8FFFF;

std::unique_ptr<TimeController> CreateTimeController(
    TimeMode mode,
    const FieldTrialsView* field_trials) {
  switch (mode) {
    case TimeMode::kRealTime:
      return std::make_unique<RealTimeController>(field_trials);
    case TimeMode::kSimulated:
      // Using an offset of 100000 to get nice fixed width and readable
      // timestamps in typical test scenarios.
      const Timestamp kSimulatedStartTime = Timestamp::Seconds(100000);
      return std::make_unique<GlobalSimulatedTimeController>(
          kSimulatedStartTime);
  }
}
}  // namespace

NetworkEmulationManagerImpl::NetworkEmulationManagerImpl(
    NetworkEmulationManagerConfig config)
    : time_mode_(config.time_mode),
      stats_gathering_mode_(config.stats_gathering_mode),
      time_controller_(
          CreateTimeController(config.time_mode, config.field_trials)),
      env_(CreateTestEnvironment({.field_trials = config.field_trials,
                                  .time = time_controller_.get()})),
      fake_dtls_handshake_sizes_(config.fake_dtls_handshake_sizes),
      next_node_id_(1),
      next_ip4_address_(kMinIPv4Address),
      task_queue_(env_.task_queue_factory().CreateTaskQueue(
          "NetworkEmulation",
          TaskQueueFactory::Priority::kNormal)) {}

// TODO(srte): Ensure that any pending task that must be run for consistency
// (such as stats collection tasks) are not cancelled when the task queue is
// destroyed.
NetworkEmulationManagerImpl::~NetworkEmulationManagerImpl() {
  for (auto& turn_server : turn_servers_) {
    turn_server->Stop();
  }
}

EmulatedNetworkNode* NetworkEmulationManagerImpl::CreateEmulatedNode(
    BuiltInNetworkBehaviorConfig config,
    uint64_t random_seed) {
  return CreateEmulatedNode(
      std::make_unique<SimulatedNetwork>(config, random_seed));
}

EmulatedNetworkNode* NetworkEmulationManagerImpl::CreateEmulatedNode(
    std::unique_ptr<NetworkBehaviorInterface> network_behavior) {
  auto node = std::make_unique<EmulatedNetworkNode>(
      &env_.clock(), task_queue_.Get(), std::move(network_behavior),
      stats_gathering_mode_, fake_dtls_handshake_sizes_);
  EmulatedNetworkNode* out = node.get();
  task_queue_.PostTask([this, node = std::move(node)]() mutable {
    network_nodes_.push_back(std::move(node));
  });
  return out;
}

NetworkEmulationManager::SimulatedNetworkNode::Builder
NetworkEmulationManagerImpl::NodeBuilder() {
  return SimulatedNetworkNode::Builder(this);
}

EmulatedEndpointImpl* NetworkEmulationManagerImpl::CreateEndpoint(
    EmulatedEndpointConfig config) {
  std::optional<IPAddress> ip = config.ip;
  if (!ip) {
    switch (config.generated_ip_family) {
      case EmulatedEndpointConfig::IpAddressFamily::kIpv4:
        ip = GetNextIPv4Address();
        RTC_CHECK(ip) << "All auto generated IPv4 addresses exhausted";
        break;
      case EmulatedEndpointConfig::IpAddressFamily::kIpv6:
        ip = GetNextIPv4Address();
        RTC_CHECK(ip) << "All auto generated IPv6 addresses exhausted";
        ip = ip->AsIPv6Address();
        break;
    }
  }

  bool res = used_ip_addresses_.insert(*ip).second;
  RTC_CHECK(res) << "IP=" << ip->ToString() << " already in use";
  auto node = std::make_unique<EmulatedEndpointImpl>(
      EmulatedEndpointImpl::Options(next_node_id_++, *ip, config,
                                    stats_gathering_mode_),
      config.start_as_enabled, task_queue_.Get(), &env_.clock());
  EmulatedEndpointImpl* out = node.get();
  endpoints_.push_back(std::move(node));
  return out;
}

void NetworkEmulationManagerImpl::EnableEndpoint(EmulatedEndpoint* endpoint) {
  EmulatedNetworkManager* network_manager =
      endpoint_to_network_manager_[endpoint];
  RTC_CHECK(network_manager);
  static_cast<EmulatedEndpointImpl*>(endpoint)->Enable();
  network_manager->UpdateNetworks();
}

void NetworkEmulationManagerImpl::DisableEndpoint(EmulatedEndpoint* endpoint) {
  EmulatedNetworkManager* network_manager =
      endpoint_to_network_manager_[endpoint];
  RTC_CHECK(network_manager);
  static_cast<EmulatedEndpointImpl*>(endpoint)->Disable();
  network_manager->UpdateNetworks();
}

EmulatedRoute* NetworkEmulationManagerImpl::CreateRoute(
    EmulatedEndpoint* from,
    const std::vector<EmulatedNetworkNode*>& via_nodes,
    EmulatedEndpoint* to) {
  // Because endpoint has no send node by default at least one should be
  // provided here.
  RTC_CHECK(!via_nodes.empty());

  static_cast<EmulatedEndpointImpl*>(from)->router()->SetReceiver(
      to->GetPeerLocalAddress(), via_nodes[0]);
  EmulatedNetworkNode* cur_node = via_nodes[0];
  for (size_t i = 1; i < via_nodes.size(); ++i) {
    cur_node->router()->SetReceiver(to->GetPeerLocalAddress(), via_nodes[i]);
    cur_node = via_nodes[i];
  }
  cur_node->router()->SetReceiver(to->GetPeerLocalAddress(), to);

  std::unique_ptr<EmulatedRoute> route = std::make_unique<EmulatedRoute>(
      static_cast<EmulatedEndpointImpl*>(from), std::move(via_nodes),
      static_cast<EmulatedEndpointImpl*>(to), /*is_default=*/false);
  EmulatedRoute* out = route.get();
  routes_.push_back(std::move(route));
  return out;
}

EmulatedRoute* NetworkEmulationManagerImpl::CreateRoute(
    const std::vector<EmulatedNetworkNode*>& via_nodes) {
  EmulatedEndpoint* from = CreateEndpoint(EmulatedEndpointConfig());
  EmulatedEndpoint* to = CreateEndpoint(EmulatedEndpointConfig());
  return CreateRoute(from, via_nodes, to);
}

EmulatedRoute* NetworkEmulationManagerImpl::CreateDefaultRoute(
    EmulatedEndpoint* from,
    const std::vector<EmulatedNetworkNode*>& via_nodes,
    EmulatedEndpoint* to) {
  // Because endpoint has no send node by default at least one should be
  // provided here.
  RTC_CHECK(!via_nodes.empty());

  static_cast<EmulatedEndpointImpl*>(from)->router()->SetDefaultReceiver(
      via_nodes[0]);
  EmulatedNetworkNode* cur_node = via_nodes[0];
  for (size_t i = 1; i < via_nodes.size(); ++i) {
    cur_node->router()->SetDefaultReceiver(via_nodes[i]);
    cur_node = via_nodes[i];
  }
  cur_node->router()->SetDefaultReceiver(to);

  std::unique_ptr<EmulatedRoute> route = std::make_unique<EmulatedRoute>(
      static_cast<EmulatedEndpointImpl*>(from), std::move(via_nodes),
      static_cast<EmulatedEndpointImpl*>(to), /*is_default=*/true);
  EmulatedRoute* out = route.get();
  routes_.push_back(std::move(route));
  return out;
}

void NetworkEmulationManagerImpl::ClearRoute(EmulatedRoute* route) {
  RTC_CHECK(route->active) << "Route already cleared";
  task_queue_.SendTask([route]() {
    // Remove receiver from intermediate nodes.
    for (auto* node : route->via_nodes) {
      if (route->is_default) {
        node->router()->RemoveDefaultReceiver();
      } else {
        node->router()->RemoveReceiver(route->to->GetPeerLocalAddress());
      }
    }
    // Remove destination endpoint from source endpoint's router.
    if (route->is_default) {
      route->from->router()->RemoveDefaultReceiver();
    } else {
      route->from->router()->RemoveReceiver(route->to->GetPeerLocalAddress());
    }

    route->active = false;
  });
}

TcpMessageRoute* NetworkEmulationManagerImpl::CreateTcpRoute(
    EmulatedRoute* send_route,
    EmulatedRoute* ret_route) {
  auto tcp_route = std::make_unique<TcpMessageRouteImpl>(
      &env_.clock(), task_queue_.Get(), send_route, ret_route);
  auto* route_ptr = tcp_route.get();
  task_queue_.PostTask([this, tcp_route = std::move(tcp_route)]() mutable {
    tcp_message_routes_.push_back(std::move(tcp_route));
  });
  return route_ptr;
}

CrossTrafficRoute* NetworkEmulationManagerImpl::CreateCrossTrafficRoute(
    const std::vector<EmulatedNetworkNode*>& via_nodes) {
  RTC_CHECK(!via_nodes.empty());
  EmulatedEndpointImpl* endpoint = CreateEndpoint(EmulatedEndpointConfig());

  // Setup a route via specified nodes.
  EmulatedNetworkNode* cur_node = via_nodes[0];
  for (size_t i = 1; i < via_nodes.size(); ++i) {
    cur_node->router()->SetReceiver(endpoint->GetPeerLocalAddress(),
                                    via_nodes[i]);
    cur_node = via_nodes[i];
  }
  cur_node->router()->SetReceiver(endpoint->GetPeerLocalAddress(), endpoint);

  std::unique_ptr<CrossTrafficRoute> traffic_route =
      std::make_unique<CrossTrafficRouteImpl>(&env_.clock(), via_nodes[0],
                                              endpoint);
  CrossTrafficRoute* out = traffic_route.get();
  traffic_routes_.push_back(std::move(traffic_route));
  return out;
}

CrossTrafficGenerator* NetworkEmulationManagerImpl::StartCrossTraffic(
    std::unique_ptr<CrossTrafficGenerator> generator) {
  CrossTrafficGenerator* out = generator.get();
  task_queue_.PostTask([this, generator = std::move(generator)]() mutable {
    auto* generator_ptr = generator.get();

    auto repeating_task_handle =
        RepeatingTaskHandle::Start(task_queue_.Get(), [this, generator_ptr] {
          generator_ptr->Process(Now());
          return generator_ptr->GetProcessInterval();
        });

    cross_traffics_.push_back(CrossTrafficSource(
        std::move(generator), std::move(repeating_task_handle)));
  });
  return out;
}

void NetworkEmulationManagerImpl::StopCrossTraffic(
    CrossTrafficGenerator* generator) {
  task_queue_.PostTask([this, generator]() {
    auto it = std::find_if(cross_traffics_.begin(), cross_traffics_.end(),
                           [=](const CrossTrafficSource& el) {
                             return el.first.get() == generator;
                           });
    it->second.Stop();
    cross_traffics_.erase(it);
  });
}

EmulatedNetworkManagerInterface* absl_nonnull
NetworkEmulationManagerImpl::CreateEmulatedNetworkManagerInterface(
    const std::vector<EmulatedEndpoint*>& endpoints) {
  std::vector<EmulatedEndpointImpl*> endpoint_impls;
  endpoint_impls.reserve(endpoints.size());
  for (EmulatedEndpoint* endpoint : endpoints) {
    endpoint_impls.push_back(static_cast<EmulatedEndpointImpl*>(endpoint));
  }
  auto endpoints_container = std::make_unique<EndpointsContainer>(
      &env_.clock(), endpoint_impls, stats_gathering_mode_);
  auto network_manager = std::make_unique<EmulatedNetworkManager>(
      time_controller_.get(), task_queue_.Get(), endpoints_container.get());
  for (auto* endpoint : endpoints) {
    // Associate endpoint with network manager.
    endpoint_to_network_manager_[endpoint] = network_manager.get();
  }

  EmulatedNetworkManagerInterface* out = network_manager.get();

  endpoints_containers_.push_back(std::move(endpoints_container));
  network_managers_.push_back(std::move(network_manager));
  return out;
}

void NetworkEmulationManagerImpl::GetStats(
    ArrayView<EmulatedEndpoint* const> endpoints,
    std::function<void(EmulatedNetworkStats)> stats_callback) {
  task_queue_.PostTask([endpoints, stats_callback, env = env_,
                        stats_gathering_mode = stats_gathering_mode_]() {
    EmulatedNetworkStatsBuilder stats_builder(env.clock(),
                                              stats_gathering_mode);
    for (auto* endpoint : endpoints) {
      // It's safe to cast here because EmulatedEndpointImpl can be the only
      // implementation of EmulatedEndpoint, because only it has access to
      // EmulatedEndpoint constructor.
      auto endpoint_impl = static_cast<EmulatedEndpointImpl*>(endpoint);
      stats_builder.AddEmulatedNetworkStats(endpoint_impl->stats());
    }
    stats_callback(stats_builder.Build());
  });
}

void NetworkEmulationManagerImpl::GetStats(
    ArrayView<EmulatedNetworkNode* const> nodes,
    std::function<void(EmulatedNetworkNodeStats)> stats_callback) {
  task_queue_.PostTask([nodes, stats_callback, env = env_,
                        stats_gathering_mode = stats_gathering_mode_]() {
    EmulatedNetworkNodeStatsBuilder stats_builder(env.clock(),
                                                  stats_gathering_mode);
    for (auto* node : nodes) {
      stats_builder.AddEmulatedNetworkNodeStats(node->stats());
    }
    stats_callback(stats_builder.Build());
  });
}

std::optional<IPAddress> NetworkEmulationManagerImpl::GetNextIPv4Address() {
  uint32_t addresses_count = kMaxIPv4Address - kMinIPv4Address;
  for (uint32_t i = 0; i < addresses_count; i++) {
    IPAddress ip(next_ip4_address_);
    if (next_ip4_address_ == kMaxIPv4Address) {
      next_ip4_address_ = kMinIPv4Address;
    } else {
      next_ip4_address_++;
    }
    if (used_ip_addresses_.find(ip) == used_ip_addresses_.end()) {
      return ip;
    }
  }
  return std::nullopt;
}

Timestamp NetworkEmulationManagerImpl::Now() const {
  return env_.clock().CurrentTime();
}

EmulatedTURNServerInterface* NetworkEmulationManagerImpl::CreateTURNServer(
    EmulatedTURNServerConfig config) {
  auto* client = CreateEndpoint(config.client_config);
  auto* peer = CreateEndpoint(config.client_config);
  char buf[128];
  SimpleStringBuilder str(buf);
  str.AppendFormat("turn_server_%u",
                   static_cast<unsigned>(turn_servers_.size()));
  auto turn = std::make_unique<EmulatedTURNServer>(
      env_, config, time_controller_->CreateThread(str.str()), client, peer);
  auto out = turn.get();
  turn_servers_.push_back(std::move(turn));
  return out;
}

}  // namespace test
}  // namespace webrtc
