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
#include "api/array_view.h"
#include "api/numerics/samples_stats_counter.h"
#include "api/sequence_checker.h"
#include "api/test/network_emulation_manager.h"
#include "api/test/simulated_network.h"
#include "api/units/timestamp.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/network.h"
#include "rtc_base/network_constants.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/task_queue_for_test.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "rtc_base/thread_annotations.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

// This class is immutable and so thread safe.
class EmulatedNetworkOutgoingStatsImpl final
    : public EmulatedNetworkOutgoingStats {
 public:
  EmulatedNetworkOutgoingStatsImpl(
      int64_t packets_sent,
      DataSize bytes_sent,
      SamplesStatsCounter sent_packets_size_counter,
      DataSize first_sent_packet_size,
      Timestamp first_packet_sent_time,
      Timestamp last_packet_sent_time)
      : packets_sent_(packets_sent),
        bytes_sent_(bytes_sent),
        sent_packets_size_counter_(std::move(sent_packets_size_counter)),
        first_sent_packet_size_(first_sent_packet_size),
        first_packet_sent_time_(first_packet_sent_time),
        last_packet_sent_time_(last_packet_sent_time) {}
  explicit EmulatedNetworkOutgoingStatsImpl(
      const EmulatedNetworkOutgoingStats& stats)
      : packets_sent_(stats.PacketsSent()),
        bytes_sent_(stats.BytesSent()),
        sent_packets_size_counter_(stats.SentPacketsSizeCounter()),
        first_sent_packet_size_(stats.FirstSentPacketSize()),
        first_packet_sent_time_(stats.FirstPacketSentTime()),
        last_packet_sent_time_(stats.LastPacketSentTime()) {}
  ~EmulatedNetworkOutgoingStatsImpl() override = default;

  int64_t PacketsSent() const override { return packets_sent_; }

  DataSize BytesSent() const override { return bytes_sent_; }

  const SamplesStatsCounter& SentPacketsSizeCounter() const override {
    return sent_packets_size_counter_;
  }

  DataSize FirstSentPacketSize() const override {
    return first_sent_packet_size_;
  }

  Timestamp FirstPacketSentTime() const override {
    return first_packet_sent_time_;
  }

  Timestamp LastPacketSentTime() const override {
    return last_packet_sent_time_;
  }

  DataRate AverageSendRate() const override;

 private:
  const int64_t packets_sent_;
  const DataSize bytes_sent_;
  const SamplesStatsCounter sent_packets_size_counter_;
  const DataSize first_sent_packet_size_;
  const Timestamp first_packet_sent_time_;
  const Timestamp last_packet_sent_time_;
};

