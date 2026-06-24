/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/sframe_packet_buffer.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <variant>

#include "absl/strings/string_view.h"
#include "modules/rtp_rtcp/source/sframe_descriptor.h"
#include "modules/rtp_rtcp/source/sframe_rtp_packet_received.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

SFramePacketBuffer::SFramePacketBuffer(size_t buffer_size)
    : buffer_(buffer_size) {
  // Buffer size must always be a power of 2.
  RTC_DCHECK(std::has_single_bit(buffer_size));
}

SFramePacketBuffer::~SFramePacketBuffer() = default;

std::variant<SFramePacketBuffer::AssembledFrame,
             SFramePacketBuffer::InsertResult>
SFramePacketBuffer::InsertPacket(
    std::unique_ptr<SframeRtpPacketReceived> packet) {
  int64_t seq_num = seq_num_unwrapper_.Unwrap(packet->SequenceNumber());
  bool buffer_cleared = false;

  // Adjust the receive window and reject packets that are too old.
  if (!UpdateWindowStart(seq_num)) {
    return InsertResult::kNoFrame;  // Too old, drop.
  }

  // Find or make room for this packet in the circular buffer.
  auto [outcome, slot] = ResolveSlot(seq_num);
  switch (outcome) {
    case SlotOutcome::kDuplicate:
      return InsertResult::kNoFrame;
    case SlotOutcome::kFull:
      // Buffer full — clear and reclaim the slot.
      RTC_LOG(LS_WARNING) << "SFramePacketBuffer full, clearing.";
      Clear();
      buffer_cleared = true;
      first_seq_num_ = seq_num;
      slot = ToIdx(seq_num);
      break;
    case SlotOutcome::kOk:
      break;
  }

  buffer_[slot] = std::move(packet);

  if (buffer_cleared) {
    return InsertResult::kBufferCleared;
  }

  // Check whether this insertion completes a frame.
  if (std::optional<AssembledFrame> frame = FindFrame(seq_num)) {
    return std::move(*frame);
  }

  return InsertResult::kNoFrame;
}

std::pair<SFramePacketBuffer::SlotOutcome, size_t>
SFramePacketBuffer::ResolveSlot(int64_t seq_num) {
  size_t idx = ToIdx(seq_num);

  if (!buffer_[idx]) {
    return {SlotOutcome::kOk, idx};  // Empty slot.
  }

  if (buffer_[idx]->SequenceNumber() == static_cast<uint16_t>(seq_num)) {
    return {SlotOutcome::kDuplicate, 0};  // Duplicate.
  }

  // Slot collision with a different seq num — buffer is full.
  return {SlotOutcome::kFull, 0};
}

std::optional<SFramePacketBuffer::AssembledFrame> SFramePacketBuffer::FindFrame(
    int64_t seq_num) {
  // Walk backward to find the S=1 (start-of-frame) packet.
  std::optional<int64_t> start = FindFrameStart(seq_num);
  if (!start) {
    return std::nullopt;
  }

  // Walk forward to find the E=1 (end-of-frame) packet.
  std::optional<int64_t> end = FindFrameEnd(seq_num, *start);
  if (!end) {
    return std::nullopt;
  }

  // Validate consistency and extract the complete frame.
  return AssembleFrame(*start, *end);
}

std::optional<int64_t> SFramePacketBuffer::FindFrameStart(int64_t seq_num) {
  // Walk backward one packet at a time from the insertion point.
  int64_t start = seq_num;
  for (size_t steps = 0; steps < buffer_.size(); ++steps) {
    size_t idx = ToIdx(start);

    // Slot is empty — packet hasn't arrived yet.
    if (!buffer_[idx]) {
      return std::nullopt;
    }

    // Slot holds a packet from a different sequence number (modulo aliasing).
    if (buffer_[idx]->SequenceNumber() != static_cast<uint16_t>(start)) {
      return std::nullopt;
    }

    // Reached the first packet of the frame (S-bit set).
    if (buffer_[idx]->descriptor().start) {
      return start;
    }

    // Reached the oldest packet in our window without finding S-bit.
    if (start == *first_seq_num_) {
      return std::nullopt;
    }

    --start;  // Continue walking backward.
  }

  return std::nullopt;
}

std::optional<int64_t> SFramePacketBuffer::FindFrameEnd(int64_t seq_num,
                                                        int64_t start) {
  // Walk forward one packet at a time from the insertion point.
  int64_t end = seq_num;
  while (true) {
    size_t idx = ToIdx(end);

    // Slot is empty — packet hasn't arrived yet.
    if (!buffer_[idx]) {
      return std::nullopt;
    }

    // Slot holds a packet from a different sequence number (modulo aliasing).
    if (buffer_[idx]->SequenceNumber() != static_cast<uint16_t>(end)) {
      return std::nullopt;
    }

    // Reached the last packet of the frame (E-bit set).
    if (buffer_[idx]->descriptor().end) {
      return end;
    }

    ++end;  // Continue walking forward.

    // Prevent scanning beyond the buffer capacity.
    if (end - start >= static_cast<int64_t>(buffer_.size())) {
      return std::nullopt;
    }
  }
}

