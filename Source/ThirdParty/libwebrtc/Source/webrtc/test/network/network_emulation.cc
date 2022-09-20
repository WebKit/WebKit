/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/network/network_emulation.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "absl/types/optional.h"
#include "api/numerics/samples_stats_counter.h"
#include "api/test/network_emulation/network_emulation_interfaces.h"
#include "api/units/data_size.h"
#include "rtc_base/logging.h"

namespace webrtc {

DataRate EmulatedNetworkOutgoingStatsImpl::AverageSendRate() const {
  RTC_DCHECK_GE(packets_sent_, 2);
  RTC_DCHECK(first_packet_sent_time_.IsFinite());
  RTC_DCHECK(last_packet_sent_time_.IsFinite());
  return (bytes_sent_ - first_sent_packet_size_) /
         (last_packet_sent_time_ - first_packet_sent_time_);
}

DataRate EmulatedNetworkIncomingStatsImpl::AverageReceiveRate() const {
  RTC_DCHECK_GE(packets_received_, 2);
  RTC_DCHECK(first_packet_received_time_.IsFinite());
  RTC_DCHECK(last_packet_received_time_.IsFinite());
  return (bytes_received_ - first_received_packet_size_) /
         (last_packet_received_time_ - first_packet_received_time_);
}

std::map<rtc::IPAddress, std::unique_ptr<EmulatedNetworkOutgoingStats>>
EmulatedNetworkStatsImpl::OutgoingStatsPerDestination() const {
  std::map<rtc::IPAddress, std::unique_ptr<EmulatedNetworkOutgoingStats>> out;
  for (const auto& entry : outgoing_stats_per_destination_) {
    out.emplace(entry.first, std::make_unique<EmulatedNetworkOutgoingStatsImpl>(
                                 *entry.second));
  }
  return out;
}

std::map<rtc::IPAddress, std::unique_ptr<EmulatedNetworkIncomingStats>>
EmulatedNetworkStatsImpl::IncomingStatsPerSource() const {
  std::map<rtc::IPAddress, std::unique_ptr<EmulatedNetworkIncomingStats>> out;
  for (const auto& entry : incoming_stats_per_source_) {
    out.emplace(entry.first, std::make_unique<EmulatedNetworkIncomingStatsImpl>(
                                 *entry.second));
  }
  return out;
}

std::unique_ptr<EmulatedNetworkOutgoingStats>
EmulatedNetworkStatsImpl::GetOverallOutgoingStats() const {
  EmulatedNetworkOutgoingStatsBuilder builder;
  for (const auto& entry : outgoing_stats_per_destination_) {
    builder.AddOutgoingStats(*entry.second);
  }
  return builder.Build();
}

std::unique_ptr<EmulatedNetworkIncomingStats>
EmulatedNetworkStatsImpl::GetOverallIncomingStats() const {
  EmulatedNetworkIncomingStatsBuilder builder;
  for (const auto& entry : incoming_stats_per_source_) {
    builder.AddIncomingStats(*entry.second);
  }
  return builder.Build();
}

EmulatedNetworkOutgoingStatsBuilder::EmulatedNetworkOutgoingStatsBuilder() {
  sequence_checker_.Detach();
}

void EmulatedNetworkOutgoingStatsBuilder::OnPacketSent(
    Timestamp sent_time,
    DataSize packet_size,
    EmulatedEndpointConfig::StatsGatheringMode mode) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_CHECK_GE(packet_size, DataSize::Zero());
  if (first_packet_sent_time_.IsInfinite()) {
    first_packet_sent_time_ = sent_time;
    first_sent_packet_size_ = packet_size;
  }
  last_packet_sent_time_ = sent_time;
  packets_sent_++;
  bytes_sent_ += packet_size;
  if (mode == EmulatedEndpointConfig::StatsGatheringMode::kDebug) {
    sent_packets_size_counter_.AddSample(packet_size.bytes());
  }
}

