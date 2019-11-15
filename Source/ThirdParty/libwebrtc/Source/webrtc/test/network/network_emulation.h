/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_NETWORK_NETWORK_EMULATION_H_
#define TEST_NETWORK_NETWORK_EMULATION_H_

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/test/network_emulation_manager.h"
#include "api/test/simulated_network.h"
#include "api/units/timestamp.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/network.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/task_queue_for_test.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "rtc_base/thread_checker.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

struct EmulatedIpPacket {
 public:
  EmulatedIpPacket(const rtc::SocketAddress& from,
                   const rtc::SocketAddress& to,
                   rtc::CopyOnWriteBuffer data,
                   Timestamp arrival_time);
  ~EmulatedIpPacket() = default;
  // This object is not copyable or assignable.
  EmulatedIpPacket(const EmulatedIpPacket&) = delete;
  EmulatedIpPacket& operator=(const EmulatedIpPacket&) = delete;
  // This object is only moveable.
  EmulatedIpPacket(EmulatedIpPacket&&) = default;
  EmulatedIpPacket& operator=(EmulatedIpPacket&&) = default;

  size_t size() const { return data.size(); }
  const uint8_t* cdata() const { return data.cdata(); }

  rtc::SocketAddress from;
  rtc::SocketAddress to;
  rtc::CopyOnWriteBuffer data;
  Timestamp arrival_time;
};

class EmulatedNetworkReceiverInterface {
 public:
  virtual ~EmulatedNetworkReceiverInterface() = default;

  virtual void OnPacketReceived(EmulatedIpPacket packet) = 0;
};

class LinkEmulation : public EmulatedNetworkReceiverInterface {
 public:
  LinkEmulation(Clock* clock,
                rtc::TaskQueue* task_queue,
                std::unique_ptr<NetworkBehaviorInterface> network_behavior,
                EmulatedNetworkReceiverInterface* receiver)
      : clock_(clock),
        task_queue_(task_queue),
        network_behavior_(std::move(network_behavior)),
        receiver_(receiver) {}
  void OnPacketReceived(EmulatedIpPacket packet) override;

 private:
  struct StoredPacket {
    uint64_t id;
    EmulatedIpPacket packet;
    bool removed;
  };
  void Process(Timestamp at_time) RTC_RUN_ON(task_queue_);
  void HandlePacketReceived(EmulatedIpPacket packet) RTC_RUN_ON(task_queue_);

  Clock* const clock_;
  rtc::TaskQueue* const task_queue_;
  const std::unique_ptr<NetworkBehaviorInterface> network_behavior_
      RTC_GUARDED_BY(task_queue_);
  EmulatedNetworkReceiverInterface* const receiver_;
  RepeatingTaskHandle process_task_ RTC_GUARDED_BY(task_queue_);
  std::deque<StoredPacket> packets_ RTC_GUARDED_BY(task_queue_);
  uint64_t next_packet_id_ RTC_GUARDED_BY(task_queue_) = 1;
};

class NetworkRouterNode : public EmulatedNetworkReceiverInterface {
 public:
  explicit NetworkRouterNode(rtc::TaskQueue* task_queue);

  void OnPacketReceived(EmulatedIpPacket packet) override;
  void SetReceiver(rtc::IPAddress dest_ip,
                   EmulatedNetworkReceiverInterface* receiver);
  void RemoveReceiver(rtc::IPAddress dest_ip);
  void SetWatcher(std::function<void(const EmulatedIpPacket&)> watcher);

 private:
  rtc::TaskQueue* const task_queue_;
  std::map<rtc::IPAddress, EmulatedNetworkReceiverInterface*> routing_
      RTC_GUARDED_BY(task_queue_);
  std::function<void(const EmulatedIpPacket&)> watcher_
      RTC_GUARDED_BY(task_queue_);
};