// This class is immutable and so thread safe.
class EmulatedNetworkIncomingStatsImpl final
    : public EmulatedNetworkIncomingStats {
 public:
  EmulatedNetworkIncomingStatsImpl(
      int64_t packets_received,
      DataSize bytes_received,
      SamplesStatsCounter received_packets_size_counter,
      int64_t packets_dropped,
      DataSize bytes_dropped,
      SamplesStatsCounter dropped_packets_size_counter,
      DataSize first_received_packet_size,
      Timestamp first_packet_received_time,
      Timestamp last_packet_received_time)
      : packets_received_(packets_received),
        bytes_received_(bytes_received),
        received_packets_size_counter_(received_packets_size_counter),
        packets_dropped_(packets_dropped),
        bytes_dropped_(bytes_dropped),
        dropped_packets_size_counter_(dropped_packets_size_counter),
        first_received_packet_size_(first_received_packet_size),
        first_packet_received_time_(first_packet_received_time),
        last_packet_received_time_(last_packet_received_time) {}
  explicit EmulatedNetworkIncomingStatsImpl(
      const EmulatedNetworkIncomingStats& stats)
      : packets_received_(stats.PacketsReceived()),
        bytes_received_(stats.BytesReceived()),
        received_packets_size_counter_(stats.ReceivedPacketsSizeCounter()),
        packets_dropped_(stats.PacketsDropped()),
        bytes_dropped_(stats.BytesDropped()),
        dropped_packets_size_counter_(stats.DroppedPacketsSizeCounter()),
        first_received_packet_size_(stats.FirstReceivedPacketSize()),
        first_packet_received_time_(stats.FirstPacketReceivedTime()),
        last_packet_received_time_(stats.LastPacketReceivedTime()) {}
  ~EmulatedNetworkIncomingStatsImpl() override = default;

  int64_t PacketsReceived() const override { return packets_received_; }

  DataSize BytesReceived() const override { return bytes_received_; }

  const SamplesStatsCounter& ReceivedPacketsSizeCounter() const override {
    return received_packets_size_counter_;
  }

  int64_t PacketsDropped() const override { return packets_dropped_; }

  DataSize BytesDropped() const override { return bytes_dropped_; }

  const SamplesStatsCounter& DroppedPacketsSizeCounter() const override {
    return dropped_packets_size_counter_;
  }

  DataSize FirstReceivedPacketSize() const override {
    return first_received_packet_size_;
  }

  Timestamp FirstPacketReceivedTime() const override {
    return first_packet_received_time_;
  }

  Timestamp LastPacketReceivedTime() const override {
    return last_packet_received_time_;
  }

  DataRate AverageReceiveRate() const override;

 private:
  const int64_t packets_received_;
  const DataSize bytes_received_;
  const SamplesStatsCounter received_packets_size_counter_;
  const int64_t packets_dropped_;
  const DataSize bytes_dropped_;
  const SamplesStatsCounter dropped_packets_size_counter_;
  const DataSize first_received_packet_size_;
  const Timestamp first_packet_received_time_;
  const Timestamp last_packet_received_time_;
};

// This class is immutable and so is thread safe.
class EmulatedNetworkStatsImpl final : public EmulatedNetworkStats {
 public:
  EmulatedNetworkStatsImpl(
      std::vector<rtc::IPAddress> local_addresses,
      SamplesStatsCounter sent_packets_queue_wait_time_us,
      std::map<rtc::IPAddress, std::unique_ptr<EmulatedNetworkOutgoingStats>>
          outgoing_stats_per_destination,
      std::map<rtc::IPAddress, std::unique_ptr<EmulatedNetworkIncomingStats>>
          incoming_stats_per_source)
      : local_addresses_(std::move(local_addresses)),
        sent_packets_queue_wait_time_us_(sent_packets_queue_wait_time_us),
        outgoing_stats_per_destination_(
            std::move(outgoing_stats_per_destination)),
        incoming_stats_per_source_(std::move(incoming_stats_per_source)),
        overall_outgoing_stats_(GetOverallOutgoingStats()),
        overall_incoming_stats_(GetOverallIncomingStats()) {}
  ~EmulatedNetworkStatsImpl() override = default;

  std::vector<rtc::IPAddress> LocalAddresses() const override {
    return local_addresses_;
  }

  int64_t PacketsSent() const override {
    return overall_outgoing_stats_->PacketsSent();
  }

  DataSize BytesSent() const override {
    return overall_outgoing_stats_->BytesSent();
  }

  const SamplesStatsCounter& SentPacketsSizeCounter() const override {
    return overall_outgoing_stats_->SentPacketsSizeCounter();
  }

  const SamplesStatsCounter& SentPacketsQueueWaitTimeUs() const override {
    return sent_packets_queue_wait_time_us_;
  }

  DataSize FirstSentPacketSize() const override {
    return overall_outgoing_stats_->FirstSentPacketSize();
  }

  Timestamp FirstPacketSentTime() const override {
    return overall_outgoing_stats_->FirstPacketSentTime();
  }

  Timestamp LastPacketSentTime() const override {
    return overall_outgoing_stats_->LastPacketSentTime();
  }

  DataRate AverageSendRate() const override {
    return overall_outgoing_stats_->AverageSendRate();
  }