void EmulatedNetworkOutgoingStatsBuilder::AddOutgoingStats(
    const EmulatedNetworkOutgoingStats& stats) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  packets_sent_ += stats.PacketsSent();
  bytes_sent_ += stats.BytesSent();
  sent_packets_size_counter_.AddSamples(stats.SentPacketsSizeCounter());
  if (first_packet_sent_time_ > stats.FirstPacketSentTime()) {
    first_packet_sent_time_ = stats.FirstPacketSentTime();
    first_sent_packet_size_ = stats.FirstSentPacketSize();
  }
  if (last_packet_sent_time_ < stats.LastPacketSentTime()) {
    last_packet_sent_time_ = stats.LastPacketSentTime();
  }
}

std::unique_ptr<EmulatedNetworkOutgoingStats>
EmulatedNetworkOutgoingStatsBuilder::Build() const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return std::make_unique<EmulatedNetworkOutgoingStatsImpl>(
      packets_sent_, bytes_sent_, sent_packets_size_counter_,
      first_sent_packet_size_, first_packet_sent_time_, last_packet_sent_time_);
}

EmulatedNetworkIncomingStatsBuilder::EmulatedNetworkIncomingStatsBuilder() {
  sequence_checker_.Detach();
}

void EmulatedNetworkIncomingStatsBuilder::OnPacketDropped(
    DataSize packet_size,
    EmulatedEndpointConfig::StatsGatheringMode mode) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  packets_dropped_++;
  bytes_dropped_ += packet_size;
  if (mode == EmulatedEndpointConfig::StatsGatheringMode::kDebug) {
    dropped_packets_size_counter_.AddSample(packet_size.bytes());
  }
}

void EmulatedNetworkIncomingStatsBuilder::OnPacketReceived(
    Timestamp received_time,
    DataSize packet_size,
    EmulatedEndpointConfig::StatsGatheringMode mode) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_CHECK_GE(packet_size, DataSize::Zero());
  if (first_packet_received_time_.IsInfinite()) {
    first_packet_received_time_ = received_time;
    first_received_packet_size_ = packet_size;
  }
  last_packet_received_time_ = received_time;
  packets_received_++;
  bytes_received_ += packet_size;
  if (mode == EmulatedEndpointConfig::StatsGatheringMode::kDebug) {
    received_packets_size_counter_.AddSample(packet_size.bytes());
  }
}

void EmulatedNetworkIncomingStatsBuilder::AddIncomingStats(
    const EmulatedNetworkIncomingStats& stats) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  packets_received_ += stats.PacketsReceived();
  bytes_received_ += stats.BytesReceived();
  received_packets_size_counter_.AddSamples(stats.ReceivedPacketsSizeCounter());
  packets_dropped_ += stats.PacketsDropped();
  bytes_dropped_ += stats.BytesDropped();
  dropped_packets_size_counter_.AddSamples(stats.DroppedPacketsSizeCounter());
  if (first_packet_received_time_ > stats.FirstPacketReceivedTime()) {
    first_packet_received_time_ = stats.FirstPacketReceivedTime();
    first_received_packet_size_ = stats.FirstReceivedPacketSize();
  }
  if (last_packet_received_time_ < stats.LastPacketReceivedTime()) {
    last_packet_received_time_ = stats.LastPacketReceivedTime();
  }
}

std::unique_ptr<EmulatedNetworkIncomingStats>
EmulatedNetworkIncomingStatsBuilder::Build() const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return std::make_unique<EmulatedNetworkIncomingStatsImpl>(
      packets_received_, bytes_received_, received_packets_size_counter_,
      packets_dropped_, bytes_dropped_, dropped_packets_size_counter_,
      first_received_packet_size_, first_packet_received_time_,
      last_packet_received_time_);
}

EmulatedNetworkStatsBuilder::EmulatedNetworkStatsBuilder() {
  sequence_checker_.Detach();
}

EmulatedNetworkStatsBuilder::EmulatedNetworkStatsBuilder(
    rtc::IPAddress local_ip) {
  local_addresses_.push_back(local_ip);
  sequence_checker_.Detach();
}

