/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/transport_feedback_packet_loss_tracker.h"

#include <limits>
#include <utility>

#include "webrtc/base/checks.h"
#include "webrtc/base/mod_ops.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"

namespace {
constexpr uint16_t kSeqNumHalf = 0x8000u;
void UpdateCounter(size_t* counter, bool increment) {
  if (increment) {
    RTC_DCHECK_LT(*counter, std::numeric_limits<std::size_t>::max());
    ++(*counter);
  } else {
    RTC_DCHECK_GT(*counter, 0);
    --(*counter);
  }
}

}  // namespace

namespace webrtc {

TransportFeedbackPacketLossTracker::TransportFeedbackPacketLossTracker(
    size_t max_acked_packets,
    size_t plr_min_num_acked_packets,
    size_t rplr_min_num_acked_pairs)
    : max_acked_packets_(max_acked_packets),
      ref_packet_status_(packet_status_window_.begin()),
      plr_state_(plr_min_num_acked_packets),
      rplr_state_(rplr_min_num_acked_pairs) {
  RTC_DCHECK_GT(plr_min_num_acked_packets, 0);
  RTC_DCHECK_GE(max_acked_packets, plr_min_num_acked_packets);
  RTC_DCHECK_LE(max_acked_packets, kSeqNumHalf);
  RTC_DCHECK_GT(rplr_min_num_acked_pairs, 0);
  RTC_DCHECK_GT(max_acked_packets, rplr_min_num_acked_pairs);
  Reset();
}

void TransportFeedbackPacketLossTracker::Reset() {
  acked_packets_ = 0;
  plr_state_.Reset();
  rplr_state_.Reset();
  packet_status_window_.clear();
  ref_packet_status_ = packet_status_window_.begin();
}

uint16_t TransportFeedbackPacketLossTracker::ReferenceSequenceNumber() const {
  RTC_DCHECK(!packet_status_window_.empty());
  return ref_packet_status_->first;
}

uint16_t TransportFeedbackPacketLossTracker::NewestSequenceNumber() const {
  RTC_DCHECK(!packet_status_window_.empty());
  return PreviousPacketStatus(packet_status_window_.end())->first;
}

void TransportFeedbackPacketLossTracker::OnPacketAdded(uint16_t seq_num) {
  if (packet_status_window_.find(seq_num) != packet_status_window_.end() ||
      (!packet_status_window_.empty() &&
       ForwardDiff(seq_num, NewestSequenceNumber()) <= kSeqNumHalf)) {
    // The only way for these two to happen is when the stream lies dormant for
    // long enough for the sequence numbers to wrap. Everything in the window in
    // such a case would be too old to use.
    Reset();
  }

  // Shift older packets out of window.
  while (!packet_status_window_.empty() &&
         ForwardDiff(ref_packet_status_->first, seq_num) >= kSeqNumHalf) {
    RemoveOldestPacketStatus();
  }

  packet_status_window_.insert(packet_status_window_.end(),
                               std::make_pair(seq_num, PacketStatus::Unacked));

  if (packet_status_window_.size() == 1) {
    ref_packet_status_ = packet_status_window_.cbegin();
  }
}

void TransportFeedbackPacketLossTracker::OnReceivedTransportFeedback(
    const rtcp::TransportFeedback& feedback) {
  const auto& fb_vector = feedback.GetStatusVector();
  const uint16_t base_seq_num = feedback.GetBaseSequence();

  uint16_t seq_num = base_seq_num;
  for (size_t i = 0; i < fb_vector.size(); ++i, ++seq_num) {
    const auto& it = packet_status_window_.find(seq_num);

    // Packets which aren't at least marked as unacked either do not belong to
    // this media stream, or have been shifted out of window.
    if (it != packet_status_window_.end()) {
      const bool received = fb_vector[i] !=
          webrtc::rtcp::TransportFeedback::StatusSymbol::kNotReceived;
      RecordFeedback(it, received);
    }
  }
}

rtc::Optional<float>
TransportFeedbackPacketLossTracker::GetPacketLossRate() const {
  return plr_state_.GetMetric();
}

rtc::Optional<float>
TransportFeedbackPacketLossTracker::GetRecoverablePacketLossRate() const {
  return rplr_state_.GetMetric();
}

void TransportFeedbackPacketLossTracker::RecordFeedback(
    PacketStatusMap::iterator it,
    bool received) {
  if (it->second != PacketStatus::Unacked) {
    // Normally, packets are sent (inserted into window as "unacked"), then we
    // receive one feedback for them.
    // But it is possible that a packet would receive two feedbacks. Then:
    if (it->second == PacketStatus::Lost && received) {
      // If older status said that the packet was lost but newer one says it
      // is received, we take the newer one.
      UpdateMetrics(it, false);
      it->second = PacketStatus::Unacked;  // For clarity; overwritten shortly.
    } else {
      // If the value is unchanged or if older status said that the packet was
      // received but the newer one says it is lost, we ignore it.
      // The standard allows for previously-reported packets to carry
      // no report when the reports overlap, which also looks like the
      // packet is being reported as lost.
      return;
    }
  }

  // Change from UNACKED to RECEIVED/LOST.
  it->second = received ? PacketStatus::Received : PacketStatus::Lost;
  UpdateMetrics(it, true);

  // Remove packets from the beginning of the window until maximum-acked
  // is observed again. Note that multiple sent-but-unacked packets might
  // be removed before we reach the first acked (whether as received or as
  // lost) packet.
  while (acked_packets_ > max_acked_packets_)
    RemoveOldestPacketStatus();
}

void TransportFeedbackPacketLossTracker::RemoveOldestPacketStatus() {
  UpdateMetrics(ref_packet_status_, false);
  const auto it = ref_packet_status_;
  ref_packet_status_ = NextPacketStatus(it);
  packet_status_window_.erase(it);
}

void TransportFeedbackPacketLossTracker::UpdateMetrics(
    ConstPacketStatusIterator it,
    bool apply /* false = undo */) {
  RTC_DCHECK(it != packet_status_window_.end());
  // Metrics are dependent on feedbacks from the other side. We don't want
  // to update the metrics each time a packet is sent, except for the case
  // when it shifts old sent-but-unacked-packets out of window.
  RTC_DCHECK(!apply || it->second != PacketStatus::Unacked);

  if (it->second != PacketStatus::Unacked) {
    UpdateCounter(&acked_packets_, apply);
  }

  UpdatePlr(it, apply);
  UpdateRplr(it, apply);
}

void TransportFeedbackPacketLossTracker::UpdatePlr(
    ConstPacketStatusIterator it,
    bool apply /* false = undo */) {
  switch (it->second) {
    case PacketStatus::Unacked:
      return;
    case PacketStatus::Received:
      UpdateCounter(&plr_state_.num_received_packets_, apply);
      break;
    case PacketStatus::Lost:
      UpdateCounter(&plr_state_.num_lost_packets_, apply);
      break;
    default:
      RTC_NOTREACHED();
  }
}

void TransportFeedbackPacketLossTracker::UpdateRplr(
    ConstPacketStatusIterator it,
    bool apply /* false = undo */) {
  if (it->second == PacketStatus::Unacked) {
    // Unacked packets cannot compose a pair.
    return;
  }

  // Previous packet and current packet might compose a pair.
  if (it != ref_packet_status_) {
    const auto& prev = PreviousPacketStatus(it);
    if (prev->second != PacketStatus::Unacked) {
      UpdateCounter(&rplr_state_.num_acked_pairs_, apply);
      if (prev->second == PacketStatus::Lost &&
          it->second == PacketStatus::Received) {
        UpdateCounter(
            &rplr_state_.num_recoverable_losses_, apply);
      }
    }
  }

  // Current packet and next packet might compose a pair.
  const auto& next = NextPacketStatus(it);
  if (next != packet_status_window_.end() &&
      next->second != PacketStatus::Unacked) {
    UpdateCounter(&rplr_state_.num_acked_pairs_, apply);
    if (it->second == PacketStatus::Lost &&
        next->second == PacketStatus::Received) {
      UpdateCounter(&rplr_state_.num_recoverable_losses_, apply);
    }
  }
}

TransportFeedbackPacketLossTracker::ConstPacketStatusIterator
TransportFeedbackPacketLossTracker::PreviousPacketStatus(
    ConstPacketStatusIterator it) const {
  RTC_DCHECK(it != ref_packet_status_);
  if (it == packet_status_window_.end()) {
    // This is to make PreviousPacketStatus(packet_status_window_.end()) point
    // to the last element.
    it = ref_packet_status_;
  }

  if (it == packet_status_window_.begin()) {
    // Due to the circular nature of sequence numbers, we let the iterator
    // go to the end.
    it = packet_status_window_.end();
  }
  return --it;
}

TransportFeedbackPacketLossTracker::ConstPacketStatusIterator
TransportFeedbackPacketLossTracker::NextPacketStatus(
    ConstPacketStatusIterator it) const {
  RTC_DCHECK(it != packet_status_window_.end());
  ++it;
  if (it == packet_status_window_.end()) {
    // Due to the circular nature of sequence numbers, we let the iterator
    // goes back to the beginning.
    it = packet_status_window_.begin();
  }
  if (it == ref_packet_status_) {
    // This is to make the NextPacketStatus of the last element to return the
    // beyond-the-end iterator.
    it = packet_status_window_.end();
  }
  return it;
}

// TODO(minyue): This method checks the states of this class do not misbehave.
// The method is used both in unit tests and a fuzzer test. The fuzzer test
// is present to help finding potential errors. Once the fuzzer test shows no
// error after long period, we can remove the fuzzer test, and move this method
// to unit test.
void TransportFeedbackPacketLossTracker::Validate() const {  // Testing only!
  RTC_CHECK_EQ(plr_state_.num_received_packets_ + plr_state_.num_lost_packets_,
               acked_packets_);
  RTC_CHECK_LE(acked_packets_, packet_status_window_.size());
  RTC_CHECK_LE(acked_packets_, max_acked_packets_);
  RTC_CHECK_LE(rplr_state_.num_recoverable_losses_,
               rplr_state_.num_acked_pairs_);
  RTC_CHECK_LE(rplr_state_.num_acked_pairs_, acked_packets_ - 1);

  size_t unacked_packets = 0;
  size_t received_packets = 0;
  size_t lost_packets = 0;
  size_t acked_pairs = 0;
  size_t recoverable_losses = 0;

  if (!packet_status_window_.empty()) {
    ConstPacketStatusIterator it = ref_packet_status_;
    do {
      switch (it->second) {
        case PacketStatus::Unacked:
          ++unacked_packets;
          break;
        case PacketStatus::Received:
          ++received_packets;
          break;
        case PacketStatus::Lost:
          ++lost_packets;
          break;
        default:
          RTC_NOTREACHED();
      }

      auto next = std::next(it);
      if (next == packet_status_window_.end())
        next = packet_status_window_.begin();

      if (next != ref_packet_status_ &&
          it->second != PacketStatus::Unacked &&
          next->second != PacketStatus::Unacked) {
        ++acked_pairs;
        if (it->second == PacketStatus::Lost &&
            next->second == PacketStatus::Received)
          ++recoverable_losses;
      }

      RTC_CHECK_LT(ForwardDiff(ReferenceSequenceNumber(), it->first),
                   kSeqNumHalf);

      it = next;
    } while (it != ref_packet_status_);
  }

  RTC_CHECK_EQ(plr_state_.num_received_packets_, received_packets);
  RTC_CHECK_EQ(plr_state_.num_lost_packets_, lost_packets);
  RTC_CHECK_EQ(packet_status_window_.size(),
               unacked_packets + received_packets + lost_packets);
  RTC_CHECK_EQ(rplr_state_.num_acked_pairs_, acked_pairs);
  RTC_CHECK_EQ(rplr_state_.num_recoverable_losses_, recoverable_losses);
}

rtc::Optional<float>
TransportFeedbackPacketLossTracker::PlrState::GetMetric() const {
  const size_t total = num_lost_packets_ + num_received_packets_;
  if (total < min_num_acked_packets_) {
    return rtc::Optional<float>();
  } else {
    return rtc::Optional<float>(
        static_cast<float>(num_lost_packets_) / total);
  }
}

rtc::Optional<float>
TransportFeedbackPacketLossTracker::RplrState::GetMetric() const {
  if (num_acked_pairs_ < min_num_acked_pairs_) {
    return rtc::Optional<float>();
  } else {
    return rtc::Optional<float>(
        static_cast<float>(num_recoverable_losses_) / num_acked_pairs_);
  }
}

}  // namespace webrtc