  int64_t PacketsReceived() const override {
    return overall_incoming_stats_->PacketsReceived();
  }

  DataSize BytesReceived() const override {
    return overall_incoming_stats_->BytesReceived();
  }

  const SamplesStatsCounter& ReceivedPacketsSizeCounter() const override {
    return overall_incoming_stats_->ReceivedPacketsSizeCounter();
  }

  int64_t PacketsDropped() const override {
    return overall_incoming_stats_->PacketsDropped();
  }

  DataSize BytesDropped() const override {
    return overall_incoming_stats_->BytesDropped();
  }

  const SamplesStatsCounter& DroppedPacketsSizeCounter() const override {
    return overall_incoming_stats_->DroppedPacketsSizeCounter();
  }

  DataSize FirstReceivedPacketSize() const override {
    return overall_incoming_stats_->FirstReceivedPacketSize();
  }

  Timestamp FirstPacketReceivedTime() const override {
    return overall_incoming_stats_->FirstPacketReceivedTime();
  }

  Timestamp LastPacketReceivedTime() const override {
    return overall_incoming_stats_->LastPacketReceivedTime();
  }

  DataRate AverageReceiveRate() const override {
    return overall_incoming_stats_->AverageReceiveRate();
  }

  std::map<rtc::IPAddress, std::unique_ptr<EmulatedNetworkOutgoingStats>>
  OutgoingStatsPerDestination() const override;

  std::map<rtc::IPAddress, std::unique_ptr<EmulatedNetworkIncomingStats>>
  IncomingStatsPerSource() const override;

 private:
  std::unique_ptr<EmulatedNetworkOutgoingStats> GetOverallOutgoingStats() const;
  std::unique_ptr<EmulatedNetworkIncomingStats> GetOverallIncomingStats() const;

  const std::vector<rtc::IPAddress> local_addresses_;
  const SamplesStatsCounter sent_packets_queue_wait_time_us_;
  const std::map<rtc::IPAddress, std::unique_ptr<EmulatedNetworkOutgoingStats>>
      outgoing_stats_per_destination_;
  const std::map<rtc::IPAddress, std::unique_ptr<EmulatedNetworkIncomingStats>>
      incoming_stats_per_source_;
  const std::unique_ptr<EmulatedNetworkOutgoingStats> overall_outgoing_stats_;
  const std::unique_ptr<EmulatedNetworkIncomingStats> overall_incoming_stats_;
};

class EmulatedNetworkOutgoingStatsBuilder {
 public:
  EmulatedNetworkOutgoingStatsBuilder();

  void OnPacketSent(Timestamp sent_time,
                    DataSize packet_size,
                    EmulatedEndpointConfig::StatsGatheringMode mode);

  void AddOutgoingStats(const EmulatedNetworkOutgoingStats& stats);

  std::unique_ptr<EmulatedNetworkOutgoingStats> Build() const;

 private:
  SequenceChecker sequence_checker_;

  int64_t packets_sent_ RTC_GUARDED_BY(sequence_checker_) = 0;
  DataSize bytes_sent_ RTC_GUARDED_BY(sequence_checker_) = DataSize::Zero();
  SamplesStatsCounter sent_packets_size_counter_
      RTC_GUARDED_BY(sequence_checker_);
  DataSize first_sent_packet_size_ RTC_GUARDED_BY(sequence_checker_) =
      DataSize::Zero();
  Timestamp first_packet_sent_time_ RTC_GUARDED_BY(sequence_checker_) =
      Timestamp::PlusInfinity();
  Timestamp last_packet_sent_time_ RTC_GUARDED_BY(sequence_checker_) =
      Timestamp::MinusInfinity();
};

class EmulatedNetworkIncomingStatsBuilder {
 public:
  EmulatedNetworkIncomingStatsBuilder();

  void OnPacketDropped(DataSize packet_size,
                       EmulatedEndpointConfig::StatsGatheringMode mode);

  void OnPacketReceived(Timestamp received_time,
                        DataSize packet_size,
                        EmulatedEndpointConfig::StatsGatheringMode mode);