void EmulatedNetworkStatsBuilder::OnPacketSent(
    Timestamp queued_time,
    Timestamp sent_time,
    rtc::IPAddress destination_ip,
    DataSize packet_size,
    EmulatedEndpointConfig::StatsGatheringMode mode) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (mode == EmulatedEndpointConfig::StatsGatheringMode::kDebug) {
    sent_packets_queue_wait_time_us_.AddSample((sent_time - queued_time).us());
  }
  outgoing_stats_per_destination_[destination_ip].OnPacketSent(
      sent_time, packet_size, mode);
}

void EmulatedNetworkStatsBuilder::OnPacketDropped(
    rtc::IPAddress source_ip,
    DataSize packet_size,
    EmulatedEndpointConfig::StatsGatheringMode mode) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  incoming_stats_per_source_[source_ip].OnPacketDropped(packet_size, mode);
}

void EmulatedNetworkStatsBuilder::OnPacketReceived(
    Timestamp received_time,
    rtc::IPAddress source_ip,
    DataSize packet_size,
    EmulatedEndpointConfig::StatsGatheringMode mode) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  incoming_stats_per_source_[source_ip].OnPacketReceived(received_time,
                                                         packet_size, mode);
}

void EmulatedNetworkStatsBuilder::AddEmulatedNetworkStats(
    const EmulatedNetworkStats& stats) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  // Append IPs from other endpoints stats to the builder.
  for (const rtc::IPAddress& addr : stats.LocalAddresses()) {
    local_addresses_.push_back(addr);
  }

  sent_packets_queue_wait_time_us_.AddSamples(
      stats.SentPacketsQueueWaitTimeUs());

  // Add outgoing stats from other endpoints to the builder.
  const std::map<rtc::IPAddress, std::unique_ptr<EmulatedNetworkOutgoingStats>>
      outgoing_stats_per_destination = stats.OutgoingStatsPerDestination();
  for (const auto& entry : outgoing_stats_per_destination) {
    outgoing_stats_per_destination_[entry.first].AddOutgoingStats(
        *entry.second);
  }

  // Add incoming stats from other endpoints to the builder.
  const std::map<rtc::IPAddress, std::unique_ptr<EmulatedNetworkIncomingStats>>
      incoming_stats_per_source = stats.IncomingStatsPerSource();
  for (const auto& entry : incoming_stats_per_source) {
    incoming_stats_per_source_[entry.first].AddIncomingStats(*entry.second);
  }
}

std::unique_ptr<EmulatedNetworkStats> EmulatedNetworkStatsBuilder::Build()
    const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  std::map<rtc::IPAddress, std::unique_ptr<EmulatedNetworkOutgoingStats>>
      outgoing_stats;
  for (const auto& entry : outgoing_stats_per_destination_) {
    outgoing_stats.emplace(entry.first, entry.second.Build());
  }
  std::map<rtc::IPAddress, std::unique_ptr<EmulatedNetworkIncomingStats>>
      incoming_stats;
  for (const auto& entry : incoming_stats_per_source_) {
    incoming_stats.emplace(entry.first, entry.second.Build());
  }
  return std::make_unique<EmulatedNetworkStatsImpl>(
      local_addresses_, sent_packets_queue_wait_time_us_,
      std::move(outgoing_stats), std::move(incoming_stats));
}

void LinkEmulation::OnPacketReceived(EmulatedIpPacket packet) {
  task_queue_->PostTask([this, packet = std::move(packet)]() mutable {
    RTC_DCHECK_RUN_ON(task_queue_);

    uint64_t packet_id = next_packet_id_++;
    bool sent = network_behavior_->EnqueuePacket(PacketInFlightInfo(
        packet.ip_packet_size(), packet.arrival_time.us(), packet_id));
    if (sent) {
      packets_.emplace_back(StoredPacket{packet_id, std::move(packet), false});
    }
    if (process_task_.Running())
      return;
    absl::optional<int64_t> next_time_us =
        network_behavior_->NextDeliveryTimeUs();
    if (!next_time_us)
      return;
    Timestamp current_time = clock_->CurrentTime();
    process_task_ = RepeatingTaskHandle::DelayedStart(
        task_queue_->Get(),
        std::max(TimeDelta::Zero(),
                 Timestamp::Micros(*next_time_us) - current_time),
        [this]() {
          RTC_DCHECK_RUN_ON(task_queue_);
          Timestamp current_time = clock_->CurrentTime();
          Process(current_time);
          absl::optional<int64_t> next_time_us =
              network_behavior_->NextDeliveryTimeUs();
          if (!next_time_us) {
            process_task_.Stop();
            return TimeDelta::Zero();  // This is ignored.
          }
          RTC_DCHECK_GE(*next_time_us, current_time.us());
          return Timestamp::Micros(*next_time_us) - current_time;
        });
  });
}