std::optional<SFramePacketBuffer::AssembledFrame>
SFramePacketBuffer::AssembleFrame(int64_t start, int64_t end) {
  // Number of packets in the [start, end] range.
  const int64_t frame_size = end - start + 1;

  // Use the first packet as reference for consistency checks.
  const SframeRtpPacketReceived& first_pkt = *buffer_[ToIdx(start)];
  const SframeEncryptionLevel encryption_level =
      first_pkt.descriptor().encryption_level;
  const uint8_t pt = first_pkt.PayloadType();
  const uint32_t ts = first_pkt.Timestamp();

  // Verify that all packets in the frame share the same T-bit, PT, and
  // timestamp. A mismatch means corrupted or spurious packets slipped in.
  int64_t s = start;
  for (int64_t i = 0; i < frame_size; ++i, ++s) {
    const SframeRtpPacketReceived& pkt = *buffer_[ToIdx(s)];
    if (pkt.descriptor().encryption_level != encryption_level) {
      DropFrame("T-bit mismatch", pkt.SequenceNumber(), start, end);
      return std::nullopt;
    }

    if (pkt.PayloadType() != pt) {
      DropFrame("payload type mismatch", pkt.SequenceNumber(), start, end);
      return std::nullopt;
    }

    if (pkt.Timestamp() != ts) {
      DropFrame("timestamp mismatch", pkt.SequenceNumber(), start, end);
      return std::nullopt;
    }
  }

  // Validation passed — move packets out of the buffer into the result.
  AssembledFrame result;
  result.encryption_level = encryption_level;
  result.packets.reserve(frame_size);

  s = start;
  for (int64_t i = 0; i < frame_size; ++i, ++s) {
    size_t idx = ToIdx(s);
    result.packets.push_back(buffer_[idx]->TakePacket());
    buffer_[idx].reset();
  }

  return result;
}

void SFramePacketBuffer::ClearTo(uint16_t seq_num) {
  // Nothing to clear if the buffer hasn't been used yet.
  if (!first_seq_num_) {
    return;
  }

  int64_t unwrapped = seq_num_unwrapper_.PeekUnwrap(seq_num);

  // Ignore if the window already starts past the requested point.
  if (*first_seq_num_ > unwrapped) {
    return;
  }

  // ClearTo is inclusive, so advance one past the target.
  int64_t target = unwrapped + 1;

  // Walk from the current window start to the target, clearing slots whose
  // packets fall within the cleared range.  Only reset slots that actually
  // belong to the old range (a slot may hold a newer packet due to modulo
  // aliasing).
  int64_t diff = target - *first_seq_num_;
  int64_t iterations = std::min(diff, static_cast<int64_t>(buffer_.size()));
  int64_t cur = *first_seq_num_;
  for (int64_t i = 0; i < iterations; ++i) {
    auto& slot = buffer_[ToIdx(cur)];
    if (slot && slot->SequenceNumber() == static_cast<uint16_t>(cur)) {
      slot.reset();
    }
    ++cur;
  }

  // Advance the window start and lock it so late packets can't move it back.
  first_seq_num_ = target;
  is_cleared_to_first_seq_num_ = true;
}

void SFramePacketBuffer::Clear() {
  for (auto& slot : buffer_) {
    slot.reset();
  }
  first_seq_num_.reset();
  is_cleared_to_first_seq_num_ = false;
}

void SFramePacketBuffer::DropFrame(absl::string_view reason,
                                   uint16_t bad_seq,
                                   int64_t start,
                                   int64_t end) {
  RTC_LOG(LS_WARNING) << "SFramePacketBuffer: " << reason << ", seq=" << bad_seq
                      << ". Dropping frame [" << start << ".." << end << "].";
  const int64_t frame_size = end - start + 1;
  int64_t s = start;
  for (int64_t i = 0; i < frame_size; ++i, ++s) {
    buffer_[ToIdx(s)].reset();
  }
}

bool SFramePacketBuffer::UpdateWindowStart(int64_t seq_num) {
  // First packet ever — initialize the window.
  if (!first_seq_num_) {
    first_seq_num_ = seq_num;
  } else if (*first_seq_num_ > seq_num) {
    // Packet arrived before our current window start (reordered).

    // ClearTo was called — the window start was explicitly set and we must
    // not move it backward.
    if (is_cleared_to_first_seq_num_) {
      return false;
    }

    // Packet is so old it wouldn't fit in the buffer.
    if (*first_seq_num_ - seq_num >= static_cast<int64_t>(buffer_.size())) {
      return false;
    }

    // Extend the window backward to include this reordered packet.
    first_seq_num_ = seq_num;
  }
  return true;
}

}  // namespace webrtc
