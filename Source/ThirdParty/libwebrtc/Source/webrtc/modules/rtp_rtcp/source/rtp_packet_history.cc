/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_packet_history.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "absl/memory/memory.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

constexpr size_t RtpPacketHistory::kMaxCapacity;
constexpr int64_t RtpPacketHistory::kMinPacketDurationMs;
constexpr int RtpPacketHistory::kMinPacketDurationRtt;
constexpr int RtpPacketHistory::kPacketCullingDelayFactor;

RtpPacketHistory::PacketState::PacketState() = default;
RtpPacketHistory::PacketState::PacketState(const PacketState&) = default;
RtpPacketHistory::PacketState::~PacketState() = default;

RtpPacketHistory::StoredPacket::StoredPacket(
    std::unique_ptr<RtpPacketToSend> packet,
    absl::optional<int64_t> send_time_ms,
    uint64_t insert_order)
    : send_time_ms_(send_time_ms),
      packet_(std::move(packet)),
      // No send time indicates packet is not sent immediately, but instead will
      // be put in the pacer queue and later retrieved via
      // GetPacketAndSetSendTime().
      pending_transmission_(!send_time_ms.has_value()),
      insert_order_(insert_order),
      times_retransmitted_(0) {}

RtpPacketHistory::StoredPacket::StoredPacket(StoredPacket&&) = default;
RtpPacketHistory::StoredPacket& RtpPacketHistory::StoredPacket::operator=(
    RtpPacketHistory::StoredPacket&&) = default;
RtpPacketHistory::StoredPacket::~StoredPacket() = default;

void RtpPacketHistory::StoredPacket::IncrementTimesRetransmitted(
    PacketPrioritySet* priority_set) {
  // Check if this StoredPacket is in the priority set. If so, we need to remove
  // it before updating |times_retransmitted_| since that is used in sorting,
  // and then add it back.
  const bool in_priority_set = priority_set->erase(this) > 0;
  ++times_retransmitted_;
  if (in_priority_set) {
    auto it = priority_set->insert(this);
    RTC_DCHECK(it.second)
        << "ERROR: Priority set already contains matching packet! In set: "
           "insert order = "
        << (*it.first)->insert_order_
        << ", times retransmitted = " << (*it.first)->times_retransmitted_
        << ". Trying to add: insert order = " << insert_order_
        << ", times retransmitted = " << times_retransmitted_;
  }
}

bool RtpPacketHistory::MoreUseful::operator()(StoredPacket* lhs,
                                              StoredPacket* rhs) const {
  // Prefer to send packets we haven't already sent as padding.
  if (lhs->times_retransmitted() != rhs->times_retransmitted()) {
    return lhs->times_retransmitted() < rhs->times_retransmitted();
  }
  // All else being equal, prefer newer packets.
  return lhs->insert_order() > rhs->insert_order();
}

RtpPacketHistory::RtpPacketHistory(Clock* clock)
    : clock_(clock),
      number_to_store_(0),
      mode_(StorageMode::kDisabled),
      rtt_ms_(-1),
      packets_inserted_(0) {}

RtpPacketHistory::~RtpPacketHistory() {}

void RtpPacketHistory::SetStorePacketsStatus(StorageMode mode,
                                             size_t number_to_store) {
  RTC_DCHECK_LE(number_to_store, kMaxCapacity);
  rtc::CritScope cs(&lock_);
  if (mode != StorageMode::kDisabled && mode_ != StorageMode::kDisabled) {
    RTC_LOG(LS_WARNING) << "Purging packet history in order to re-set status.";
  }
  Reset();
  mode_ = mode;
  number_to_store_ = std::min(kMaxCapacity, number_to_store);
}

RtpPacketHistory::StorageMode RtpPacketHistory::GetStorageMode() const {
  rtc::CritScope cs(&lock_);
  return mode_;
}