void LinkEmulation::Process(Timestamp at_time) {
  std::vector<PacketDeliveryInfo> delivery_infos =
      network_behavior_->DequeueDeliverablePackets(at_time.us());
  for (PacketDeliveryInfo& delivery_info : delivery_infos) {
    StoredPacket* packet = nullptr;
    for (auto& stored_packet : packets_) {
      if (stored_packet.id == delivery_info.packet_id) {
        packet = &stored_packet;
        break;
      }
    }
    RTC_CHECK(packet);
    RTC_DCHECK(!packet->removed);
    packet->removed = true;

    if (delivery_info.receive_time_us != PacketDeliveryInfo::kNotReceived) {
      packet->packet.arrival_time =
          Timestamp::Micros(delivery_info.receive_time_us);
      receiver_->OnPacketReceived(std::move(packet->packet));
    }
    while (!packets_.empty() && packets_.front().removed) {
      packets_.pop_front();
    }
  }
}

NetworkRouterNode::NetworkRouterNode(rtc::TaskQueue* task_queue)
    : task_queue_(task_queue) {}

void NetworkRouterNode::OnPacketReceived(EmulatedIpPacket packet) {
  RTC_DCHECK_RUN_ON(task_queue_);
  if (watcher_) {
    watcher_(packet);
  }
  if (filter_) {
    if (!filter_(packet))
      return;
  }
  auto receiver_it = routing_.find(packet.to.ipaddr());
  if (receiver_it == routing_.end()) {
    if (default_receiver_.has_value()) {
      (*default_receiver_)->OnPacketReceived(std::move(packet));
    }
    return;
  }
  RTC_CHECK(receiver_it != routing_.end());

  receiver_it->second->OnPacketReceived(std::move(packet));
}

void NetworkRouterNode::SetReceiver(
    const rtc::IPAddress& dest_ip,
    EmulatedNetworkReceiverInterface* receiver) {
  task_queue_->PostTask([=] {
    RTC_DCHECK_RUN_ON(task_queue_);
    EmulatedNetworkReceiverInterface* cur_receiver = routing_[dest_ip];
    RTC_CHECK(cur_receiver == nullptr || cur_receiver == receiver)
        << "Routing for dest_ip=" << dest_ip.ToString() << " already exists";
    routing_[dest_ip] = receiver;
  });
}

void NetworkRouterNode::RemoveReceiver(const rtc::IPAddress& dest_ip) {
  RTC_DCHECK_RUN_ON(task_queue_);
  routing_.erase(dest_ip);
}

void NetworkRouterNode::SetDefaultReceiver(
    EmulatedNetworkReceiverInterface* receiver) {
  task_queue_->PostTask([=] {
    RTC_DCHECK_RUN_ON(task_queue_);
    if (default_receiver_.has_value()) {
      RTC_CHECK_EQ(*default_receiver_, receiver)
          << "Router already default receiver";
    }
    default_receiver_ = receiver;
  });
}

void NetworkRouterNode::RemoveDefaultReceiver() {
  RTC_DCHECK_RUN_ON(task_queue_);
  default_receiver_ = absl::nullopt;
}

void NetworkRouterNode::SetWatcher(
    std::function<void(const EmulatedIpPacket&)> watcher) {
  task_queue_->PostTask([=] {
    RTC_DCHECK_RUN_ON(task_queue_);
    watcher_ = watcher;
  });
}

void NetworkRouterNode::SetFilter(
    std::function<bool(const EmulatedIpPacket&)> filter) {
  task_queue_->PostTask([=] {
    RTC_DCHECK_RUN_ON(task_queue_);
    filter_ = filter;
  });
}

