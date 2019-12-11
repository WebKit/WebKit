/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/scenario/network_node.h"

#include <algorithm>
#include <vector>

#include "absl/memory/memory.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {
namespace test {
namespace {
constexpr char kDummyTransportName[] = "dummy";
SimulatedNetwork::Config CreateSimulationConfig(
    NetworkSimulationConfig config) {
  SimulatedNetwork::Config sim_config;
  sim_config.link_capacity_kbps = config.bandwidth.kbps_or(0);
  sim_config.loss_percent = config.loss_rate * 100;
  sim_config.queue_delay_ms = config.delay.ms();
  sim_config.delay_standard_deviation_ms = config.delay_std_dev.ms();
  sim_config.packet_overhead = config.packet_overhead.bytes<int>();
  sim_config.codel_active_queue_management =
      config.codel_active_queue_management;
  return sim_config;
}
}  // namespace

SimulationNode::SimulationNode(NetworkSimulationConfig config,
                               SimulatedNetwork* behavior,
                               EmulatedNetworkNode* network_node)
    : config_(config), simulation_(behavior), network_node_(network_node) {}

std::unique_ptr<SimulatedNetwork> SimulationNode::CreateBehavior(
    NetworkSimulationConfig config) {
  SimulatedNetwork::Config sim_config = CreateSimulationConfig(config);
  return absl::make_unique<SimulatedNetwork>(sim_config);
}

void SimulationNode::UpdateConfig(
    std::function<void(NetworkSimulationConfig*)> modifier) {
  modifier(&config_);
  SimulatedNetwork::Config sim_config = CreateSimulationConfig(config_);
  simulation_->SetConfig(sim_config);
}

void SimulationNode::PauseTransmissionUntil(Timestamp until) {
  simulation_->PauseTransmissionUntil(until.us());
}

ColumnPrinter SimulationNode::ConfigPrinter() const {
  return ColumnPrinter::Lambda(
      "propagation_delay capacity loss_rate",
      [this](rtc::SimpleStringBuilder& sb) {
        sb.AppendFormat("%.3lf %.0lf %.2lf", config_.delay.seconds<double>(),
                        config_.bandwidth.bps() / 8.0, config_.loss_rate);
      });
}

NetworkNodeTransport::NetworkNodeTransport(Clock* sender_clock,
                                           Call* sender_call)
    : sender_clock_(sender_clock), sender_call_(sender_call) {}

NetworkNodeTransport::~NetworkNodeTransport() = default;

bool NetworkNodeTransport::SendRtp(const uint8_t* packet,
                                   size_t length,
                                   const PacketOptions& options) {
  int64_t send_time_ms = sender_clock_->TimeInMilliseconds();
  rtc::SentPacket sent_packet;
  sent_packet.packet_id = options.packet_id;
  sent_packet.info.included_in_feedback = options.included_in_feedback;
  sent_packet.info.included_in_allocation = options.included_in_allocation;
  sent_packet.send_time_ms = send_time_ms;
  sent_packet.info.packet_size_bytes = length;
  sent_packet.info.packet_type = rtc::PacketType::kData;
  sender_call_->OnSentPacket(sent_packet);

  Timestamp send_time = Timestamp::ms(send_time_ms);
  rtc::CritScope crit(&crit_sect_);
  if (!send_net_)
    return false;
  rtc::CopyOnWriteBuffer buffer(packet, length,
                                length + packet_overhead_.bytes());
  buffer.SetSize(length + packet_overhead_.bytes());
  send_net_->OnPacketReceived(
      EmulatedIpPacket(local_address_, receiver_address_, buffer, send_time));
  return true;
}

bool NetworkNodeTransport::SendRtcp(const uint8_t* packet, size_t length) {
  rtc::CopyOnWriteBuffer buffer(packet, length);
  Timestamp send_time = sender_clock_->CurrentTime();
  rtc::CritScope crit(&crit_sect_);
  buffer.SetSize(length + packet_overhead_.bytes());
  if (!send_net_)
    return false;
  send_net_->OnPacketReceived(
      EmulatedIpPacket(local_address_, receiver_address_, buffer, send_time));
  return true;
}

void NetworkNodeTransport::Connect(EmulatedNetworkNode* send_node,
                                   rtc::IPAddress receiver_ip,
                                   DataSize packet_overhead) {
  rtc::NetworkRoute route;
  route.connected = true;
  route.local_network_id =
      static_cast<uint16_t>(receiver_ip.v4AddressAsHostOrderInteger());
  route.remote_network_id =
      static_cast<uint16_t>(receiver_ip.v4AddressAsHostOrderInteger());
  {
    // Only IPv4 address is supported. We don't use full range of IPs in
    // scenario framework and also we need a simple way to convert IP into
    // network_id to signal network route.
    RTC_CHECK_EQ(receiver_ip.family(), AF_INET);
    RTC_CHECK_LE(receiver_ip.v4AddressAsHostOrderInteger(),
                 std::numeric_limits<uint16_t>::max());
    rtc::CritScope crit(&crit_sect_);
    send_net_ = send_node;
    receiver_address_ = rtc::SocketAddress(receiver_ip, 0);
    packet_overhead_ = packet_overhead;
    current_network_route_ = route;
  }

  sender_call_->GetTransportControllerSend()->OnNetworkRouteChanged(
      kDummyTransportName, route);
}

void NetworkNodeTransport::Disconnect() {
  rtc::CritScope crit(&crit_sect_);
  current_network_route_.connected = false;
  sender_call_->GetTransportControllerSend()->OnNetworkRouteChanged(
      kDummyTransportName, current_network_route_);
  current_network_route_ = {};
  send_net_ = nullptr;
}

}  // namespace test
}  // namespace webrtc