void RtpPacketHistory::SetRtt(int64_t rtt_ms) {
  rtc::CritScope cs(&lock_);
  RTC_DCHECK_GE(rtt_ms, 0);
  rtt_ms_ = rtt_ms;
  // If storage is not disabled,  packets will be removed after a timeout
  // that depends on the RTT. Changing the RTT may thus cause some packets
  // become "old" and subject to removal.
  if (mode_ != StorageMode::kDisabled) {
    CullOldPackets(clock_->TimeInMilliseconds());
  }
}

void RtpPacketHistory::PutRtpPacket(std::unique_ptr<RtpPacketToSend> packet,
                                    absl::optional<int64_t> send_time_ms) {
  RTC_DCHECK(packet);
  rtc::CritScope cs(&lock_);
  int64_t now_ms = clock_->TimeInMilliseconds();
  if (mode_ == StorageMode::kDisabled) {
    return;
  }

  RTC_DCHECK(packet->allow_retransmission());
  CullOldPackets(now_ms);

  // Store packet.
  const uint16_t rtp_seq_no = packet->SequenceNumber();
  auto packet_it = packet_history_.emplace(
      rtp_seq_no,
      StoredPacket(std::move(packet), send_time_ms, packets_inserted_++));
  RTC_DCHECK(packet_it.second) << "Failed to insert packet in history.";
  StoredPacket& stored_packet = packet_it.first->second;

  if (!start_seqno_) {
    start_seqno_ = rtp_seq_no;
  }

  // Store the sequence number of the last send packet with this size.
  auto prio_it = padding_priority_.insert(&stored_packet);
  RTC_DCHECK(prio_it.second) << "Failed to insert packet into prio set.";
}

std::unique_ptr<RtpPacketToSend> RtpPacketHistory::GetPacketAndSetSendTime(
    uint16_t sequence_number) {
  rtc::CritScope cs(&lock_);
  if (mode_ == StorageMode::kDisabled) {
    return nullptr;
  }

  int64_t now_ms = clock_->TimeInMilliseconds();
  StoredPacketIterator rtp_it = packet_history_.find(sequence_number);
  if (rtp_it == packet_history_.end()) {
    return nullptr;
  }

  StoredPacket& packet = rtp_it->second;
  if (!VerifyRtt(rtp_it->second, now_ms)) {
    return nullptr;
  }

  if (packet.send_time_ms_) {
    packet.IncrementTimesRetransmitted(&padding_priority_);
  }

  // Update send-time and mark as no long in pacer queue.
  packet.send_time_ms_ = now_ms;
  packet.pending_transmission_ = false;

  // Return copy of packet instance since it may need to be retransmitted again.
  return absl::make_unique<RtpPacketToSend>(*packet.packet_);
}

std::unique_ptr<RtpPacketToSend> RtpPacketHistory::GetPacketAndMarkAsPending(
    uint16_t sequence_number) {
  return GetPacketAndMarkAsPending(
      sequence_number, [](const RtpPacketToSend& packet) {
        return absl::make_unique<RtpPacketToSend>(packet);
      });
}

std::unique_ptr<RtpPacketToSend> RtpPacketHistory::GetPacketAndMarkAsPending(
    uint16_t sequence_number,
    rtc::FunctionView<std::unique_ptr<RtpPacketToSend>(const RtpPacketToSend&)>
        encapsulate) {
  rtc::CritScope cs(&lock_);
  if (mode_ == StorageMode::kDisabled) {
    return nullptr;
  }

  int64_t now_ms = clock_->TimeInMilliseconds();
  StoredPacketIterator rtp_it = packet_history_.find(sequence_number);
  if (rtp_it == packet_history_.end()) {
    return nullptr;
  }

  StoredPacket& packet = rtp_it->second;

  if (packet.pending_transmission_) {
    // Packet already in pacer queue, ignore this request.
    return nullptr;
  }

  if (!VerifyRtt(rtp_it->second, now_ms)) {
    // Packet already resent within too short a time window, ignore.
    return nullptr;
  }

  // Copy and/or encapsulate packet.
  std::unique_ptr<RtpPacketToSend> encapsulated_packet =
      encapsulate(*packet.packet_);
  if (encapsulated_packet) {
    packet.pending_transmission_ = true;
  }

  return encapsulated_packet;
}

