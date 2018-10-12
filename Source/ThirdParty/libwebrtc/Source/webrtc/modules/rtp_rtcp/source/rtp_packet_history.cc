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
namespace {
// Min packet size for BestFittingPacket() to honor.
constexpr size_t kMinPacketRequestBytes = 50;

// Utility function to get the absolute difference in size between the provided
// target size and the size of packet.
size_t SizeDiff(size_t packet_size, size_t size) {
  if (packet_size > size) {
    return packet_size - size;
  }
  return size - packet_size;
}
}  // namespace

constexpr size_t RtpPacketHistory::kMaxCapacity;
constexpr int64_t RtpPacketHistory::kMinPacketDurationMs;
constexpr int RtpPacketHistory::kMinPacketDurationRtt;
constexpr int RtpPacketHistory::kPacketCullingDelayFactor;

RtpPacketHistory::PacketState::PacketState() = default;
RtpPacketHistory::PacketState::PacketState(const PacketState&) = default;
RtpPacketHistory::PacketState::~PacketState() = default;

RtpPacketHistory::StoredPacket::StoredPacket() = default;
RtpPacketHistory::StoredPacket::StoredPacket(StoredPacket&&) = default;
RtpPacketHistory::StoredPacket& RtpPacketHistory::StoredPacket::operator=(
    RtpPacketHistory::StoredPacket&&) = default;
RtpPacketHistory::StoredPacket::~StoredPacket() = default;

RtpPacketHistory::RtpPacketHistory(Clock* clock)
    : clock_(clock),
      number_to_store_(0),
      mode_(StorageMode::kDisabled),
      rtt_ms_(-1) {}

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
}

void RtpPacketHistory::PutRtpPacket(std::unique_ptr<RtpPacketToSend> packet,
                                    StorageType type,
                                    absl::optional<int64_t> send_time_ms) {
  RTC_DCHECK(packet);
  rtc::CritScope cs(&lock_);
  int64_t now_ms = clock_->TimeInMilliseconds();
  if (mode_ == StorageMode::kDisabled) {
    return;
  }

  CullOldPackets(now_ms);

  // Store packet.
  const uint16_t rtp_seq_no = packet->SequenceNumber();
  StoredPacket& stored_packet = packet_history_[rtp_seq_no];
  RTC_DCHECK(stored_packet.packet == nullptr);
  stored_packet.packet = std::move(packet);

  if (stored_packet.packet->capture_time_ms() <= 0) {
    stored_packet.packet->set_capture_time_ms(now_ms);
  }
  stored_packet.send_time_ms = send_time_ms;
  stored_packet.storage_type = type;
  stored_packet.times_retransmitted = 0;

  if (!start_seqno_) {
    start_seqno_ = rtp_seq_no;
  }
  // Store the sequence number of the last send packet with this size.
  if (type != StorageType::kDontRetransmit) {
    packet_size_[stored_packet.packet->size()] = rtp_seq_no;
  }
}

std::unique_ptr<RtpPacketToSend> RtpPacketHistory::GetPacketAndSetSendTime(
    uint16_t sequence_number,
    bool verify_rtt) {
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
  if (verify_rtt && !VerifyRtt(rtp_it->second, now_ms)) {
    return nullptr;
  }

  if (packet.send_time_ms) {
    ++packet.times_retransmitted;
  }

  // Update send-time and return copy of packet instance.
  packet.send_time_ms = now_ms;

  if (packet.storage_type == StorageType::kDontRetransmit) {
    // Non retransmittable packet, so call must come from paced sender.
    // Remove from history and return actual packet instance.
    return RemovePacket(rtp_it);
  }
  return absl::make_unique<RtpPacketToSend>(*packet.packet);
}