  // Adds stats collected from another endpoints to the builder.
  void AddIncomingStats(const EmulatedNetworkIncomingStats& stats);

  std::unique_ptr<EmulatedNetworkIncomingStats> Build() const;

 private:
  SequenceChecker sequence_checker_;

  int64_t packets_received_ RTC_GUARDED_BY(sequence_checker_) = 0;
  DataSize bytes_received_ RTC_GUARDED_BY(sequence_checker_) = DataSize::Zero();
  SamplesStatsCounter received_packets_size_counter_
      RTC_GUARDED_BY(sequence_checker_);
  int64_t packets_dropped_ RTC_GUARDED_BY(sequence_checker_) = 0;
  DataSize bytes_dropped_ RTC_GUARDED_BY(sequence_checker_) = DataSize::Zero();
  SamplesStatsCounter dropped_packets_size_counter_
      RTC_GUARDED_BY(sequence_checker_);
  DataSize first_received_packet_size_ RTC_GUARDED_BY(sequence_checker_) =
      DataSize::Zero();
  Timestamp first_packet_received_time_ RTC_GUARDED_BY(sequence_checker_) =
      Timestamp::PlusInfinity();
  Timestamp last_packet_received_time_ RTC_GUARDED_BY(sequence_checker_) =
      Timestamp::MinusInfinity();
};

// All methods of EmulatedNetworkStatsBuilder have to be used on a single
// thread. It may be created on another thread.
class EmulatedNetworkStatsBuilder {
 public:
  EmulatedNetworkStatsBuilder();
  explicit EmulatedNetworkStatsBuilder(rtc::IPAddress local_ip);

  void OnPacketSent(Timestamp queued_time,
                    Timestamp sent_time,
                    rtc::IPAddress destination_ip,
                    DataSize packet_size,
                    EmulatedEndpointConfig::StatsGatheringMode mode);

  void OnPacketDropped(rtc::IPAddress source_ip,
                       DataSize packet_size,
                       EmulatedEndpointConfig::StatsGatheringMode mode);

  void OnPacketReceived(Timestamp received_time,
                        rtc::IPAddress source_ip,
                        DataSize packet_size,
                        EmulatedEndpointConfig::StatsGatheringMode mode);

  void AddEmulatedNetworkStats(const EmulatedNetworkStats& stats);

  std::unique_ptr<EmulatedNetworkStats> Build() const;

 private:
  SequenceChecker sequence_checker_;

  std::vector<rtc::IPAddress> local_addresses_
      RTC_GUARDED_BY(sequence_checker_);
  SamplesStatsCounter sent_packets_queue_wait_time_us_;
  std::map<rtc::IPAddress, EmulatedNetworkOutgoingStatsBuilder>
      outgoing_stats_per_destination_ RTC_GUARDED_BY(sequence_checker_);
  std::map<rtc::IPAddress, EmulatedNetworkIncomingStatsBuilder>
      incoming_stats_per_source_ RTC_GUARDED_BY(sequence_checker_);
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

  Clock* const clock_;
  rtc::TaskQueue* const task_queue_;
  const std::unique_ptr<NetworkBehaviorInterface> network_behavior_
      RTC_GUARDED_BY(task_queue_);
  EmulatedNetworkReceiverInterface* const receiver_;
  RepeatingTaskHandle process_task_ RTC_GUARDED_BY(task_queue_);
  std::deque<StoredPacket> packets_ RTC_GUARDED_BY(task_queue_);
  uint64_t next_packet_id_ RTC_GUARDED_BY(task_queue_) = 1;
};

// Represents a component responsible for routing packets based on their IP
// address. All possible routes have to be set explicitly before packet for
// desired destination will be seen for the first time. If route is unknown
// the packet will be silently dropped.
class NetworkRouterNode : public EmulatedNetworkReceiverInterface {
 public:
  explicit NetworkRouterNode(rtc::TaskQueue* task_queue);

