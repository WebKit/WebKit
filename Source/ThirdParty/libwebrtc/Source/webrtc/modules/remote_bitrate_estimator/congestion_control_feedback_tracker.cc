
/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/remote_bitrate_estimator/congestion_control_feedback_tracker.h"

#include <cstdint>
#include <vector>

#include "api/transport/ecn_marking.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/source/rtcp_packet/congestion_control_feedback.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

constexpr int kMaxPacketsPerSsrc = 16384;
constexpr int kMaxPacketsToKeepForReorderCalculation = 64;

CongestionControlFeedbackTracker::PacketInfo*
CongestionControlFeedbackTracker::FindOrCreatePacketInfo(
    int64_t sequence_number) {
  if (packets_.empty()) {
    // First packet ever, or sequence number reset.
    // To avoid special logic to distinguish out of order packets at the start
    // of the call, and too old packets in the middle of a call, ensure
    // `packets_.size()` is always >= `kMaxPacketsToKeepForReorderCalculation`
    // once at least one packet is received.
    packets_.resize(kMaxPacketsToKeepForReorderCalculation);
    first_sequence_number_in_packets_ =
        sequence_number - kMaxPacketsToKeepForReorderCalculation + 1;
    next_sequence_number_in_feedback_ = sequence_number;
    return &packets_.back();
  }

  if (sequence_number < first_sequence_number_in_packets_) {
    RTC_LOG(LS_VERBOSE) << "Received too old packet ssrc:" << ssrc_
                        << " seq:" << sequence_number << ". Expected seq >= "
                        << first_sequence_number_in_packets_
                        << ". Ignoring the packet.";
    return nullptr;
  }

  if (sequence_number >=
      first_sequence_number_in_packets_ + std::ssize(packets_)) {
    int64_t new_size = sequence_number - first_sequence_number_in_packets_ + 1;
    if (new_size > kMaxPacketsPerSsrc) {
      RTC_LOG(LS_VERBOSE)
          << "Received too new packet ssrc:" << ssrc_
          << " seq:" << sequence_number
          << " that would increase number of packet to report to " << new_size
          << " from current " << packets_.size()
          << ", first seq:" << first_sequence_number_in_packets_
          << ". Ignoring the packet.";
      return nullptr;
    }

    packets_.resize(new_size);
    return &packets_.back();
  }

  // `PacketInfo` for `sequence_number` already exists.
  return &packets_[sequence_number - first_sequence_number_in_packets_];
}

void CongestionControlFeedbackTracker::ReceivedPacket(
    const RtpPacketReceived& packet) {
  RTC_DCHECK_EQ(packet.Ssrc(), ssrc_);

  int64_t sequence_number = unwrapper_.Unwrap(packet.SequenceNumber());
  PacketInfo* info = FindOrCreatePacketInfo(sequence_number);
  if (info == nullptr) {
    ++num_ignored_packets_since_last_feedback_;
    return;
  }

  if (info->received()) {
    // Duplicate copy of an RTP packet is received. According to RFC8888:
    // arrival time of the first copy to arrive MUST be reported.
    // If any of the copies of the duplicated packet are ECN-CE marked, then an
    // ECN-CE mark MUST be reported for that packet; otherwise, the ECN mark of
    // the first copy to arrive is reported.
    if (packet.ecn() == EcnMarking::kCe && info->ecn != EcnMarking::kCe) {
      info->ecn = EcnMarking::kCe;
    } else {
      // No information to report is updated for this packet, so no need to
      // try to report it in the next feedback message.
      return;
    }
  } else {
    // Packet was received for the 1st time.
    info->arrival_time = packet.arrival_time();
    info->ecn = packet.ecn();
  }

  // Newly received packet, or new information about an old packet - ensure such
  // new information is included in the next report.
  if (sequence_number < next_sequence_number_in_feedback_) {
    RTC_LOG(LS_WARNING)
        << "Received packet unorderered between feeedback. SSRC: "
        << packet.Ssrc() << " Seq: " << packet.SequenceNumber()
        << " last feedback: " << next_sequence_number_in_feedback_;
    next_sequence_number_in_feedback_ = sequence_number;
  }
}

void CongestionControlFeedbackTracker::AddPacketsToFeedback(
    Timestamp feedback_time,
    std::vector<rtcp::CongestionControlFeedback::PacketInfo>& packet_feedback) {
  if (packets_.empty()) {
    // No packets received since last reset.
    return;
  }

  RTC_DCHECK_GE(next_sequence_number_in_feedback_,
                first_sequence_number_in_packets_);
  RTC_DCHECK_LE(next_sequence_number_in_feedback_,
                first_sequence_number_in_packets_ + std::ssize(packets_));

  if (next_sequence_number_in_feedback_ ==
      first_sequence_number_in_packets_ + std::ssize(packets_)) {
    // No packets to report received since last produced feedback.
    if (num_ignored_packets_since_last_feedback_ > 0) {
      // There were packets received, but all of them were discarared due to
      // reorder. That likely indicates sequence number reset. Reset the state
      // so that next feedback could be produced.
      RTC_LOG(LS_WARNING)
          << num_ignored_packets_since_last_feedback_
          << " received packets were discarded while no packets were accepted "
             "to produce feedback for SSRC: "
          << ssrc_
          << ". Assuming sequence numbers were reset, "
             "reset state and next sequence number in feedback from "
          << next_sequence_number_in_feedback_;
      // Clear packets_, rest of the state will be reset on first packet
      // arrived after that.
      packets_.clear();
      num_ignored_packets_since_last_feedback_ = 0;
    }
    return;
  }
  num_ignored_packets_since_last_feedback_ = 0;

  uint16_t rtp_sequence_number =
      static_cast<uint16_t>(next_sequence_number_in_feedback_);
  for (auto it = packets_.begin() + (next_sequence_number_in_feedback_ -
                                     first_sequence_number_in_packets_);
       it != packets_.end(); ++it, ++rtp_sequence_number) {
    PacketInfo& info = *it;

    if (!info.received() && !info.last_reported_as_lost) {
      ++stats_.num_packets_reported_lost;
      info.last_reported_as_lost = true;
    }
    if (info.received() && info.last_reported_as_lost) {
      ++stats_.num_packets_reported_recovered;
      info.last_reported_as_lost = false;
    }

    packet_feedback.push_back(
        {.ssrc = ssrc_,
         .sequence_number = rtp_sequence_number,
         .arrival_time_offset = info.received()
                                    ? feedback_time - info.arrival_time
                                    : TimeDelta::MinusInfinity(),
         .ecn = info.ecn});
  }

  next_sequence_number_in_feedback_ =
      first_sequence_number_in_packets_ + std::ssize(packets_);

  // Reduce `packets_` to store just `kMaxKeepForReorderCalculation` latest
  // entries.
  int64_t num_elements_to_erase =
      std::ssize(packets_) - kMaxPacketsToKeepForReorderCalculation;
  first_sequence_number_in_packets_ += num_elements_to_erase;
  packets_.erase(packets_.begin(), packets_.begin() + num_elements_to_erase);
}

}  // namespace webrtc