// Represents node in the emulated network. Nodes can be connected with each
// other to form different networks with different behavior. The behavior of
// the node itself is determined by a concrete implementation of
// NetworkBehaviorInterface that is provided on construction.
class EmulatedNetworkNode : public EmulatedNetworkReceiverInterface {
 public:
  // Creates node based on |network_behavior|. The specified |packet_overhead|
  // is added to the size of each packet in the information provided to
  // |network_behavior|.
  // |task_queue| is used to process packets and to forward the packets when
  // they are ready.
  EmulatedNetworkNode(
      Clock* clock,
      rtc::TaskQueue* task_queue,
      std::unique_ptr<NetworkBehaviorInterface> network_behavior);
  ~EmulatedNetworkNode() override;
  RTC_DISALLOW_COPY_AND_ASSIGN(EmulatedNetworkNode);

  void OnPacketReceived(EmulatedIpPacket packet) override;

  LinkEmulation* link() { return &link_; }
  NetworkRouterNode* router() { return &router_; }

  // Creates a route for the given receiver_ip over all the given nodes to the
  // given receiver.
  static void CreateRoute(rtc::IPAddress receiver_ip,
                          std::vector<EmulatedNetworkNode*> nodes,
                          EmulatedNetworkReceiverInterface* receiver);
  static void ClearRoute(rtc::IPAddress receiver_ip,
                         std::vector<EmulatedNetworkNode*> nodes);

 private:
  NetworkRouterNode router_;
  LinkEmulation link_;
};

// Represents single network interface on the device.
// It will be used as sender from socket side to send data to the network and
// will act as packet receiver from emulated network side to receive packets
// from other EmulatedNetworkNodes.
class EmulatedEndpoint : public EmulatedNetworkReceiverInterface {
 public:
  EmulatedEndpoint(uint64_t id,
                   const rtc::IPAddress& ip,
                   bool is_enabled,
                   rtc::TaskQueue* task_queue,
                   Clock* clock);
  ~EmulatedEndpoint() override;

  uint64_t GetId() const;

  NetworkRouterNode* router() { return &router_; }
  // Send packet into network.
  // |from| will be used to set source address for the packet in destination
  // socket.
  // |to| will be used for routing verification and picking right socket by port
  // on destination endpoint.
  void SendPacket(const rtc::SocketAddress& from,
                  const rtc::SocketAddress& to,
                  rtc::CopyOnWriteBuffer packet);

  // Binds receiver to this endpoint to send and receive data.
  // |desired_port| is a port that should be used. If it is equal to 0,
  // endpoint will pick the first available port starting from
  // |kFirstEphemeralPort|.
  //
  // Returns the port, that should be used (it will be equals to desired, if
  // |desired_port| != 0 and is free or will be the one, selected by endpoint)
  // or absl::nullopt if desired_port in used. Also fails if there are no more
  // free ports to bind to.
  absl::optional<uint16_t> BindReceiver(
      uint16_t desired_port,
      EmulatedNetworkReceiverInterface* receiver);
  void UnbindReceiver(uint16_t port);

  rtc::IPAddress GetPeerLocalAddress() const;

  // Will be called to deliver packet into endpoint from network node.
  void OnPacketReceived(EmulatedIpPacket packet) override;

  void Enable();
  void Disable();
  bool Enabled() const;

  const rtc::Network& network() const { return *network_.get(); }

  EmulatedNetworkStats stats();

 private:
  static constexpr uint16_t kFirstEphemeralPort = 49152;
  uint16_t NextPort() RTC_EXCLUSIVE_LOCKS_REQUIRED(receiver_lock_);
  void UpdateSendStats(const EmulatedIpPacket& packet);
  void UpdateReceiveStats(const EmulatedIpPacket& packet);

  rtc::CriticalSection receiver_lock_;
  rtc::ThreadChecker enabled_state_checker_;

  uint64_t id_;
  // Peer's local IP address for this endpoint network interface.
  const rtc::IPAddress peer_local_addr_;
  bool is_enabled_ RTC_GUARDED_BY(enabled_state_checker_);
  Clock* const clock_;
  rtc::TaskQueue* const task_queue_;
  std::unique_ptr<rtc::Network> network_;
  NetworkRouterNode router_;

  uint16_t next_port_ RTC_GUARDED_BY(receiver_lock_);
  std::map<uint16_t, EmulatedNetworkReceiverInterface*> port_to_receiver_
      RTC_GUARDED_BY(receiver_lock_);