  void OnPacketReceived(EmulatedIpPacket packet) override;
  void SetReceiver(const rtc::IPAddress& dest_ip,
                   EmulatedNetworkReceiverInterface* receiver);
  void RemoveReceiver(const rtc::IPAddress& dest_ip);
  // Sets a default receive that will be used for all incoming packets for which
  // there is no specific receiver binded to their destination port.
  void SetDefaultReceiver(EmulatedNetworkReceiverInterface* receiver);
  void RemoveDefaultReceiver();
  void SetWatcher(std::function<void(const EmulatedIpPacket&)> watcher);
  void SetFilter(std::function<bool(const EmulatedIpPacket&)> filter);

 private:
  rtc::TaskQueue* const task_queue_;
  absl::optional<EmulatedNetworkReceiverInterface*> default_receiver_
      RTC_GUARDED_BY(task_queue_);
  std::map<rtc::IPAddress, EmulatedNetworkReceiverInterface*> routing_
      RTC_GUARDED_BY(task_queue_);
  std::function<void(const EmulatedIpPacket&)> watcher_
      RTC_GUARDED_BY(task_queue_);
  std::function<bool(const EmulatedIpPacket&)> filter_
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

  EmulatedNetworkNode(const EmulatedNetworkNode&) = delete;
  EmulatedNetworkNode& operator=(const EmulatedNetworkNode&) = delete;

  void OnPacketReceived(EmulatedIpPacket packet) override;

  LinkEmulation* link() { return &link_; }
  NetworkRouterNode* router() { return &router_; }

  // Creates a route for the given receiver_ip over all the given nodes to the
  // given receiver.
  static void CreateRoute(const rtc::IPAddress& receiver_ip,
                          std::vector<EmulatedNetworkNode*> nodes,
                          EmulatedNetworkReceiverInterface* receiver);
  static void ClearRoute(const rtc::IPAddress& receiver_ip,
                         std::vector<EmulatedNetworkNode*> nodes);

 private:
  NetworkRouterNode router_;
  LinkEmulation link_;
};

// Represents single network interface on the device.
// It will be used as sender from socket side to send data to the network and
// will act as packet receiver from emulated network side to receive packets
// from other EmulatedNetworkNodes.
class EmulatedEndpointImpl : public EmulatedEndpoint {
 public:
  struct Options {
    Options(uint64_t id,
            const rtc::IPAddress& ip,
            const EmulatedEndpointConfig& config);

    // TODO(titovartem) check if we can remove id.
    uint64_t id;
    // Endpoint local IP address.
    rtc::IPAddress ip;
    EmulatedEndpointConfig::StatsGatheringMode stats_gathering_mode;
    rtc::AdapterType type;
    // Allow endpoint to send packets specifying source IP address different to
    // the current endpoint IP address. If false endpoint will crash if attempt
    // to send such packet will be done.
    bool allow_send_packet_with_different_source_ip;
    // Allow endpoint to receive packet with destination IP address different to
    // the current endpoint IP address. If false endpoint will crash if such
    // packet will arrive.
    bool allow_receive_packets_with_different_dest_ip;
    // Name of the endpoint used for logging purposes.
    std::string log_name;
  };

  EmulatedEndpointImpl(const Options& options,
                       bool is_enabled,
                       rtc::TaskQueue* task_queue,
                       Clock* clock);
  ~EmulatedEndpointImpl() override;

  uint64_t GetId() const;

  NetworkRouterNode* router() { return &router_; }

  void SendPacket(const rtc::SocketAddress& from,
                  const rtc::SocketAddress& to,
                  rtc::CopyOnWriteBuffer packet_data,
                  uint16_t application_overhead = 0) override;

  absl::optional<uint16_t> BindReceiver(
      uint16_t desired_port,
      EmulatedNetworkReceiverInterface* receiver) override;
  // Binds a receiver, and automatically removes the binding after first call to
  // OnPacketReceived.
  absl::optional<uint16_t> BindOneShotReceiver(
      uint16_t desired_port,
      EmulatedNetworkReceiverInterface* receiver);
  void UnbindReceiver(uint16_t port) override;
  void BindDefaultReceiver(EmulatedNetworkReceiverInterface* receiver) override;
  void UnbindDefaultReceiver() override;