absl::optional<RtpPacketHistory::PacketState> RtpPacketHistory::GetPacketState(
    uint16_t sequence_number,
    bool verify_rtt) const {
  rtc::CritScope cs(&lock_);
  if (mode_ == StorageMode::kDisabled) {
    return absl::nullopt;
  }

  auto rtp_it = packet_history_.find(sequence_number);
  if (rtp_it == packet_history_.end()) {
    return absl::nullopt;
  }

  if (verify_rtt && !VerifyRtt(rtp_it->second, clock_->TimeInMilliseconds())) {
    return absl::nullopt;
  }

  return StoredPacketToPacketState(rtp_it->second);
}

bool RtpPacketHistory::VerifyRtt(const RtpPacketHistory::StoredPacket& packet,
                                 int64_t now_ms) const {
  if (packet.send_time_ms) {
    // Send-time already set, this check must be for a retransmission.
    if (packet.times_retransmitted > 0 &&
        now_ms < *packet.send_time_ms + rtt_ms_) {
      // This packet has already been retransmitted once, and the time since
      // that even is lower than on RTT. Ignore request as this packet is
      // likely already in the network pipe.
      return false;
    }
  }

  return true;
}

std::unique_ptr<RtpPacketToSend> RtpPacketHistory::GetBestFittingPacket(
    size_t packet_length) const {
  // TODO(sprang): Make this smarter, taking retransmit count etc into account.
  rtc::CritScope cs(&lock_);
  if (packet_length < kMinPacketRequestBytes || packet_size_.empty()) {
    return nullptr;
  }

  auto size_iter_upper = packet_size_.upper_bound(packet_length);
  auto size_iter_lower = size_iter_upper;
  if (size_iter_upper == packet_size_.end()) {
    --size_iter_upper;
  }
  if (size_iter_lower != packet_size_.begin()) {
    --size_iter_lower;
  }
  const size_t upper_bound_diff =
      SizeDiff(size_iter_upper->first, packet_length);
  const size_t lower_bound_diff =
      SizeDiff(size_iter_lower->first, packet_length);

  const uint16_t seq_no = upper_bound_diff < lower_bound_diff
                              ? size_iter_upper->second
                              : size_iter_lower->second;
  RtpPacketToSend* best_packet =
      packet_history_.find(seq_no)->second.packet.get();
  return absl::make_unique<RtpPacketToSend>(*best_packet);
}

void RtpPacketHistory::Reset() {
  packet_history_.clear();
  packet_size_.clear();
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
    if (!stored_packet.send_time_ms) {
      // Don't remove packets that have not been sent.
      return;
    }

    if (*stored_packet.send_time_ms + packet_duration_ms > now_ms) {
      // Don't cull packets too early to avoid failed retransmission requests.
      return;
    }

    if (packet_history_.size() >= number_to_store_ ||
        (mode_ == StorageMode::kStoreAndCull &&
         *stored_packet.send_time_ms +
                 (packet_duration_ms * kPacketCullingDelayFactor) <=
             now_ms)) {
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
      std::move(packet_it->second.packet);
  // Erase the packet from the map, and capture iterator to the next one.
  StoredPacketIterator next_it = packet_history_.erase(packet_it);

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

  auto size_iterator = packet_size_.find(rtp_packet->size());
  if (size_iterator != packet_size_.end() &&
      size_iterator->second == rtp_packet->SequenceNumber()) {
    packet_size_.erase(size_iterator);
  }

  return rtp_packet;
}

RtpPacketHistory::PacketState RtpPacketHistory::StoredPacketToPacketState(
    const RtpPacketHistory::StoredPacket& stored_packet) {
  RtpPacketHistory::PacketState state;
  state.rtp_sequence_number = stored_packet.packet->SequenceNumber();
  state.send_time_ms = stored_packet.send_time_ms;
  state.capture_time_ms = stored_packet.packet->capture_time_ms();
  state.ssrc = stored_packet.packet->Ssrc();
  state.payload_size = stored_packet.packet->size();
  state.times_retransmitted = stored_packet.times_retransmitted;
  return state;
}

}  // namespace webrtc