  EmulatedNetworkStats stats_ RTC_GUARDED_BY(task_queue_);
};

class EmulatedRoute {
 public:
  EmulatedRoute(EmulatedEndpoint* from,
                std::vector<EmulatedNetworkNode*> via_nodes,
                EmulatedEndpoint* to)
      : from(from), via_nodes(std::move(via_nodes)), to(to), active(true) {}

  EmulatedEndpoint* from;
  std::vector<EmulatedNetworkNode*> via_nodes;
  EmulatedEndpoint* to;
  bool active;
};
class EndpointsContainer {
 public:
  explicit EndpointsContainer(const std::vector<EmulatedEndpoint*>& endpoints);

  EmulatedEndpoint* LookupByLocalAddress(const rtc::IPAddress& local_ip) const;
  bool HasEndpoint(EmulatedEndpoint* endpoint) const;
  // Returns list of networks for enabled endpoints. Caller takes ownership of
  // returned rtc::Network objects.
  std::vector<std::unique_ptr<rtc::Network>> GetEnabledNetworks() const;
  EmulatedNetworkStats GetStats() const;

 private:
  const std::vector<EmulatedEndpoint*> endpoints_;
};

template <typename FakePacketType>
class FakePacketRoute : public EmulatedNetworkReceiverInterface {
 public:
  FakePacketRoute(EmulatedRoute* route,
                  std::function<void(FakePacketType, Timestamp)> action)
      : route_(route),
        action_(std::move(action)),
        send_addr_(route_->from->GetPeerLocalAddress(), 0),
        recv_addr_(route_->to->GetPeerLocalAddress(),
                   *route_->to->BindReceiver(0, this)) {}

  void SendPacket(size_t size, FakePacketType packet) {
    RTC_CHECK_GE(size, sizeof(int));
    sent_.emplace(next_packet_id_, packet);
    rtc::CopyOnWriteBuffer buf(size);
    reinterpret_cast<int*>(buf.data())[0] = next_packet_id_++;
    route_->from->SendPacket(send_addr_, recv_addr_, buf);
  }

  void OnPacketReceived(EmulatedIpPacket packet) override {
    int packet_id = reinterpret_cast<int*>(packet.data.data())[0];
    action_(std::move(sent_[packet_id]), packet.arrival_time);
    sent_.erase(packet_id);
  }

 private:
  EmulatedRoute* const route_;
  const std::function<void(FakePacketType, Timestamp)> action_;
  const rtc::SocketAddress send_addr_;
  const rtc::SocketAddress recv_addr_;
  int next_packet_id_ = 0;
  std::map<int, FakePacketType> sent_;
};

template <typename RequestPacketType, typename ResponsePacketType>
class TwoWayFakeTrafficRoute {
 public:
  class TrafficHandlerInterface {
   public:
    virtual void OnRequest(RequestPacketType, Timestamp) = 0;
    virtual void OnResponse(ResponsePacketType, Timestamp) = 0;
    virtual ~TrafficHandlerInterface() = default;
  };
  TwoWayFakeTrafficRoute(TrafficHandlerInterface* handler,
                         EmulatedRoute* send_route,
                         EmulatedRoute* ret_route)
      : handler_(handler),
        request_handler_{send_route,
                         [&](RequestPacketType packet, Timestamp arrival_time) {
                           handler_->OnRequest(std::move(packet), arrival_time);
                         }},
        response_handler_{
            ret_route, [&](ResponsePacketType packet, Timestamp arrival_time) {
              handler_->OnResponse(std::move(packet), arrival_time);
            }} {}
  void SendRequest(size_t size, RequestPacketType packet) {
    request_handler_.SendPacket(size, std::move(packet));
  }
  void SendResponse(size_t size, ResponsePacketType packet) {
    response_handler_.SendPacket(size, std::move(packet));
  }

 private:
  TrafficHandlerInterface* handler_;
  FakePacketRoute<RequestPacketType> request_handler_;
  FakePacketRoute<ResponsePacketType> response_handler_;
};
}  // namespace webrtc

#endif  // TEST_NETWORK_NETWORK_EMULATION_H_