  rtc::IPAddress GetPeerLocalAddress() const override;

  // Will be called to deliver packet into endpoint from network node.
  void OnPacketReceived(EmulatedIpPacket packet) override;

  void Enable();
  void Disable();
  bool Enabled() const;

  const rtc::Network& network() const { return *network_.get(); }

  std::unique_ptr<EmulatedNetworkStats> stats() const;

 private:
  struct ReceiverBinding {
    EmulatedNetworkReceiverInterface* receiver;
    bool is_one_shot;
  };

  absl::optional<uint16_t> BindReceiverInternal(
      uint16_t desired_port,
      EmulatedNetworkReceiverInterface* receiver,
      bool is_one_shot);

  static constexpr uint16_t kFirstEphemeralPort = 49152;
  uint16_t NextPort() RTC_EXCLUSIVE_LOCKS_REQUIRED(receiver_lock_);

  Mutex receiver_lock_;
  SequenceChecker enabled_state_checker_;

  const Options options_;
  bool is_enabled_ RTC_GUARDED_BY(enabled_state_checker_);
  Clock* const clock_;
  rtc::TaskQueue* const task_queue_;
  std::unique_ptr<rtc::Network> network_;
  NetworkRouterNode router_;

  uint16_t next_port_ RTC_GUARDED_BY(receiver_lock_);
  absl::optional<EmulatedNetworkReceiverInterface*> default_receiver_
      RTC_GUARDED_BY(receiver_lock_);
  std::map<uint16_t, ReceiverBinding> port_to_receiver_
      RTC_GUARDED_BY(receiver_lock_);

  EmulatedNetworkStatsBuilder stats_builder_ RTC_GUARDED_BY(task_queue_);
};

class EmulatedRoute {
 public:
  EmulatedRoute(EmulatedEndpointImpl* from,
                std::vector<EmulatedNetworkNode*> via_nodes,
                EmulatedEndpointImpl* to,
                bool is_default)
      : from(from),
        via_nodes(std::move(via_nodes)),
        to(to),
        active(true),
        is_default(is_default) {}

  EmulatedEndpointImpl* from;
  std::vector<EmulatedNetworkNode*> via_nodes;
  EmulatedEndpointImpl* to;
  bool active;
  bool is_default;
};

// This object is immutable and so thread safe.
class EndpointsContainer {
 public:
  explicit EndpointsContainer(
      const std::vector<EmulatedEndpointImpl*>& endpoints);

  EmulatedEndpointImpl* LookupByLocalAddress(
      const rtc::IPAddress& local_ip) const;
  bool HasEndpoint(EmulatedEndpointImpl* endpoint) const;
  // Returns list of networks for enabled endpoints. Caller takes ownership of
  // returned rtc::Network objects.
  std::vector<std::unique_ptr<rtc::Network>> GetEnabledNetworks() const;
  std::vector<EmulatedEndpoint*> GetEndpoints() const;
  std::unique_ptr<EmulatedNetworkStats> GetStats() const;

 private:
  const std::vector<EmulatedEndpointImpl*> endpoints_;
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

  ~FakePacketRoute() { route_->to->UnbindReceiver(recv_addr_.port()); }

  void SendPacket(size_t size, FakePacketType packet) {
    RTC_CHECK_GE(size, sizeof(int));
    sent_.emplace(next_packet_id_, packet);
    rtc::CopyOnWriteBuffer buf(size);
    reinterpret_cast<int*>(buf.MutableData())[0] = next_packet_id_++;
    route_->from->SendPacket(send_addr_, recv_addr_, buf);
  }

  void OnPacketReceived(EmulatedIpPacket packet) override {
    int packet_id = reinterpret_cast<const int*>(packet.data.data())[0];
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
