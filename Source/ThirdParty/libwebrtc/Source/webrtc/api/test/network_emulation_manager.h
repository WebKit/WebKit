/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_NETWORK_EMULATION_MANAGER_H_
#define API_TEST_NETWORK_EMULATION_MANAGER_H_

#include <memory>
#include <vector>

#include "api/test/simulated_network.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/timestamp.h"
#include "rtc_base/network.h"
#include "rtc_base/thread.h"

namespace webrtc {

// This API is still in development and can be changed without prior notice.

// These classes are forward declared here, because they used as handles, to
// make it possible for client code to operate with these abstractions and build
// required network configuration. With forward declaration here implementation
// is more readable, than with interfaces approach and cause user needn't any
// API methods on these abstractions it is acceptable here.

// EmulatedNetworkNode is an abstraction for some network in the real world,
// like 3G network between peers, or Wi-Fi for one peer and LTE for another.
// Multiple networks can be joined into chain emulating a network path from
// one peer to another.
class EmulatedNetworkNode;
// EmulatedEndpoint is and abstraction for network interface on device.
class EmulatedEndpoint;
// EmulatedRoute is handle for single route from one network interface on one
// peer device to another network interface on another peer device.
class EmulatedRoute;

struct EmulatedEndpointConfig {
  enum class IpAddressFamily { kIpv4, kIpv6 };

  IpAddressFamily generated_ip_family = IpAddressFamily::kIpv4;
  // If specified will be used as IP address for endpoint node. Must be unique
  // among all created nodes.
  absl::optional<rtc::IPAddress> ip;
  // Should endpoint be enabled or not, when it will be created.
  // Enabled endpoints will be available for webrtc to send packets.
  bool start_as_enabled = true;
};

struct EmulatedNetworkStats {
  int64_t packets_sent = 0;
  DataSize bytes_sent = DataSize::Zero();
  // Total amount of packets received with or without destination.
  int64_t packets_received = 0;
  // Total amount of bytes in received packets.
  DataSize bytes_received = DataSize::Zero();
  // Total amount of packets that were received, but no destination was found.
  int64_t packets_dropped = 0;
  // Total amount of bytes in dropped packets.
  DataSize bytes_dropped = DataSize::Zero();

  DataSize first_received_packet_size = DataSize::Zero();
  DataSize first_sent_packet_size = DataSize::Zero();

  Timestamp first_packet_sent_time = Timestamp::PlusInfinity();
  Timestamp last_packet_sent_time = Timestamp::PlusInfinity();
  Timestamp first_packet_received_time = Timestamp::PlusInfinity();
  Timestamp last_packet_received_time = Timestamp::PlusInfinity();

  DataRate AverageSendRate() const {
    RTC_DCHECK_GE(packets_sent, 2);
    return (bytes_sent - first_sent_packet_size) /
           (last_packet_sent_time - first_packet_sent_time);
  }
  DataRate AverageReceiveRate() const {
    RTC_DCHECK_GE(packets_received, 2);
    return (bytes_received - first_received_packet_size) /
           (last_packet_received_time - first_packet_received_time);
  }
};

// Provide interface to obtain all required objects to inject network emulation
// layer into PeerConnection. Also contains information about network interfaces
// accessible by PeerConnection.
class EmulatedNetworkManagerInterface {
 public:
  virtual ~EmulatedNetworkManagerInterface() = default;

  virtual rtc::Thread* network_thread() = 0;
  virtual rtc::NetworkManager* network_manager() = 0;

  // Returns summarized network stats for endpoints for this manager.
  virtual void GetStats(
      std::function<void(EmulatedNetworkStats)> stats_callback) const = 0;
};

// Provides an API for creating and configuring emulated network layer.
// All objects returned by this API are owned by NetworkEmulationManager itself
// and will be deleted when manager will be deleted.
class NetworkEmulationManager {
 public:
  virtual ~NetworkEmulationManager() = default;

  // Creates an emulated network node, which represents single network in
  // the emulated network layer.
  virtual EmulatedNetworkNode* CreateEmulatedNode(
      BuiltInNetworkBehaviorConfig config) = 0;
  virtual EmulatedNetworkNode* CreateEmulatedNode(
      std::unique_ptr<NetworkBehaviorInterface> network_behavior) = 0;

  // Creates an emulated endpoint, which represents single network interface on
  // the peer's device.
  virtual EmulatedEndpoint* CreateEndpoint(EmulatedEndpointConfig config) = 0;
  // Enable emulated endpoint to make it available for webrtc.
  // Caller mustn't enable currently enabled endpoint.
  virtual void EnableEndpoint(EmulatedEndpoint* endpoint) = 0;
  // Disable emulated endpoint to make it unavailable for webrtc.
  // Caller mustn't disable currently disabled endpoint.
  virtual void DisableEndpoint(EmulatedEndpoint* endpoint) = 0;

  // Creates a route between endpoints going through specified network nodes.
  // This route is single direction only and describe how traffic that was
  // sent by network interface |from| have to be delivered to the network
  // interface |to|. Return object can be used to remove created route. The
  // route must contains at least one network node inside it.
  //
  // Assume that E{0-9} are endpoints and N{0-9} are network nodes, then
  // creation of the route have to follow these rules:
  //   1. A route consists of a source endpoint, an ordered list of one or
  //      more network nodes, and a destination endpoint.
  //   2. If (E1, ..., E2) is a route, then E1 != E2.
  //      In other words, the source and the destination may not be the same.
  //   3. Given two simultaneously existing routes (E1, ..., E2) and
  //      (E3, ..., E4), either E1 != E3 or E2 != E4.
  //      In other words, there may be at most one route from any given source
  //      endpoint to any given destination endpoint.
  //   4. Given two simultaneously existing routes (E1, ..., N1, ..., E2)
  //      and (E3, ..., N2, ..., E4), either N1 != N2 or E2 != E4.
  //      In other words, a network node may not belong to two routes that lead
  //      to the same destination endpoint.
  virtual EmulatedRoute* CreateRoute(
      EmulatedEndpoint* from,
      const std::vector<EmulatedNetworkNode*>& via_nodes,
      EmulatedEndpoint* to) = 0;
  // Removes route previously created by CreateRoute(...).
  // Caller mustn't call this function with route, that have been already
  // removed earlier.
  virtual void ClearRoute(EmulatedRoute* route) = 0;

  // Creates EmulatedNetworkManagerInterface which can be used then to inject
  // network emulation layer into PeerConnection. |endpoints| - are available
  // network interfaces for PeerConnection. If endpoint is enabled, it will be
  // immediately available for PeerConnection, otherwise user will be able to
  // enable endpoint later to make it available for PeerConnection.
  virtual EmulatedNetworkManagerInterface*
  CreateEmulatedNetworkManagerInterface(
      const std::vector<EmulatedEndpoint*>& endpoints) = 0;
};

}  // namespace webrtc

#endif  // API_TEST_NETWORK_EMULATION_MANAGER_H_