EmulatedNetworkNode::EmulatedNetworkNode(
    Clock* clock,
    rtc::TaskQueue* task_queue,
    std::unique_ptr<NetworkBehaviorInterface> network_behavior)
    : router_(task_queue),
      link_(clock, task_queue, std::move(network_behavior), &router_) {}

void EmulatedNetworkNode::OnPacketReceived(EmulatedIpPacket packet) {
  link_.OnPacketReceived(std::move(packet));
}

void EmulatedNetworkNode::CreateRoute(
    const rtc::IPAddress& receiver_ip,
    std::vector<EmulatedNetworkNode*> nodes,
    EmulatedNetworkReceiverInterface* receiver) {
  RTC_CHECK(!nodes.empty());
  for (size_t i = 0; i + 1 < nodes.size(); ++i)
    nodes[i]->router()->SetReceiver(receiver_ip, nodes[i + 1]);
  nodes.back()->router()->SetReceiver(receiver_ip, receiver);
}

void EmulatedNetworkNode::ClearRoute(const rtc::IPAddress& receiver_ip,
                                     std::vector<EmulatedNetworkNode*> nodes) {
  for (EmulatedNetworkNode* node : nodes)
    node->router()->RemoveReceiver(receiver_ip);
}

EmulatedNetworkNode::~EmulatedNetworkNode() = default;

EmulatedEndpointImpl::Options::Options(uint64_t id,
                                       const rtc::IPAddress& ip,
                                       const EmulatedEndpointConfig& config)
    : id(id),
      ip(ip),
      stats_gathering_mode(config.stats_gathering_mode),
      type(config.type),
      allow_send_packet_with_different_source_ip(
          config.allow_send_packet_with_different_source_ip),
      allow_receive_packets_with_different_dest_ip(
          config.allow_receive_packets_with_different_dest_ip),
      log_name(ip.ToString() + " (" + config.name.value_or("") + ")") {}

EmulatedEndpointImpl::EmulatedEndpointImpl(const Options& options,
                                           bool is_enabled,
                                           rtc::TaskQueue* task_queue,
                                           Clock* clock)
    : options_(options),
      is_enabled_(is_enabled),
      clock_(clock),
      task_queue_(task_queue),
      router_(task_queue_),
      next_port_(kFirstEphemeralPort),
      stats_builder_(options_.ip) {
  constexpr int kIPv4NetworkPrefixLength = 24;
  constexpr int kIPv6NetworkPrefixLength = 64;

  int prefix_length = 0;
  if (options_.ip.family() == AF_INET) {
    prefix_length = kIPv4NetworkPrefixLength;
  } else if (options_.ip.family() == AF_INET6) {
    prefix_length = kIPv6NetworkPrefixLength;
  }
  rtc::IPAddress prefix = TruncateIP(options_.ip, prefix_length);
  network_ = std::make_unique<rtc::Network>(
      options_.ip.ToString(), "Endpoint id=" + std::to_string(options_.id),
      prefix, prefix_length, options_.type);
  network_->AddIP(options_.ip);

  enabled_state_checker_.Detach();
  RTC_LOG(LS_INFO) << "Created emulated endpoint " << options_.log_name
                   << "; id=" << options_.id;
}
EmulatedEndpointImpl::~EmulatedEndpointImpl() = default;

uint64_t EmulatedEndpointImpl::GetId() const {
  return options_.id;
}

void EmulatedEndpointImpl::SendPacket(const rtc::SocketAddress& from,
                                      const rtc::SocketAddress& to,
                                      rtc::CopyOnWriteBuffer packet_data,
                                      uint16_t application_overhead) {
  if (!options_.allow_send_packet_with_different_source_ip) {
    RTC_CHECK(from.ipaddr() == options_.ip);
  }
  EmulatedIpPacket packet(from, to, std::move(packet_data),
                          clock_->CurrentTime(), application_overhead);
  task_queue_->PostTask([this, packet = std::move(packet)]() mutable {
    RTC_DCHECK_RUN_ON(task_queue_);
    stats_builder_.OnPacketSent(packet.arrival_time, clock_->CurrentTime(),
                                packet.to.ipaddr(),
                                DataSize::Bytes(packet.ip_packet_size()),
                                options_.stats_gathering_mode);

    if (packet.to.ipaddr() == options_.ip) {
      OnPacketReceived(std::move(packet));
    } else {
      router_.OnPacketReceived(std::move(packet));
    }
  });
}

