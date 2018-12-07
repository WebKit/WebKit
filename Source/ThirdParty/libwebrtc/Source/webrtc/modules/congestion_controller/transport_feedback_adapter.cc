/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/transport_feedback_adapter.h"

#include <algorithm>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/mod_ops.h"

namespace webrtc {

const int64_t kNoTimestamp = -1;
const int64_t kSendTimeHistoryWindowMs = 60000;
const int64_t kBaseTimestampScaleFactor =
    rtcp::TransportFeedback::kDeltaScaleFactor * (1 << 8);
const int64_t kBaseTimestampRangeSizeUs = kBaseTimestampScaleFactor * (1 << 24);

LegacyTransportFeedbackAdapter::LegacyTransportFeedbackAdapter(
    const Clock* clock)
    : send_time_history_(clock, kSendTimeHistoryWindowMs),
      clock_(clock),
      current_offset_ms_(kNoTimestamp),
      last_timestamp_us_(kNoTimestamp),
      local_net_id_(0),
      remote_net_id_(0) {}

LegacyTransportFeedbackAdapter::~LegacyTransportFeedbackAdapter() {
  RTC_DCHECK(observers_.empty());
}

void LegacyTransportFeedbackAdapter::RegisterPacketFeedbackObserver(
    PacketFeedbackObserver* observer) {
  rtc::CritScope cs(&observers_lock_);
  RTC_DCHECK(observer);
  RTC_DCHECK(std::find(observers_.begin(), observers_.end(), observer) ==
             observers_.end());
  observers_.push_back(observer);
}

void LegacyTransportFeedbackAdapter::DeRegisterPacketFeedbackObserver(
    PacketFeedbackObserver* observer) {
  rtc::CritScope cs(&observers_lock_);
  RTC_DCHECK(observer);
  const auto it = std::find(observers_.begin(), observers_.end(), observer);
  RTC_DCHECK(it != observers_.end());
  observers_.erase(it);
}

void LegacyTransportFeedbackAdapter::AddPacket(
    uint32_t ssrc,
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

void LegacyTransportFeedbackAdapter::OnSentPacket(uint16_t sequence_number,
                                                  int64_t send_time_ms) {
  rtc::CritScope cs(&lock_);
  send_time_history_.OnSentPacket(sequence_number, send_time_ms);
}

void LegacyTransportFeedbackAdapter::SetNetworkIds(uint16_t local_id,
                                                   uint16_t remote_id) {
  rtc::CritScope cs(&lock_);
  local_net_id_ = local_id;
  remote_net_id_ = remote_id;
}

std::vector<PacketFeedback>
LegacyTransportFeedbackAdapter::GetPacketFeedbackVector(
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
  int64_t feedback_rtt = -1;
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
        if (packet_feedback.send_time_ms >= 0) {
          int64_t rtt = now_ms - packet_feedback.send_time_ms;
          // max() is used to account for feedback being delayed by the
          // receiver.
          feedback_rtt = std::max(rtt, feedback_rtt);
        }
        packet_feedback_vector.push_back(packet_feedback);
      }

      ++seq_num;
    }

    if (failed_lookups > 0) {
      RTC_LOG(LS_WARNING) << "Failed to lookup send time for " << failed_lookups
                          << " packet" << (failed_lookups > 1 ? "s" : "")
                          << ". Send time history too small?";
    }
    if (feedback_rtt > -1) {
      feedback_rtts_.push_back(feedback_rtt);
      const size_t kFeedbackRttWindow = 32;
      if (feedback_rtts_.size() > kFeedbackRttWindow)
        feedback_rtts_.pop_front();
      min_feedback_rtt_.emplace(
          *std::min_element(feedback_rtts_.begin(), feedback_rtts_.end()));
    }
  }
  return packet_feedback_vector;
}

void LegacyTransportFeedbackAdapter::OnTransportFeedback(
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
LegacyTransportFeedbackAdapter::GetTransportFeedbackVector() const {
  return last_packet_feedback_vector_;
}

absl::optional<int64_t> LegacyTransportFeedbackAdapter::GetMinFeedbackLoopRtt()
    const {
  rtc::CritScope cs(&lock_);
  return min_feedback_rtt_;
}

size_t LegacyTransportFeedbackAdapter::GetOutstandingBytes() const {
  rtc::CritScope cs(&lock_);
  return send_time_history_.GetOutstandingData(local_net_id_, remote_net_id_)
      .bytes();
}
}  // namespace webrtc
