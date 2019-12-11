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

#include <stdlib.h>

#include <algorithm>
#include <cmath>
#include <utility>

#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {

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

TransportFeedbackAdapter::TransportFeedbackAdapter()
    : allow_duplicates_(field_trial::IsEnabled(
          "WebRTC-TransportFeedbackAdapter-AllowDuplicates")),
      send_time_history_(kSendTimeHistoryWindowMs),
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

void TransportFeedbackAdapter::AddPacket(const RtpPacketSendInfo& packet_info,
                                         size_t overhead_bytes,
                                         Timestamp creation_time) {
  {
    rtc::CritScope cs(&lock_);
    PacketFeedback packet_feedback(
        creation_time.ms(), packet_info.transport_sequence_number,
        packet_info.length + overhead_bytes, local_net_id_, remote_net_id_,
        packet_info.pacing_info);
    if (packet_info.has_rtp_sequence_number) {
      packet_feedback.ssrc = packet_info.ssrc;
      packet_feedback.rtp_sequence_number = packet_info.rtp_sequence_number;
    }
    send_time_history_.RemoveOld(creation_time.ms());
    send_time_history_.AddNewPacket(std::move(packet_feedback));
  }

  {
    rtc::CritScope cs(&observers_lock_);
    for (auto* observer : observers_) {
      observer->OnPacketAdded(packet_info.ssrc,
                              packet_info.transport_sequence_number);
    }
  }
}
absl::optional<SentPacket> TransportFeedbackAdapter::ProcessSentPacket(
    const rtc::SentPacket& sent_packet) {
  rtc::CritScope cs(&lock_);
  // TODO(srte): Only use one way to indicate that packet feedback is used.
  if (sent_packet.info.included_in_feedback || sent_packet.packet_id != -1) {
    SendTimeHistory::Status send_status = send_time_history_.OnSentPacket(
        sent_packet.packet_id, sent_packet.send_time_ms);
    absl::optional<PacketFeedback> packet;
    if (allow_duplicates_ ||
        send_status != SendTimeHistory::Status::kDuplicate) {
      packet = send_time_history_.GetPacket(sent_packet.packet_id);
    }

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
    const rtcp::TransportFeedback& feedback,
    Timestamp feedback_receive_time) {
  DataSize prior_in_flight = GetOutstandingData();

  last_packet_feedback_vector_ =
      GetPacketFeedbackVector(feedback, feedback_receive_time);
  {
    rtc::CritScope cs(&observers_lock_);
    for (auto* observer : observers_) {
      observer->OnPacketFeedbackVector(last_packet_feedback_vector_);
    }
  }

  std::vector<PacketFeedback> feedback_vector = last_packet_feedback_vector_;
  if (feedback_vector.empty())
    return absl::nullopt;

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
  {
    rtc::CritScope cs(&lock_);
    absl::optional<int64_t> first_unacked_send_time_ms =
        send_time_history_.GetFirstUnackedSendTime();
    if (first_unacked_send_time_ms)
      msg.first_unacked_send_time = Timestamp::ms(*first_unacked_send_time_ms);
  }
  msg.feedback_time = feedback_receive_time;
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
    const rtcp::TransportFeedback& feedback,
    Timestamp feedback_time) {
  // Add timestamp deltas to a local time base selected on first packet arrival.
  // This won't be the true time base, but makes it easier to manually inspect
  // time stamps.
  if (last_timestamp_us_ == kNoTimestamp) {
    current_offset_ms_ = feedback_time.ms();
  } else {
    current_offset_ms_ += feedback.GetBaseDeltaUs(last_timestamp_us_) / 1000;
  }
  last_timestamp_us_ = feedback.GetBaseTimeUs();

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

std::vector<PacketFeedback>
TransportFeedbackAdapter::GetTransportFeedbackVector() const {
  return last_packet_feedback_vector_;
}

}  // namespace webrtc
