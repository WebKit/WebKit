/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/network/feedback_generator.h"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/rtc_event_log/rtc_event_log_factory.h"
#include "api/rtp_parameters.h"
#include "api/test/create_network_emulation_manager.h"
#include "api/test/network_emulation_manager.h"
#include "api/test/simulated_network.h"
#include "api/transport/network_types.h"
#include "api/transport/test/feedback_generator_interface.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/events/rtc_event_rtcp_packet_incoming.h"
#include "logging/rtc_event_log/events/rtc_event_rtp_packet_outgoing.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/checks.h"
#include "test/network/network_emulation.h"
#include "test/network/simulated_network.h"
namespace webrtc {


namespace {
Environment GetEnvironment(NetworkEmulationManager& net) {
  EnvironmentFactory factory;
  factory.Set(net.time_controller()->GetClock());
  factory.Set(net.time_controller()->GetTaskQueueFactory());
  return factory.Create();
}

EmulatedRoute* CreateRoute(NetworkEmulationManager& net,
                           std::vector<EmulatedNetworkNode*> link) {
  if (!link.empty()) {
    return net.CreateRoute(link);
  } else {
    return net.CreateRoute({net.CreateUnconstrainedEmulatedNode()});
  }
}

}  // namespace

FeedbackGeneratorWithoutNetworkImpl::FeedbackGeneratorWithoutNetworkImpl(
    Config config,
    NetworkEmulationManager& net)
    : net_(net),
      feedback_interval_(config.feedback_interval),
      feedback_packet_size_(config.feedback_packet_size),
      event_log_(RtcEventLogFactory().Create(GetEnvironment(net))),
      route_(this,
             CreateRoute(net, std::move(config.sent_via_nodes)),
             CreateRoute(net, std::move(config.received_via_nodes))) {
  RtpExtension transport_sequence_number_extension;
  transport_sequence_number_extension.uri =
      RtpExtension::kTransportSequenceNumberUri;
  transport_sequence_number_extension.id = 1;
  transport_sequence_number_extension.encrypt = false;
  rtp_extensions_.RegisterByUri(transport_sequence_number_extension.id,
                                transport_sequence_number_extension.uri);
}

void FeedbackGeneratorWithoutNetworkImpl::SendPacket(size_t total_size,
                                                     size_t overhead) {
  SentPacket sent;
  sent.send_time = Now();
  sent.size = DataSize::Bytes(total_size);
  sent.sequence_number = sequence_number_++;
  sent_packets_.push(sent);

  RtpPacketToSend packet_to_send(&rtp_extensions_);
  packet_to_send.set_transport_sequence_number(sent.sequence_number);
  packet_to_send.SetExtension<TransportSequenceNumber>(sent.sequence_number);
  RTC_DCHECK(total_size > packet_to_send.headers_size() + overhead);
  if (total_size > packet_to_send.headers_size() + overhead) {
    packet_to_send.SetPayloadSize(total_size - packet_to_send.headers_size() -
                                  overhead);
    RTC_DCHECK_EQ(packet_to_send.size(), total_size - overhead);
  }
  event_log_->Log(std::make_unique<RtcEventRtpPacketOutgoing>(
      packet_to_send, /*probe_cluster_id*/ 0));

  route_.SendRequest(total_size, sent);
}

std::vector<TransportPacketsFeedback>
FeedbackGeneratorWithoutNetworkImpl::PopFeedback() {
  std::vector<TransportPacketsFeedback> ret;
  ret.swap(feedback_);
  return ret;
}

Timestamp FeedbackGeneratorWithoutNetworkImpl::Now() {
  return net_.time_controller()->GetClock()->CurrentTime();
}

void FeedbackGeneratorWithoutNetworkImpl::OnRequest(SentPacket packet,
                                                    Timestamp arrival_time) {
  PacketResult result;
  result.sent_packet = packet;
  result.receive_time = arrival_time;
  received_packets_.push_back(result);
  Timestamp first_recv = received_packets_.front().receive_time;
  if (Now() - first_recv > feedback_interval_) {
    route_.SendResponse(feedback_packet_size_.bytes<size_t>(),
                        std::move(received_packets_));
    received_packets_ = {};
  }
}

void FeedbackGeneratorWithoutNetworkImpl::OnResponse(
    std::vector<PacketResult> packet_results,
    Timestamp arrival_time) {
  TransportPacketsFeedback feedback;
  feedback.feedback_time = arrival_time;
  std::vector<PacketResult>::const_iterator received_packet_iterator =
      packet_results.begin();
  while (received_packet_iterator != packet_results.end()) {
    RTC_DCHECK(!sent_packets_.empty() &&
               sent_packets_.front().sequence_number <=
                   received_packet_iterator->sent_packet.sequence_number)
        << "reordering not implemented";
    if (sent_packets_.front().sequence_number <
        received_packet_iterator->sent_packet.sequence_number) {
      // Packet lost.
      PacketResult lost;
      lost.sent_packet = sent_packets_.front();
      feedback.packet_feedbacks.push_back(lost);
    }
    if (sent_packets_.front().sequence_number ==
        received_packet_iterator->sent_packet.sequence_number) {
      feedback.packet_feedbacks.push_back(*received_packet_iterator);
      ++received_packet_iterator;
    }
    sent_packets_.pop();
  }
  rtcp::TransportFeedback transport_feedback;
  transport_feedback.SetBase(
      feedback.ReceivedWithSendInfo()[0].sent_packet.sequence_number,
      feedback.ReceivedWithSendInfo()[0].receive_time);
  for (const auto& received_packet : feedback.ReceivedWithSendInfo()) {
    transport_feedback.AddReceivedPacket(
        received_packet.sent_packet.sequence_number,
        received_packet.receive_time);
  }
  event_log_->Log(
      std::make_unique<RtcEventRtcpPacketIncoming>(transport_feedback.Build()));
  feedback_.push_back(feedback);
}

FeedbackGeneratorImpl::FeedbackGeneratorImpl(
    FeedbackGeneratorImpl::Config config)
    : conf_(config),
      net_(CreateNetworkEmulationManager({.time_mode = TimeMode::kSimulated})),
      send_link_{new SimulatedNetwork(conf_.send_link)},
      ret_link_{new SimulatedNetwork(conf_.return_link)},
      delegate_({.sent_via_nodes = {net_->CreateEmulatedNode(
                     absl::WrapUnique(send_link_))},
                 .received_via_nodes = {net_->CreateEmulatedNode(
                     absl::WrapUnique(ret_link_))},
                 .feedback_interval = config.feedback_interval,
                 .feedback_packet_size = config.feedback_packet_size},
                *net_) {}

Timestamp FeedbackGeneratorImpl::Now() {
  return delegate_.Now();
}

void FeedbackGeneratorImpl::Sleep(TimeDelta duration) {
  net_->time_controller()->AdvanceTime(duration);
}

void FeedbackGeneratorImpl::SendPacket(size_t size) {
  delegate_.SendPacket(size, /*overhead=*/0);
}

std::vector<TransportPacketsFeedback> FeedbackGeneratorImpl::PopFeedback() {
  return delegate_.PopFeedback();
}

void FeedbackGeneratorImpl::SetSendConfig(BuiltInNetworkBehaviorConfig config) {
  conf_.send_link = config;
  send_link_->SetConfig(conf_.send_link);
}

void FeedbackGeneratorImpl::SetReturnConfig(
    BuiltInNetworkBehaviorConfig config) {
  conf_.return_link = config;
  ret_link_->SetConfig(conf_.return_link);
}

void FeedbackGeneratorImpl::SetSendLinkCapacity(DataRate capacity) {
  conf_.send_link.link_capacity = capacity;
  send_link_->SetConfig(conf_.send_link);
}

}  // namespace webrtc