absl::optional<uint16_t> EmulatedEndpointImpl::BindReceiver(
    uint16_t desired_port,
    EmulatedNetworkReceiverInterface* receiver) {
  return BindReceiverInternal(desired_port, receiver, /*is_one_shot=*/false);
}

absl::optional<uint16_t> EmulatedEndpointImpl::BindOneShotReceiver(
    uint16_t desired_port,
    EmulatedNetworkReceiverInterface* receiver) {
  return BindReceiverInternal(desired_port, receiver, /*is_one_shot=*/true);
}

absl::optional<uint16_t> EmulatedEndpointImpl::BindReceiverInternal(
    uint16_t desired_port,
    EmulatedNetworkReceiverInterface* receiver,
    bool is_one_shot) {
  MutexLock lock(&receiver_lock_);
  uint16_t port = desired_port;
  if (port == 0) {
    // Because client can specify its own port, next_port_ can be already in
    // use, so we need to find next available port.
    int ports_pool_size =
        std::numeric_limits<uint16_t>::max() - kFirstEphemeralPort + 1;
    for (int i = 0; i < ports_pool_size; ++i) {
      uint16_t next_port = NextPort();
      if (port_to_receiver_.find(next_port) == port_to_receiver_.end()) {
        port = next_port;
        break;
      }
    }
  }
  RTC_CHECK(port != 0) << "Can't find free port for receiver in endpoint "
                       << options_.log_name << "; id=" << options_.id;
  bool result =
      port_to_receiver_.insert({port, {receiver, is_one_shot}}).second;
  if (!result) {
    RTC_LOG(LS_INFO) << "Can't bind receiver to used port " << desired_port
                     << " in endpoint " << options_.log_name
                     << "; id=" << options_.id;
    return absl::nullopt;
  }
  RTC_LOG(LS_INFO) << "New receiver is binded to endpoint " << options_.log_name
                   << "; id=" << options_.id << " on port " << port;
  return port;
}

uint16_t EmulatedEndpointImpl::NextPort() {
  uint16_t out = next_port_;
  if (next_port_ == std::numeric_limits<uint16_t>::max()) {
    next_port_ = kFirstEphemeralPort;
  } else {
    next_port_++;
  }
  return out;
}

void EmulatedEndpointImpl::UnbindReceiver(uint16_t port) {
  MutexLock lock(&receiver_lock_);
  RTC_LOG(LS_INFO) << "Receiver is removed on port " << port
                   << " from endpoint " << options_.log_name
                   << "; id=" << options_.id;
  port_to_receiver_.erase(port);
}

void EmulatedEndpointImpl::BindDefaultReceiver(
    EmulatedNetworkReceiverInterface* receiver) {
  MutexLock lock(&receiver_lock_);
  RTC_CHECK(!default_receiver_.has_value())
      << "Endpoint " << options_.log_name << "; id=" << options_.id
      << " already has default receiver";
  RTC_LOG(LS_INFO) << "Default receiver is binded to endpoint "
                   << options_.log_name << "; id=" << options_.id;
  default_receiver_ = receiver;
}

void EmulatedEndpointImpl::UnbindDefaultReceiver() {
  MutexLock lock(&receiver_lock_);
  RTC_LOG(LS_INFO) << "Default receiver is removed from endpoint "
                   << options_.log_name << "; id=" << options_.id;
  default_receiver_ = absl::nullopt;
}

rtc::IPAddress EmulatedEndpointImpl::GetPeerLocalAddress() const {
  return options_.ip;
}

