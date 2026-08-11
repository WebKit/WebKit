/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_SFRAME_PACKET_BUFFER_H_
#define MODULES_VIDEO_CODING_SFRAME_PACKET_BUFFER_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "absl/strings/string_view.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/sframe_descriptor.h"
#include "modules/rtp_rtcp/source/sframe_rtp_packet_received.h"
#include "rtc_base/numerics/sequence_number_unwrapper.h"

namespace webrtc {

// Buffers incoming SFrame RTP packets and validates complete S->E runs.
//
// Implements the receiver-side validation from
// draft-ietf-avtcore-rtp-sframe section 5.2:
//   1. Add each packet to the set.
//   2. Find the smallest consecutive S=1->E=1 run.
//   3. Validate T-bits are identical, PTs are identical.
//   4. Return the validated packets + metadata.
//
// Uses a circular buffer indexed by (seq_num % buffer_size), similar to
// PacketBuffer, to avoid per-packet heap allocations.
//
// Descriptor stripping, payload assembly, and decryption are handled
// downstream by the packet transformer.
// This buffer sits before SFrame decryption and before the codec depacketizer.
class SFramePacketBuffer {
 public:
  // Outcome of InsertPacket() when no frame was assembled.
  enum class InsertResult {
    // Packet was buffered but no complete frame yet.
    kNoFrame,
    // Buffer was full and had to be cleared. Caller should request a
    // keyframe. Any partially assembled state is discarded.
    kBufferCleared,
  };

  struct AssembledFrame {
    // Encryption granularity from the SFrame descriptor T bit.
    SframeEncryptionLevel encryption_level = SframeEncryptionLevel::kFrame;

    // The RTP packets forming the validated S->E run, in sequence-number
    // order.
    std::vector<std::unique_ptr<RtpPacketReceived>> packets;
  };

  explicit SFramePacketBuffer(size_t buffer_size = 2048);
  ~SFramePacketBuffer();

  // Insert an SFrame-depacketized RTP packet (descriptor already parsed and
  // stripped).  If this packet completes an S->E run, returns an
  // AssembledFrame.  Otherwise returns an InsertResult status.
  std::variant<AssembledFrame, InsertResult> InsertPacket(
      std::unique_ptr<SframeRtpPacketReceived> packet);

  // Discard all packets up to and including `seq_num`.
  void ClearTo(uint16_t seq_num);

  // Clear all buffered state.
  void Clear();

 private:
  // Check whether the newly inserted packet at `seq_num` completes an S->E
  // run. Walks backward from `seq_num` to find S=1, then forward to find E=1.
  std::optional<AssembledFrame> FindFrame(int64_t seq_num);

  // Walk backward from `seq_num` to find the S=1 packet (start of frame).
  std::optional<int64_t> FindFrameStart(int64_t seq_num);

  // Walk forward from `seq_num` to find the E=1 packet (end of frame).
  std::optional<int64_t> FindFrameEnd(int64_t seq_num, int64_t start);

  // Validate the S->E run, extract packets, and remove them from the buffer.
  std::optional<AssembledFrame> AssembleFrame(int64_t start, int64_t end);

  // Clear all packets in [start, end] on validation failure and log `reason`.
  void DropFrame(absl::string_view reason,
                 uint16_t bad_seq,
                 int64_t start,
                 int64_t end);

  // Update first_seq_num_ to track the lowest sequence number in the window.
  // Returns false if the packet is too old to fit in the buffer.
  bool UpdateWindowStart(int64_t seq_num);

  // Outcome of ResolveSlot: found an available slot, detected a duplicate
  // packet, or ran out of buffer space.
  enum class SlotOutcome { kOk, kDuplicate, kFull };

  // Resolve the buffer slot for `seq_num`.  Returns the outcome and the
  // slot index (only valid when kOk).  Does NOT clear the buffer on
  // overflow — the caller decides how to handle that.
  std::pair<SlotOutcome, size_t> ResolveSlot(int64_t seq_num);

  size_t ToIdx(int64_t seq_num) const {
    return static_cast<size_t>(seq_num) % buffer_.size();
  }

  // Circular buffer indexed by ToIdx(unwrapped_seq_num).
  std::vector<std::unique_ptr<SframeRtpPacketReceived>> buffer_;

  std::optional<int64_t> first_seq_num_;

  // True after ClearTo() has been called. Prevents UpdateWindowStart from
  // moving first_seq_num_ backward past the cleared point.
  bool is_cleared_to_first_seq_num_ = false;

  // Unwraps uint16_t RTP sequence numbers to monotonic int64_t values.
  SeqNumUnwrapper<uint16_t> seq_num_unwrapper_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_SFRAME_PACKET_BUFFER_H_