void RtpPacketHistory::MarkPacketAsSent(uint16_t sequence_number) {
  rtc::CritScope cs(&lock_);
  if (mode_ == StorageMode::kDisabled) {
    return;
  }

  int64_t now_ms = clock_->TimeInMilliseconds();
  StoredPacketIterator rtp_it = packet_history_.find(sequence_number);
  if (rtp_it == packet_history_.end()) {
    return;
  }

  StoredPacket& packet = rtp_it->second;
  RTC_DCHECK(packet.send_time_ms_);

  // Update send-time, mark as no longer in pacer queue, and increment
  // transmission count.
  packet.send_time_ms_ = now_ms;
  packet.pending_transmission_ = false;
  packet.IncrementTimesRetransmitted(&padding_priority_);
}

absl::optional<RtpPacketHistory::PacketState> RtpPacketHistory::GetPacketState(
    uint16_t sequence_number) const {
  rtc::CritScope cs(&lock_);
  if (mode_ == StorageMode::kDisabled) {
    return absl::nullopt;
  }

  auto rtp_it = packet_history_.find(sequence_number);
  if (rtp_it == packet_history_.end()) {
    return absl::nullopt;
  }

  if (!VerifyRtt(rtp_it->second, clock_->TimeInMilliseconds())) {
    return absl::nullopt;
  }

  return StoredPacketToPacketState(rtp_it->second);
}

bool RtpPacketHistory::VerifyRtt(const RtpPacketHistory::StoredPacket& packet,
                                 int64_t now_ms) const {
  if (packet.send_time_ms_) {
    // Send-time already set, this check must be for a retransmission.
    if (packet.times_retransmitted() > 0 &&
        now_ms < *packet.send_time_ms_ + rtt_ms_) {
      // This packet has already been retransmitted once, and the time since
      // that even is lower than on RTT. Ignore request as this packet is
      // likely already in the network pipe.
      return false;
    }
  }

  return true;
}

std::unique_ptr<RtpPacketToSend> RtpPacketHistory::GetPayloadPaddingPacket() {
  // Default implementation always just returns a copy of the packet.
  return GetPayloadPaddingPacket([](const RtpPacketToSend& packet) {
    return absl::make_unique<RtpPacketToSend>(packet);
  });
}

std::unique_ptr<RtpPacketToSend> RtpPacketHistory::GetPayloadPaddingPacket(
    rtc::FunctionView<std::unique_ptr<RtpPacketToSend>(const RtpPacketToSend&)>
        encapsulate) {
  rtc::CritScope cs(&lock_);
  if (mode_ == StorageMode::kDisabled || padding_priority_.empty()) {
    return nullptr;
  }

  auto best_packet_it = padding_priority_.begin();
  StoredPacket* best_packet = *best_packet_it;
  if (best_packet->pending_transmission_) {
    // Because PacedSender releases it's lock when it calls
    // GeneratePadding() there is the potential for a race where a new
    // packet ends up here instead of the regular transmit path. In such a
    // case, just return empty and it will be picked up on the next
    // Process() call.
    return nullptr;
  }

  auto padding_packet = encapsulate(*best_packet->packet_);
  if (!padding_packet) {
    return nullptr;
  }

  best_packet->send_time_ms_ = clock_->TimeInMilliseconds();
  best_packet->IncrementTimesRetransmitted(&padding_priority_);

  return padding_packet;
}

void RtpPacketHistory::CullAcknowledgedPackets(
    rtc::ArrayView<const uint16_t> sequence_numbers) {
  rtc::CritScope cs(&lock_);
  if (mode_ == StorageMode::kDisabled) {
    return;
  }

  for (uint16_t sequence_number : sequence_numbers) {
    auto stored_packet_it = packet_history_.find(sequence_number);
    if (stored_packet_it != packet_history_.end()) {
      RemovePacket(stored_packet_it);
    }
  }
}