void EmulatedEndpointImpl::OnPacketReceived(EmulatedIpPacket packet) {
  RTC_DCHECK_RUN_ON(task_queue_);
  if (!options_.allow_receive_packets_with_different_dest_ip) {
    RTC_CHECK(packet.to.ipaddr() == options_.ip)
        << "Routing error: wrong destination endpoint. Packet.to.ipaddr()=: "
        << packet.to.ipaddr().ToString()
        << "; Receiver options_.ip=" << options_.ip.ToString();
  }
  MutexLock lock(&receiver_lock_);
  stats_builder_.OnPacketReceived(clock_->CurrentTime(), packet.from.ipaddr(),
                                  DataSize::Bytes(packet.ip_packet_size()),
                                  options_.stats_gathering_mode);
  auto it = port_to_receiver_.find(packet.to.port());
  if (it == port_to_receiver_.end()) {
    if (default_receiver_.has_value()) {
      (*default_receiver_)->OnPacketReceived(std::move(packet));
      return;
    }
    // It can happen, that remote peer closed connection, but there still some
    // packets, that are going to it. It can happen during peer connection close
    // process: one peer closed connection, second still sending data.
    RTC_LOG(LS_INFO) << "Drop packet: no receiver registered in "
                     << options_.log_name << "; id=" << options_.id
                     << " on port " << packet.to.port();
    stats_builder_.OnPacketDropped(packet.from.ipaddr(),
                                   DataSize::Bytes(packet.ip_packet_size()),
                                   options_.stats_gathering_mode);
    return;
  }
  // Endpoint holds lock during packet processing to ensure that a call to
  // UnbindReceiver followed by a delete of the receiver cannot race with this
  // call to OnPacketReceived.
  it->second.receiver->OnPacketReceived(std::move(packet));

  if (it->second.is_one_shot) {
    port_to_receiver_.erase(it);
  }
}

void EmulatedEndpointImpl::Enable() {
  RTC_DCHECK_RUN_ON(&enabled_state_checker_);
  RTC_CHECK(!is_enabled_);
  is_enabled_ = true;
}

void EmulatedEndpointImpl::Disable() {
  RTC_DCHECK_RUN_ON(&enabled_state_checker_);
  RTC_CHECK(is_enabled_);
  is_enabled_ = false;
}

bool EmulatedEndpointImpl::Enabled() const {
  RTC_DCHECK_RUN_ON(&enabled_state_checker_);
  return is_enabled_;
}

std::unique_ptr<EmulatedNetworkStats> EmulatedEndpointImpl::stats() const {
  RTC_DCHECK_RUN_ON(task_queue_);
  return stats_builder_.Build();
}

EndpointsContainer::EndpointsContainer(
    const std::vector<EmulatedEndpointImpl*>& endpoints)
    : endpoints_(endpoints) {}

EmulatedEndpointImpl* EndpointsContainer::LookupByLocalAddress(
    const rtc::IPAddress& local_ip) const {
  for (auto* endpoint : endpoints_) {
    rtc::IPAddress peer_local_address = endpoint->GetPeerLocalAddress();
    if (peer_local_address == local_ip) {
      return endpoint;
    }
  }
  RTC_CHECK(false) << "No network found for address" << local_ip.ToString();
}

bool EndpointsContainer::HasEndpoint(EmulatedEndpointImpl* endpoint) const {
  for (auto* e : endpoints_) {
    if (e->GetId() == endpoint->GetId()) {
      return true;
    }
  }
  return false;
}

std::vector<std::unique_ptr<rtc::Network>>
EndpointsContainer::GetEnabledNetworks() const {
  std::vector<std::unique_ptr<rtc::Network>> networks;
  for (auto* endpoint : endpoints_) {
    if (endpoint->Enabled()) {
      networks.emplace_back(
          std::make_unique<rtc::Network>(endpoint->network()));
    }
  }
  return networks;
}

std::vector<EmulatedEndpoint*> EndpointsContainer::GetEndpoints() const {
  return std::vector<EmulatedEndpoint*>(endpoints_.begin(), endpoints_.end());
}

std::unique_ptr<EmulatedNetworkStats> EndpointsContainer::GetStats() const {
  EmulatedNetworkStatsBuilder stats_builder;
  for (auto* endpoint : endpoints_) {
    stats_builder.AddEmulatedNetworkStats(*endpoint->stats());
  }
  return stats_builder.Build();
}

}  // namespace webrtc
