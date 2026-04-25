/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/scream/test/cc_feedback_generator.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "api/test/simulated_network.h"
#include "api/transport/ecn_marking.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/clock.h"
#include "test/gtest.h"
#include "test/network/simulated_network.h"

namespace webrtc {

// static
int CcFeedbackGenerator::CountCeMarks(
    const TransportPacketsFeedback& feedback) {
  int number_of_ce_marks = 0;
  for (const PacketResult& packet : feedback.packet_feedbacks) {
    if (packet.ecn == EcnMarking::kCe) {
      ++number_of_ce_marks;
    }
  }
  return number_of_ce_marks;
}

CcFeedbackGenerator::CcFeedbackGenerator(Config config)
    : packet_size_(config.packet_size),
      time_between_feedback_(config.time_between_feedback),
      one_way_delay_(TimeDelta::Millis(config.network_config.queue_delay_ms)),
      send_as_ect1_(config.send_as_ect1),
      network_(config.network_config,
               /*random_seed=*/1,
               std::move(config.queue)) {
  RTC_CHECK(!config.network_config.allow_reordering)
      << "Reordering is not supported";
}

TransportPacketsFeedback CcFeedbackGenerator::ProcessUntilNextFeedback(
    DataRate send_rate,
    SimulatedClock& clock,
    absl::AnyInvocable<void(const SentPacket&)> sent_packet_cb,
    TimeDelta max_time) {
  Timestamp end_time = clock.CurrentTime() + max_time;
  while (clock.CurrentTime() < end_time) {
    std::optional<SentPacket> sent_packet =
        MaybeSendPackets(clock.CurrentTime(), send_rate);
    ProcessNetwork(clock.CurrentTime());
    if (sent_packet_cb && sent_packet.has_value()) {
      sent_packet_cb(*sent_packet);
    }
    std::optional<TransportPacketsFeedback> feedback =
        MaybeSendFeedback(clock.CurrentTime());
    if (feedback.has_value()) {
      return *feedback;
    }
    if (!sent_packet.has_value() && !feedback.has_value()) {
      clock.AdvanceTime(TimeDelta::Millis(1));
    }
  }
  ADD_FAILURE() << "No feedback received after " << max_time;
  return TransportPacketsFeedback();
}

std::optional<SentPacket> CcFeedbackGenerator::MaybeSendPackets(
    Timestamp time,
    DataRate send_rate) {
  if (last_send_budget_update.IsInfinite()) {
    send_budget_ = packet_size_;
  } else {
    send_budget_ += send_rate * (time - last_send_budget_update);
  }
  last_send_budget_update = time;

  // This simulator pace out packets with perfect pacing.
  if (send_budget_ < packet_size_) {
    return std::nullopt;
  }
    send_budget_ -= packet_size_;
    PacketInFlightInfo packet_info(
        packet_size_, time,
        /*packet_id=*/next_packet_id_++,
        send_as_ect1_ ? EcnMarking::kEct1 : EcnMarking::kNotEct);
    packets_in_flight_.push_back(packet_info);
    bool packet_sent = network_.EnqueuePacket(packet_info);
    if (!packet_sent) {
      RTC_LOG(LS_VERBOSE) << "Packet " << (next_packet_id_ - 1)
                          << " dropped by queueu ";
    }

    DataSize bytes_in_flight = DataSize::Zero();
    for (const PacketInFlightInfo& in_flight : packets_in_flight_) {
      bytes_in_flight += in_flight.packet_size();
    }
    SentPacket sent_packet;
    sent_packet.send_time = packet_info.send_time();
    sent_packet.size = packet_info.packet_size();
    sent_packet.data_in_flight = bytes_in_flight;
    return sent_packet;
}

void CcFeedbackGenerator::ProcessNetwork(Timestamp time) {
  std::vector<PacketDeliveryInfo> received_packets =
      network_.DequeueDeliverablePackets(time.us());
  packets_received_.insert(packets_received_.end(), received_packets.begin(),
                           received_packets.end());
}

std::optional<TransportPacketsFeedback>
CcFeedbackGenerator::MaybeSendFeedback(Timestamp time) {
  if (last_feedback_time_.IsFinite() &&
      time - last_feedback_time_ < time_between_feedback_) {
    return std::nullopt;
  }
  // Time to deliver feedback if there are packets to deliver.
  TransportPacketsFeedback feedback;
  // Deliver feedback one way delay later than when the packets were
  // received.
  while (!packets_received_.empty() &&
         time - Timestamp::Micros(packets_received_.front().receive_time_us) >=
             one_way_delay_) {
    PacketDeliveryInfo delivery_info = packets_received_.front();
    packets_received_.pop_front();

    while (packets_in_flight_.front().packet_id != delivery_info.packet_id) {
      // Reordering of packets is not supported, so the packet is lost.
      PacketResult packet_result;
      packet_result.sent_packet.send_time =
          packets_in_flight_.front().send_time();
      packet_result.sent_packet.size = packets_in_flight_.front().packet_size();
      packets_in_flight_.pop_front();
      feedback.packet_feedbacks.push_back(packet_result);
    }
    RTC_CHECK_EQ(packets_in_flight_.front().packet_id, delivery_info.packet_id);
    PacketResult packet_result;
    packet_result.sent_packet.send_time =
        packets_in_flight_.front().send_time();
    packet_result.sent_packet.sequence_number =
        packets_in_flight_.front().packet_id;
    packet_result.sent_packet.size = packets_in_flight_.front().packet_size();

    packet_result.receive_time =
        Timestamp::Micros(delivery_info.receive_time_us);
    packet_result.arrival_time_offset =
        time - packet_result.receive_time - one_way_delay_;
    packet_result.ecn = delivery_info.ecn;
    packets_in_flight_.pop_front();
    feedback.packet_feedbacks.push_back(packet_result);
  }
  if (feedback.packet_feedbacks.empty()) {
    return std::nullopt;
  }
  for (const PacketInFlightInfo& in_flight : packets_in_flight_) {
    feedback.data_in_flight += in_flight.packet_size();
  }
  feedback.feedback_time = time;
  last_feedback_time_ = time;
  return feedback;
}
}  // namespace webrtc