bool RtpPacketHistory::SetPendingTransmission(uint16_t sequence_number) {
  rtc::CritScope cs(&lock_);
  if (mode_ == StorageMode::kDisabled) {
    return false;
  }

  auto rtp_it = packet_history_.find(sequence_number);
  if (rtp_it == packet_history_.end()) {
    return false;
  }

  rtp_it->second.pending_transmission_ = true;
  return true;
}

void RtpPacketHistory::Clear() {
  rtc::CritScope cs(&lock_);
  Reset();
}

void RtpPacketHistory::Reset() {
  packet_history_.clear();
  padding_priority_.clear();
  start_seqno_.reset();
}

void RtpPacketHistory::CullOldPackets(int64_t now_ms) {
  int64_t packet_duration_ms =
      std::max(kMinPacketDurationRtt * rtt_ms_, kMinPacketDurationMs);
  while (!packet_history_.empty()) {
    auto stored_packet_it = packet_history_.find(*start_seqno_);
    RTC_DCHECK(stored_packet_it != packet_history_.end());

    if (packet_history_.size() >= kMaxCapacity) {
      // We have reached the absolute max capacity, remove one packet
      // unconditionally.
      RemovePacket(stored_packet_it);
      continue;
    }

    const StoredPacket& stored_packet = stored_packet_it->second;
    if (stored_packet_it->second.pending_transmission_) {
      // Don't remove packets in the pacer queue, pending tranmission.
      return;
    }

    if (*stored_packet.send_time_ms_ + packet_duration_ms > now_ms) {
      // Don't cull packets too early to avoid failed retransmission requests.
      return;
    }

    if (packet_history_.size() >= number_to_store_ ||
        *stored_packet.send_time_ms_ +
                (packet_duration_ms * kPacketCullingDelayFactor) <=
            now_ms) {
      // Too many packets in history, or this packet has timed out. Remove it
      // and continue.
      RemovePacket(stored_packet_it);
    } else {
      // No more packets can be removed right now.
      return;
    }
  }
}

std::unique_ptr<RtpPacketToSend> RtpPacketHistory::RemovePacket(
    StoredPacketIterator packet_it) {
  // Move the packet out from the StoredPacket container.
  std::unique_ptr<RtpPacketToSend> rtp_packet =
      std::move(packet_it->second.packet_);

  // Check if this is the oldest packet in the history, as this must be updated
  // in order to cull old packets.
  const bool is_first_packet = packet_it->first == start_seqno_;

  // Erase from padding priority set, if eligible.
  size_t num_erased = padding_priority_.erase(&packet_it->second);
  RTC_DCHECK_EQ(num_erased, 1)
      << "Failed to remove one packet from prio set, got " << num_erased;
  if (num_erased != 1) {
    RTC_LOG(LS_ERROR) << "RtpPacketHistory in inconsistent state, resetting.";
    Reset();
    return nullptr;
  }

  // Erase the packet from the map, and capture iterator to the next one.
  StoredPacketIterator next_it = packet_history_.erase(packet_it);

  if (is_first_packet) {
    // |next_it| now points to the next element, or to the end. If the end,
    // check if we can wrap around.
    if (next_it == packet_history_.end()) {
      next_it = packet_history_.begin();
    }

    // Update |start_seq_no| to the new oldest item.
    if (next_it != packet_history_.end()) {
      start_seqno_ = next_it->first;
    } else {
      start_seqno_.reset();
    }
  }

  return rtp_packet;
}

RtpPacketHistory::PacketState RtpPacketHistory::StoredPacketToPacketState(
    const RtpPacketHistory::StoredPacket& stored_packet) {
  RtpPacketHistory::PacketState state;
  state.rtp_sequence_number = stored_packet.packet_->SequenceNumber();
  state.send_time_ms = stored_packet.send_time_ms_;
  state.capture_time_ms = stored_packet.packet_->capture_time_ms();
  state.ssrc = stored_packet.packet_->Ssrc();
  state.packet_size = stored_packet.packet_->size();
  state.times_retransmitted = stored_packet.times_retransmitted();
  state.pending_transmission = stored_packet.pending_transmission_;
  return state;
}

}  // namespace webrtc
