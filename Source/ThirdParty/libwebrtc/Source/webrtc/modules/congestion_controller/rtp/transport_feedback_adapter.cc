/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/rtp/transport_feedback_adapter.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "api/transport/ecn_marking.h"
#include "api/transport/network_types.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/ntp_time_util.h"
#include "modules/rtp_rtcp/source/rtcp_packet/congestion_control_feedback.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/network_route.h"

namespace webrtc {
namespace {

constexpr TimeDelta kSendTimeHistoryWindow = TimeDelta::Seconds(60);

class MinMax {
 public:
  explicit MinMax(int64_t value) : minimum_(value), maximum_(value) {}

  void Update(int64_t value) {
    minimum_ = std::min(minimum_, value);
    maximum_ = std::max(maximum_, value);
  }

  int64_t minimum() const { return minimum_; }
  int64_t maximum() const { return maximum_; }

 private:
  int64_t minimum_;
  int64_t maximum_;
};

}  // namespace

void InFlightBytesTracker::AddInFlightPacketBytes(
    const PacketFeedback& packet) {
  RTC_DCHECK(packet.sent.send_time.IsFinite());
  auto it = in_flight_data_.find(packet.network_route);
  if (it != in_flight_data_.end()) {
    it->second += packet.sent.size;
  } else {
    in_flight_data_.insert({packet.network_route, packet.sent.size});
  }
}

void InFlightBytesTracker::RemoveInFlightPacketBytes(
    const PacketFeedback& packet) {
  if (packet.sent.send_time.IsInfinite())
    return;
  auto it = in_flight_data_.find(packet.network_route);
  if (it != in_flight_data_.end()) {
    RTC_DCHECK_GE(it->second, packet.sent.size);
    it->second -= packet.sent.size;
    if (it->second.IsZero())
      in_flight_data_.erase(it);
  }
}

DataSize InFlightBytesTracker::GetOutstandingData(
    const NetworkRoute& network_route) const {
  auto it = in_flight_data_.find(network_route);
  if (it != in_flight_data_.end()) {
    return it->second;
  } else {
    return DataSize::Zero();
  }
}

// Comparator for consistent map with NetworkRoute as key.
bool InFlightBytesTracker::NetworkRouteComparator::operator()(
    const NetworkRoute& a,
    const NetworkRoute& b) const {
  if (a.local.network_id() != b.local.network_id())
    return a.local.network_id() < b.local.network_id();
  if (a.remote.network_id() != b.remote.network_id())
    return a.remote.network_id() < b.remote.network_id();

  if (a.local.adapter_id() != b.local.adapter_id())
    return a.local.adapter_id() < b.local.adapter_id();
  if (a.remote.adapter_id() != b.remote.adapter_id())
    return a.remote.adapter_id() < b.remote.adapter_id();

  if (a.local.uses_turn() != b.local.uses_turn())
    return a.local.uses_turn() < b.local.uses_turn();
  if (a.remote.uses_turn() != b.remote.uses_turn())
    return a.remote.uses_turn() < b.remote.uses_turn();

  return a.connected < b.connected;
}

TransportFeedbackAdapter::TransportFeedbackAdapter() = default;

void TransportFeedbackAdapter::AddPacket(const RtpPacketToSend& packet_to_send,
                                         const PacedPacketInfo& pacing_info,
                                         size_t overhead_bytes,
                                         Timestamp creation_time) {
  RTC_DCHECK(packet_to_send.transport_sequence_number());
  PacketFeedback feedback;

  feedback.creation_time = creation_time;
  // Note, if transport sequence number header extension is used, transport
  // sequence numbers are wrapped to 16 bit. See
  // RtpSenderEgress::CompleteSendPacket.
  feedback.sent.sequence_number = seq_num_unwrapper_.Unwrap(
      packet_to_send.transport_sequence_number().value_or(0));
  feedback.sent.size = DataSize::Bytes(packet_to_send.size() + overhead_bytes);
  feedback.sent.audio =
      packet_to_send.packet_type() == RtpPacketMediaType::kAudio;
  feedback.network_route = network_route_;
  feedback.sent.pacing_info = pacing_info;
  feedback.ssrc = packet_to_send.Ssrc();
  feedback.rtp_sequence_number = packet_to_send.SequenceNumber();
  feedback.is_retransmission =
      packet_to_send.packet_type() == RtpPacketMediaType::kRetransmission;
  feedback.sent_with_ect1 = packet_to_send.send_as_ect1();

  while (!history_.empty() &&
         creation_time - history_.begin()->second.creation_time >
             kSendTimeHistoryWindow) {
    // TODO(sprang): Warn if erasing (too many) old items?
    if (history_.begin()->second.sent.sequence_number > last_ack_seq_num_)
      in_flight_.RemoveInFlightPacketBytes(history_.begin()->second);

    const PacketFeedback& packet = history_.begin()->second;
    rtp_to_transport_sequence_number_.erase(
        {.ssrc = packet.ssrc,
         .rtp_sequence_number = packet.rtp_sequence_number});
    history_.erase(history_.begin());
  }
  // Note that it can happen that the same SSRC and sequence number is sent
  // again. e.g, audio retransmission.
  rtp_to_transport_sequence_number_.emplace(
      SsrcAndRtpSequencenumber(
          {.ssrc = feedback.ssrc,
           .rtp_sequence_number = feedback.rtp_sequence_number}),
      feedback.sent.sequence_number);
  history_.emplace(feedback.sent.sequence_number, feedback);
}

std::optional<SentPacket> TransportFeedbackAdapter::ProcessSentPacket(
    const SentPacketInfo& sent_packet) {
  auto send_time = Timestamp::Millis(sent_packet.send_time_ms);
  // TODO(srte): Only use one way to indicate that packet feedback is used.
  if (sent_packet.info.included_in_feedback || sent_packet.packet_id != -1) {
    int64_t unwrapped_seq_num =
        seq_num_unwrapper_.Unwrap(sent_packet.packet_id);
    auto it = history_.find(unwrapped_seq_num);
    if (it != history_.end()) {
      bool packet_retransmit = it->second.sent.send_time.IsFinite();
      it->second.sent.send_time = send_time;
      last_send_time_ = std::max(last_send_time_, send_time);
      // TODO(srte): Don't do this on retransmit.
      if (!pending_untracked_size_.IsZero()) {
        if (send_time < last_untracked_send_time_)
          RTC_LOG(LS_WARNING)
              << "appending acknowledged data for out of order packet. (Diff: "
              << ToString(last_untracked_send_time_ - send_time) << " ms.)";
        it->second.sent.prior_unacked_data += pending_untracked_size_;
        pending_untracked_size_ = DataSize::Zero();
      }
      if (!packet_retransmit) {
        if (it->second.sent.sequence_number > last_ack_seq_num_)
          in_flight_.AddInFlightPacketBytes(it->second);
        it->second.sent.data_in_flight = GetOutstandingData();
        return it->second.sent;
      }
    }
  } else if (sent_packet.info.included_in_allocation) {
    if (send_time < last_send_time_) {
      RTC_LOG(LS_WARNING) << "ignoring untracked data for out of order packet.";
    }
    pending_untracked_size_ +=
        DataSize::Bytes(sent_packet.info.packet_size_bytes);
    last_untracked_send_time_ = std::max(last_untracked_send_time_, send_time);
  }
  return std::nullopt;
}

std::optional<TransportPacketsFeedback>
TransportFeedbackAdapter::ProcessTransportFeedback(
    const rtcp::TransportFeedback& feedback,
    Timestamp feedback_receive_time) {
  if (feedback.GetPacketStatusCount() == 0) {
    RTC_LOG(LS_INFO) << "Empty transport feedback packet received.";
    return std::nullopt;
  }

  // Add timestamp deltas to a local time base selected on first packet arrival.
  // This won't be the true time base, but makes it easier to manually inspect
  // time stamps.
  if (last_transport_feedback_base_time_.IsInfinite()) {
    current_offset_ = feedback_receive_time;
  } else {
    // TODO(srte): We shouldn't need to do rounding here.
    const TimeDelta delta =
        feedback.GetBaseDelta(last_transport_feedback_base_time_)
            .RoundDownTo(TimeDelta::Millis(1));
    // Protect against assigning current_offset_ negative value.
    if (delta < Timestamp::Zero() - current_offset_) {
      RTC_LOG(LS_WARNING) << "Unexpected feedback timestamp received.";
      current_offset_ = feedback_receive_time;
    } else {
      current_offset_ += delta;
    }
  }
  last_transport_feedback_base_time_ = feedback.BaseTime();

  std::vector<PacketResult> packet_result_vector;
  packet_result_vector.reserve(feedback.GetPacketStatusCount());

  size_t failed_lookups = 0;
  size_t ignored = 0;

  feedback.ForAllPackets([&](uint16_t sequence_number,
                             TimeDelta delta_since_base) {
    int64_t seq_num = seq_num_unwrapper_.Unwrap(sequence_number);
    std::optional<PacketFeedback> packet_feedback = RetrievePacketFeedback(
        seq_num, /*received=*/delta_since_base.IsFinite());
    if (!packet_feedback) {
      ++failed_lookups;
      return;
    }

    if (packet_feedback->network_route == network_route_) {
      PacketResult result;
      result.sent_packet = packet_feedback->sent;
      if (delta_since_base.IsFinite()) {
        result.receive_time = current_offset_ + delta_since_base.RoundDownTo(
                                                    TimeDelta::Millis(1));
      }
      result.rtp_packet_info = {
          .ssrc = packet_feedback->ssrc,
          .rtp_sequence_number = packet_feedback->rtp_sequence_number,
          .is_retransmission = packet_feedback->is_retransmission};
      packet_result_vector.push_back(result);
    } else {
      ++ignored;
    }
  });

  if (failed_lookups > 0) {
    RTC_LOG(LS_WARNING)
        << "Failed to lookup send time for " << failed_lookups << " packet"
        << (failed_lookups > 1 ? "s" : "")
        << ". Packets reordered or send time history too small?";
  }
  if (ignored > 0) {
    RTC_LOG(LS_INFO) << "Ignoring " << ignored
                     << " packets because they were sent on a different route.";
  }
  return ToTransportFeedback(std::move(packet_result_vector),
                             feedback_receive_time, /*supports_ecn=*/false);
}

std::optional<TransportPacketsFeedback>
TransportFeedbackAdapter::ProcessCongestionControlFeedback(
    const rtcp::CongestionControlFeedback& feedback,
    Timestamp feedback_receive_time) {
  if (feedback.packets().empty()) {
    RTC_LOG(LS_INFO) << "Empty congestion control feedback packet received.";
    return std::nullopt;
  }
  if (current_offset_.IsInfinite()) {
    current_offset_ = feedback_receive_time;
  }
  TimeDelta feedback_delta = last_feedback_compact_ntp_time_
                                 ? CompactNtpIntervalToTimeDelta(
                                       feedback.report_timestamp_compact_ntp() -
                                       *last_feedback_compact_ntp_time_)
                                 : TimeDelta::Zero();
  last_feedback_compact_ntp_time_ = feedback.report_timestamp_compact_ntp();
  if (feedback_delta < TimeDelta::Zero()) {
    RTC_LOG(LS_WARNING) << "Unexpected feedback ntp time delta "
                        << feedback_delta << ".";
    current_offset_ = feedback_receive_time;
  } else {
    current_offset_ += feedback_delta;
  }

  int ignored_packets = 0;
  int failed_lookups = 0;
  bool supports_ecn = true;
  std::vector<PacketResult> packet_result_vector;
  std::optional<MinMax> sequence_number_in_report;
  for (const rtcp::CongestionControlFeedback::PacketInfo& packet_info :
       feedback.packets()) {
    std::optional<PacketFeedback> packet_feedback = RetrievePacketFeedback(
        {.ssrc = packet_info.ssrc,
         .rtp_sequence_number = packet_info.sequence_number},
        /*received=*/packet_info.arrival_time_offset.IsFinite());
    if (!packet_feedback) {
      ++failed_lookups;
      continue;
    }
    if (packet_feedback->network_route != network_route_) {
      ++ignored_packets;
      continue;
    }
    if (!sequence_number_in_report.has_value()) {
      sequence_number_in_report.emplace(packet_feedback->sent.sequence_number);
    } else {
      sequence_number_in_report->Update(packet_feedback->sent.sequence_number);
    }
    PacketResult result;
    result.sent_packet = packet_feedback->sent;
    if (packet_info.arrival_time_offset.IsFinite()) {
      result.receive_time = current_offset_ - packet_info.arrival_time_offset;
      supports_ecn &= packet_info.ecn != EcnMarking::kNotEct;
      result.arrival_time_offset = packet_info.arrival_time_offset;
    }
    result.ecn = packet_info.ecn;
    result.sent_with_ect1 = packet_feedback->sent_with_ect1;
    result.reported_recovered_for_the_first_time =
        packet_info.received() && packet_feedback->previously_reported_lost;
    result.reported_lost_for_the_first_time =
        !packet_info.received() && !packet_feedback->previously_reported_lost;

    result.rtp_packet_info = {
        .ssrc = packet_feedback->ssrc,
        .rtp_sequence_number = packet_feedback->rtp_sequence_number,
        .is_retransmission = packet_feedback->is_retransmission};
    packet_result_vector.push_back(result);
  }

  // Report packets as lost that are in the same transport sequence number range
  // as reported packets, but that are not mentioned in the report.
  // Code below relies on the fact that received packets are already erased from
  // the `history_`.
  if (sequence_number_in_report.has_value()) {
    auto begin = history_.lower_bound(sequence_number_in_report->minimum());
    auto end = history_.upper_bound(sequence_number_in_report->maximum());
    for (auto it = begin; it != end; ++it) {
      PacketFeedback& packet_feedback = it->second;
      if (it->second.previously_reported_lost) {
        continue;
      }
      it->second.previously_reported_lost = true;
      PacketResult result;
      result.sent_packet = packet_feedback.sent;
      result.sent_with_ect1 = packet_feedback.sent_with_ect1;
      result.reported_lost_for_the_first_time = true;
      result.rtp_packet_info = {
          .ssrc = packet_feedback.ssrc,
          .rtp_sequence_number = packet_feedback.rtp_sequence_number,
          .is_retransmission = packet_feedback.is_retransmission};
      packet_result_vector.push_back(result);
    }
  }

  if (failed_lookups > 0) {
    RTC_LOG(LS_INFO) << "Failed to lookup send time for " << failed_lookups
                     << " packet" << (failed_lookups > 1 ? "s" : "")
                     << ". Packets reordered or send time history too "
                        "small, or packet reported received more than once.";
  }
  if (ignored_packets > 0) {
    RTC_LOG(LS_INFO) << "Ignoring " << ignored_packets
                     << " packets because they were sent on a different route.";
  }

  // Feedback is expected to be sorted in send order.
  absl::c_sort(packet_result_vector, [](const PacketResult& lhs,
                                        const PacketResult& rhs) {
    return lhs.sent_packet.sequence_number < rhs.sent_packet.sequence_number;
  });
  return ToTransportFeedback(std::move(packet_result_vector),
                             feedback_receive_time, supports_ecn);
}

std::optional<TransportPacketsFeedback>
TransportFeedbackAdapter::ToTransportFeedback(
    std::vector<PacketResult> packet_results,
    Timestamp feedback_receive_time,
    bool supports_ecn) {
  TransportPacketsFeedback msg;
  msg.feedback_time = feedback_receive_time;
  if (packet_results.empty()) {
    return std::nullopt;
  }
  msg.packet_feedbacks = std::move(packet_results);
  msg.data_in_flight = in_flight_.GetOutstandingData(network_route_);
  msg.transport_supports_ecn = supports_ecn;

  return msg;
}

void TransportFeedbackAdapter::SetNetworkRoute(
    const NetworkRoute& network_route) {
  network_route_ = network_route;
}

DataSize TransportFeedbackAdapter::GetOutstandingData() const {
  return in_flight_.GetOutstandingData(network_route_);
}

std::optional<PacketFeedback> TransportFeedbackAdapter::RetrievePacketFeedback(
    const SsrcAndRtpSequencenumber& key,
    bool received) {
  auto it = rtp_to_transport_sequence_number_.find(key);
  if (it == rtp_to_transport_sequence_number_.end()) {
    return std::nullopt;
  }
  return RetrievePacketFeedback(it->second, received);
}

std::optional<PacketFeedback> TransportFeedbackAdapter::RetrievePacketFeedback(
    int64_t transport_seq_num,
    bool received) {
  if (transport_seq_num > last_ack_seq_num_) {
    // Starts at history_.begin() if last_ack_seq_num_ < 0, since any
    // valid sequence number is >= 0.
    for (auto it = history_.upper_bound(last_ack_seq_num_);
         it != history_.upper_bound(transport_seq_num); ++it) {
      in_flight_.RemoveInFlightPacketBytes(it->second);
    }
    last_ack_seq_num_ = transport_seq_num;
  }

  auto it = history_.find(transport_seq_num);
  if (it == history_.end()) {
    RTC_LOG(LS_INFO) << "Failed to lookup send time for packet with "
                     << transport_seq_num
                     << ". Send time history too small or packet was "
                        "reported as received twice.";
    return std::nullopt;
  }

  if (it->second.sent.send_time.IsInfinite()) {
    // TODO(srte): Fix the tests that makes this happen and make this a
    // DCHECK.
    RTC_DLOG(LS_ERROR)
        << "Received feedback before packet was indicated as sent";
    return std::nullopt;
  }

  PacketFeedback packet_feedback = it->second;
  if (received) {
    // Note: Lost packets are not removed from history because they might
    // be reported as received by a later feedback.
    rtp_to_transport_sequence_number_.erase(
        {.ssrc = packet_feedback.ssrc,
         .rtp_sequence_number = packet_feedback.rtp_sequence_number});
    history_.erase(it);
  } else {
    // Update `previously_reported_lost` after taking copy of the
    // `packet_feedback'. Thus the new value would be returned next time caller
    // retrieves the feedback. This way caller can distinguish when packet was
    // reported lost for the first time and thus account such loss only once.
    it->second.previously_reported_lost = true;
  }
  return packet_feedback;
}

}  // namespace webrtc
