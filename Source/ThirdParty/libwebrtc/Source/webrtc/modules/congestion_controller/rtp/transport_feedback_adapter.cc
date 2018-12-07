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

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/mod_ops.h"

namespace webrtc {
namespace {
void SortPacketFeedbackVector(std::vector<webrtc::PacketFeedback>* input) {
  std::sort(input->begin(), input->end(), PacketFeedbackComparator());
}

PacketResult NetworkPacketFeedbackFromRtpPacketFeedback(
    const webrtc::PacketFeedback& pf) {
  PacketResult feedback;
  if (pf.arrival_time_ms == webrtc::PacketFeedback::kNotReceived) {
    feedback.receive_time = Timestamp::PlusInfinity();
  } else {
    feedback.receive_time = Timestamp::ms(pf.arrival_time_ms);
  }
  feedback.sent_packet.sequence_number = pf.long_sequence_number;
  feedback.sent_packet.send_time = Timestamp::ms(pf.send_time_ms);
  feedback.sent_packet.size = DataSize::bytes(pf.payload_size);
  feedback.sent_packet.pacing_info = pf.pacing_info;
  feedback.sent_packet.prior_unacked_data =
      DataSize::bytes(pf.unacknowledged_data);
  return feedback;
}
}  // namespace
const int64_t kNoTimestamp = -1;
const int64_t kSendTimeHistoryWindowMs = 60000;
const int64_t kBaseTimestampScaleFactor =
    rtcp::TransportFeedback::kDeltaScaleFactor * (1 << 8);
const int64_t kBaseTimestampRangeSizeUs = kBaseTimestampScaleFactor * (1 << 24);

TransportFeedbackAdapter::TransportFeedbackAdapter(const Clock* clock)
    : send_time_history_(clock, kSendTimeHistoryWindowMs),
      clock_(clock),
      current_offset_ms_(kNoTimestamp),
      last_timestamp_us_(kNoTimestamp),
      local_net_id_(0),
      remote_net_id_(0) {}

TransportFeedbackAdapter::~TransportFeedbackAdapter() {
  RTC_DCHECK(observers_.empty());
}

void TransportFeedbackAdapter::RegisterPacketFeedbackObserver(
    PacketFeedbackObserver* observer) {
  rtc::CritScope cs(&observers_lock_);
  RTC_DCHECK(observer);
  RTC_DCHECK(std::find(observers_.begin(), observers_.end(), observer) ==
             observers_.end());
  observers_.push_back(observer);
}

void TransportFeedbackAdapter::DeRegisterPacketFeedbackObserver(
    PacketFeedbackObserver* observer) {
  rtc::CritScope cs(&observers_lock_);
  RTC_DCHECK(observer);
  const auto it = std::find(observers_.begin(), observers_.end(), observer);
  RTC_DCHECK(it != observers_.end());
  observers_.erase(it);
}

void TransportFeedbackAdapter::AddPacket(uint32_t ssrc,
                                         uint16_t sequence_number,
                                         size_t length,
                                         const PacedPacketInfo& pacing_info) {
  {
    rtc::CritScope cs(&lock_);
    const int64_t creation_time_ms = clock_->TimeInMilliseconds();
    send_time_history_.AddAndRemoveOld(
        PacketFeedback(creation_time_ms, sequence_number, length, local_net_id_,
                       remote_net_id_, pacing_info));
  }

  {
    rtc::CritScope cs(&observers_lock_);
    for (auto* observer : observers_) {
      observer->OnPacketAdded(ssrc, sequence_number);
    }
  }
}

absl::optional<SentPacket> TransportFeedbackAdapter::ProcessSentPacket(
    const rtc::SentPacket& sent_packet) {
  rtc::CritScope cs(&lock_);
  // TODO(srte): Only use one way to indicate that packet feedback is used.
  if (sent_packet.info.included_in_feedback || sent_packet.packet_id != -1) {
    send_time_history_.OnSentPacket(sent_packet.packet_id,
                                    sent_packet.send_time_ms);
    absl::optional<PacketFeedback> packet =
        send_time_history_.GetPacket(sent_packet.packet_id);
    if (packet) {
      SentPacket msg;
      msg.size = DataSize::bytes(packet->payload_size);
      msg.send_time = Timestamp::ms(packet->send_time_ms);
      msg.sequence_number = packet->long_sequence_number;
      msg.prior_unacked_data = DataSize::bytes(packet->unacknowledged_data);
      msg.data_in_flight =
          send_time_history_.GetOutstandingData(local_net_id_, remote_net_id_);
      return msg;
    }
  } else if (sent_packet.info.included_in_allocation) {
    send_time_history_.AddUntracked(sent_packet.info.packet_size_bytes,
                                    sent_packet.send_time_ms);
  }
  return absl::nullopt;
}

absl::optional<TransportPacketsFeedback>
TransportFeedbackAdapter::ProcessTransportFeedback(
    const rtcp::TransportFeedback& feedback) {
  int64_t feedback_time_ms = clock_->TimeInMilliseconds();
  DataSize prior_in_flight = GetOutstandingData();
  OnTransportFeedback(feedback);
  std::vector<PacketFeedback> feedback_vector = last_packet_feedback_vector_;
  if (feedback_vector.empty())
    return absl::nullopt;

  SortPacketFeedbackVector(&feedback_vector);
  TransportPacketsFeedback msg;
  for (const PacketFeedback& rtp_feedback : feedback_vector) {
    if (rtp_feedback.send_time_ms != PacketFeedback::kNoSendTime) {
      auto feedback = NetworkPacketFeedbackFromRtpPacketFeedback(rtp_feedback);
      msg.packet_feedbacks.push_back(feedback);
    } else if (rtp_feedback.arrival_time_ms == PacketFeedback::kNotReceived) {
      msg.sendless_arrival_times.push_back(Timestamp::PlusInfinity());
    } else {
      msg.sendless_arrival_times.push_back(
          Timestamp::ms(rtp_feedback.arrival_time_ms));
    }
  }
  msg.feedback_time = Timestamp::ms(feedback_time_ms);
  msg.prior_in_flight = prior_in_flight;
  msg.data_in_flight = GetOutstandingData();
  return msg;
}

void TransportFeedbackAdapter::SetNetworkIds(uint16_t local_id,
                                             uint16_t remote_id) {
  rtc::CritScope cs(&lock_);
  local_net_id_ = local_id;
  remote_net_id_ = remote_id;
}

DataSize TransportFeedbackAdapter::GetOutstandingData() const {
  rtc::CritScope cs(&lock_);
  return send_time_history_.GetOutstandingData(local_net_id_, remote_net_id_);
}

std::vector<PacketFeedback> TransportFeedbackAdapter::GetPacketFeedbackVector(
    const rtcp::TransportFeedback& feedback) {
  int64_t timestamp_us = feedback.GetBaseTimeUs();
  int64_t now_ms = clock_->TimeInMilliseconds();
  // Add timestamp deltas to a local time base selected on first packet arrival.
  // This won't be the true time base, but makes it easier to manually inspect
  // time stamps.
  if (last_timestamp_us_ == kNoTimestamp) {
    current_offset_ms_ = now_ms;
  } else {
    int64_t delta = timestamp_us - last_timestamp_us_;

    // Detect and compensate for wrap-arounds in base time.
    if (std::abs(delta - kBaseTimestampRangeSizeUs) < std::abs(delta)) {
      delta -= kBaseTimestampRangeSizeUs;  // Wrap backwards.
    } else if (std::abs(delta + kBaseTimestampRangeSizeUs) < std::abs(delta)) {
      delta += kBaseTimestampRangeSizeUs;  // Wrap forwards.
    }

    current_offset_ms_ += delta / 1000;
  }
  last_timestamp_us_ = timestamp_us;

  std::vector<PacketFeedback> packet_feedback_vector;
  if (feedback.GetPacketStatusCount() == 0) {
    RTC_LOG(LS_INFO) << "Empty transport feedback packet received.";
    return packet_feedback_vector;
  }
  packet_feedback_vector.reserve(feedback.GetPacketStatusCount());
  {
    rtc::CritScope cs(&lock_);
    size_t failed_lookups = 0;
    int64_t offset_us = 0;
    int64_t timestamp_ms = 0;
    uint16_t seq_num = feedback.GetBaseSequence();
    for (const auto& packet : feedback.GetReceivedPackets()) {
      // Insert into the vector those unreceived packets which precede this
      // iteration's received packet.
      for (; seq_num != packet.sequence_number(); ++seq_num) {
        PacketFeedback packet_feedback(PacketFeedback::kNotReceived, seq_num);
        // Note: Element not removed from history because it might be reported
        // as received by another feedback.
        if (!send_time_history_.GetFeedback(&packet_feedback, false))
          ++failed_lookups;
        if (packet_feedback.local_net_id == local_net_id_ &&
            packet_feedback.remote_net_id == remote_net_id_) {
          packet_feedback_vector.push_back(packet_feedback);
        }
      }

      // Handle this iteration's received packet.
      offset_us += packet.delta_us();
      timestamp_ms = current_offset_ms_ + (offset_us / 1000);
      PacketFeedback packet_feedback(timestamp_ms, packet.sequence_number());
      if (!send_time_history_.GetFeedback(&packet_feedback, true))
        ++failed_lookups;
      if (packet_feedback.local_net_id == local_net_id_ &&
          packet_feedback.remote_net_id == remote_net_id_) {
        packet_feedback_vector.push_back(packet_feedback);
      }

      ++seq_num;
    }

    if (failed_lookups > 0) {
      RTC_LOG(LS_WARNING) << "Failed to lookup send time for " << failed_lookups
                          << " packet" << (failed_lookups > 1 ? "s" : "")
                          << ". Send time history too small?";
    }
  }
  return packet_feedback_vector;
}

void TransportFeedbackAdapter::OnTransportFeedback(
    const rtcp::TransportFeedback& feedback) {
  last_packet_feedback_vector_ = GetPacketFeedbackVector(feedback);
  {
    rtc::CritScope cs(&observers_lock_);
    for (auto* observer : observers_) {
      observer->OnPacketFeedbackVector(last_packet_feedback_vector_);
    }
  }
}

std::vector<PacketFeedback>
TransportFeedbackAdapter::GetTransportFeedbackVector() const {
  return last_packet_feedback_vector_;
}
}  // namespace webrtc
