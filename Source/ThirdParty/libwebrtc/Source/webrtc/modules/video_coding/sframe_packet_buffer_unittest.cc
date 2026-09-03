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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/sframe_descriptor.h"
#include "modules/rtp_rtcp/source/sframe_rtp_packet_received.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

template <class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

constexpr size_t kBufferSize = 16;

class SFramePacketBufferTest : public ::testing::Test {
 protected:
  SFramePacketBufferTest() : buffer_(kBufferSize) {}

  enum IsStart { kNotStart, kStart };
  enum IsEnd { kNotEnd, kEnd };

  std::unique_ptr<SframeRtpPacketReceived> MakePacket(
      uint16_t seq_num,
      IsStart start,
      IsEnd end,
      uint32_t timestamp = 1000,
      uint8_t payload_type = 96,
      SframeEncryptionLevel encryption_level = SframeEncryptionLevel::kFrame) {
    auto rtp = std::make_unique<RtpPacketReceived>();
    rtp->SetSequenceNumber(seq_num);
    rtp->SetTimestamp(timestamp);
    rtp->SetPayloadType(payload_type);
    SframeDescriptor desc;
    desc.start = (start == kStart);
    desc.end = (end == kEnd);
    desc.encryption_level = encryption_level;
    return std::make_unique<SframeRtpPacketReceived>(std::move(rtp), desc);
  }
  struct TestResult {
    SFramePacketBuffer::InsertResult status =
        SFramePacketBuffer::InsertResult::kNoFrame;
    SframeEncryptionLevel encryption_level = SframeEncryptionLevel::kFrame;
    std::vector<std::unique_ptr<RtpPacketReceived>> packets;
  };

  bool BufferCleared(SFramePacketBuffer::InsertResult r) {
    return r == SFramePacketBuffer::InsertResult::kBufferCleared;
  }

  TestResult InsertInto(SFramePacketBuffer& buf,
                        std::unique_ptr<SframeRtpPacketReceived> packet) {
    auto insert_result = buf.InsertPacket(std::move(packet));
    return std::visit(
        overloaded{
            [](SFramePacketBuffer::AssembledFrame& frame) -> TestResult {
              return {.encryption_level = frame.encryption_level,
                      .packets = std::move(frame.packets)};
            },
            [](SFramePacketBuffer::InsertResult& status) -> TestResult {
              return {.status = status};
            },
        },
        insert_result);
  }

  TestResult Insert(
      uint16_t seq_num,
      IsStart start,
      IsEnd end,
      uint32_t timestamp = 1000,
      uint8_t payload_type = 96,
      SframeEncryptionLevel encryption_level = SframeEncryptionLevel::kFrame) {
    return InsertInto(buffer_, MakePacket(seq_num, start, end, timestamp,
                                          payload_type, encryption_level));
  }

  SFramePacketBuffer buffer_;
};

TEST_F(SFramePacketBufferTest, SinglePacketFrame) {
  auto result = Insert(100, kStart, kEnd);
  ASSERT_EQ(result.packets.size(), 1u);
  EXPECT_EQ(result.packets[0]->SequenceNumber(), 100);
}

TEST_F(SFramePacketBufferTest, TwoPacketFrame) {
  EXPECT_TRUE(Insert(100, kStart, kNotEnd).packets.empty());
  auto result = Insert(101, kNotStart, kEnd);
  ASSERT_EQ(result.packets.size(), 2u);
  EXPECT_EQ(result.packets[0]->SequenceNumber(), 100);
  EXPECT_EQ(result.packets[1]->SequenceNumber(), 101);
}

TEST_F(SFramePacketBufferTest, ThreePacketFrame) {
  EXPECT_TRUE(Insert(200, kStart, kNotEnd).packets.empty());
  EXPECT_TRUE(Insert(201, kNotStart, kNotEnd).packets.empty());
  auto result = Insert(202, kNotStart, kEnd);
  ASSERT_EQ(result.packets.size(), 3u);
  EXPECT_EQ(result.packets[0]->SequenceNumber(), 200);
  EXPECT_EQ(result.packets[1]->SequenceNumber(), 201);
  EXPECT_EQ(result.packets[2]->SequenceNumber(), 202);
}

TEST_F(SFramePacketBufferTest, TwoPacketFrameReversed) {
  EXPECT_TRUE(Insert(101, kNotStart, kEnd).packets.empty());
  auto result = Insert(100, kStart, kNotEnd);
  ASSERT_EQ(result.packets.size(), 2u);
  EXPECT_EQ(result.packets[0]->SequenceNumber(), 100);
  EXPECT_EQ(result.packets[1]->SequenceNumber(), 101);
}

TEST_F(SFramePacketBufferTest, ThreePacketFrameMiddleLast) {
  EXPECT_TRUE(Insert(300, kStart, kNotEnd).packets.empty());
  EXPECT_TRUE(Insert(302, kNotStart, kEnd).packets.empty());
  auto result = Insert(301, kNotStart, kNotEnd);
  ASSERT_EQ(result.packets.size(), 3u);
  EXPECT_EQ(result.packets[0]->SequenceNumber(), 300);
  EXPECT_EQ(result.packets[1]->SequenceNumber(), 301);
  EXPECT_EQ(result.packets[2]->SequenceNumber(), 302);
}

TEST_F(SFramePacketBufferTest, TwoConsecutiveSinglePacketFrames) {
  auto r1 = Insert(50, kStart, kEnd, /*timestamp=*/1000);
  ASSERT_EQ(r1.packets.size(), 1u);

  auto r2 = Insert(51, kStart, kEnd, /*timestamp=*/2000);
  ASSERT_EQ(r2.packets.size(), 1u);
  EXPECT_EQ(r2.packets[0]->SequenceNumber(), 51);
}

TEST_F(SFramePacketBufferTest, TwoFramesBackToBack) {
  // Frame 1: seq 10-11
  EXPECT_TRUE(Insert(10, kStart, kNotEnd, /*timestamp=*/100).packets.empty());
  auto r1 = Insert(11, kNotStart, kEnd, /*timestamp=*/100);
  ASSERT_EQ(r1.packets.size(), 2u);

  // Frame 2: seq 12-13
  EXPECT_TRUE(Insert(12, kStart, kNotEnd, /*timestamp=*/200).packets.empty());
  auto r2 = Insert(13, kNotStart, kEnd, /*timestamp=*/200);
  ASSERT_EQ(r2.packets.size(), 2u);
}

TEST_F(SFramePacketBufferTest, IncompleteFrameMissingEnd) {
  EXPECT_TRUE(Insert(100, kStart, kNotEnd).packets.empty());
  EXPECT_TRUE(Insert(101, kNotStart, kNotEnd).packets.empty());
  // Neither packet completes a frame, but completing the frame later works.
  auto result = Insert(102, kNotStart, kEnd);
  EXPECT_EQ(result.packets.size(), 3u);
}

TEST_F(SFramePacketBufferTest, IncompleteFrameMissingStart) {
  EXPECT_TRUE(Insert(101, kNotStart, kNotEnd).packets.empty());
  EXPECT_TRUE(Insert(102, kNotStart, kEnd).packets.empty());
  // Packets are buffered but no S-bit found, so adding it completes the frame.
  auto result = Insert(100, kStart, kNotEnd);
  EXPECT_EQ(result.packets.size(), 3u);
}

TEST_F(SFramePacketBufferTest, IncompleteFrameGap) {
  // S-bit at 100, gap at 101, E-bit at 102.
  EXPECT_TRUE(Insert(100, kStart, kNotEnd).packets.empty());
  EXPECT_TRUE(Insert(102, kNotStart, kEnd).packets.empty());
  // Filling the gap completes the frame.
  auto result = Insert(101, kNotStart, kNotEnd);
  EXPECT_EQ(result.packets.size(), 3u);
}

TEST_F(SFramePacketBufferTest, DuplicatePacketDropped) {
  EXPECT_TRUE(Insert(100, kStart, kNotEnd).packets.empty());
  EXPECT_TRUE(Insert(100, kStart, kNotEnd).packets.empty());
  // Original packet still in buffer -- completing the frame proves it.
  auto result = Insert(101, kNotStart, kEnd);
  EXPECT_EQ(result.packets.size(), 2u);
}

TEST_F(SFramePacketBufferTest, TBitMismatchDropsFrame) {
  EXPECT_TRUE(
      Insert(100, kStart, kNotEnd, 1000, 96, SframeEncryptionLevel::kFrame)
          .packets.empty());
  auto result =
      Insert(101, kNotStart, kEnd, 1000, 96, SframeEncryptionLevel::kPacket);
  EXPECT_TRUE(result.packets.empty());
  // Dropped packets don't block subsequent frames.
  auto r2 = Insert(102, kStart, kEnd, /*timestamp=*/2000);
  EXPECT_FALSE(r2.packets.empty());
}

TEST_F(SFramePacketBufferTest, PayloadTypeMismatchDropsFrame) {
  EXPECT_TRUE(
      Insert(100, kStart, kNotEnd, 1000, /*payload_type=*/96).packets.empty());
  auto result = Insert(101, kNotStart, kEnd, 1000, /*payload_type=*/97);
  EXPECT_TRUE(result.packets.empty());
  auto r2 = Insert(102, kStart, kEnd, /*timestamp=*/2000);
  EXPECT_FALSE(r2.packets.empty());
}

TEST_F(SFramePacketBufferTest, TimestampMismatchDropsFrame) {
  EXPECT_TRUE(Insert(100, kStart, kNotEnd, /*timestamp=*/1000).packets.empty());
  auto result = Insert(101, kNotStart, kEnd, /*timestamp=*/2000);
  EXPECT_TRUE(result.packets.empty());
  auto r2 = Insert(102, kStart, kEnd, /*timestamp=*/3000);
  EXPECT_FALSE(r2.packets.empty());
}

TEST_F(SFramePacketBufferTest, PacketizedBitPropagated) {
  auto result =
      Insert(100, kStart, kEnd, 1000, 96, SframeEncryptionLevel::kPacket);
  ASSERT_FALSE(result.packets.empty());
  EXPECT_EQ(result.encryption_level, SframeEncryptionLevel::kPacket);
}

TEST_F(SFramePacketBufferTest, NonPacketizedBitPropagated) {
  auto result =
      Insert(100, kStart, kEnd, 1000, 96, SframeEncryptionLevel::kFrame);
  ASSERT_FALSE(result.packets.empty());
  EXPECT_EQ(result.encryption_level, SframeEncryptionLevel::kFrame);
}

TEST_F(SFramePacketBufferTest, AssembledPacketsRemovedFromBuffer) {
  auto r1 = Insert(100, kStart, kEnd);
  ASSERT_FALSE(r1.packets.empty());
  // Slot is freed after assembly. Re-inserting assembles a new frame.
  auto r2 = Insert(100, kStart, kEnd);
  ASSERT_FALSE(r2.packets.empty());
  EXPECT_EQ(r2.packets.size(), 1u);
}

TEST_F(SFramePacketBufferTest, MultiPacketFrameRemovedFromBuffer) {
  Insert(100, kStart, kNotEnd);
  Insert(101, kNotStart, kNotEnd);
  auto result = Insert(102, kNotStart, kEnd);
  ASSERT_EQ(result.packets.size(), 3u);
  // Slots freed -- a new frame at same seq range can be inserted.
  auto r2 = Insert(100, kStart, kEnd, /*timestamp=*/9000);
  EXPECT_FALSE(r2.packets.empty());
}

TEST_F(SFramePacketBufferTest, ClearRemovesAll) {
  Insert(100, kStart, kNotEnd);
  Insert(101, kNotStart, kNotEnd);

  buffer_.Clear();
  // After clear, inserting a new single-packet frame works (no stale data).
  auto result = Insert(200, kStart, kEnd, /*timestamp=*/5000);
  ASSERT_FALSE(result.packets.empty());
  EXPECT_EQ(result.packets[0]->SequenceNumber(), 200);
}

TEST_F(SFramePacketBufferTest, ClearToRemovesOldPackets) {
  Insert(100, kStart, kNotEnd, /*timestamp=*/1000);
  Insert(101, kNotStart, kNotEnd, /*timestamp=*/1000);
  Insert(102, kNotStart, kNotEnd, /*timestamp=*/1000);

  buffer_.ClearTo(101);
  // Packets 100-101 cleared, packet 102 remains.
  // Completing a frame starting from 102 should work.
  auto result = Insert(103, kNotStart, kEnd, /*timestamp=*/1000);
  // 102 has no S-bit and 100-101 are gone, so this can't assemble.
  EXPECT_TRUE(result.packets.empty());
}

TEST_F(SFramePacketBufferTest, ClearToIsInclusive) {
  Insert(100, kStart, kEnd, /*timestamp=*/1000);
  // Frame was assembled, buffer is empty. Insert new incomplete frame.
  Insert(101, kStart, kNotEnd, /*timestamp=*/2000);
  Insert(102, kNotStart, kNotEnd, /*timestamp=*/2000);

  buffer_.ClearTo(102);
  // Both 101 and 102 should be cleared. A new frame after works.
  auto result = Insert(103, kStart, kEnd, /*timestamp=*/3000);
  ASSERT_FALSE(result.packets.empty());
  EXPECT_EQ(result.packets[0]->SequenceNumber(), 103);
}

TEST_F(SFramePacketBufferTest, ClearToBeforeFirstPacketIsNoop) {
  Insert(100, kStart, kNotEnd);

  // ClearTo a seq_num before the first received packet should be a no-op.
  buffer_.ClearTo(50);
  // Packet 100 is still there -- completing the frame proves it.
  auto result = Insert(101, kNotStart, kEnd);
  ASSERT_EQ(result.packets.size(), 2u);
}

TEST_F(SFramePacketBufferTest, SeqNumWraparound) {
  uint16_t seq = 0xFFFE;
  EXPECT_TRUE(Insert(seq, kStart, kNotEnd).packets.empty());
  EXPECT_TRUE(Insert(seq + 1, kNotStart, kNotEnd).packets.empty());
  // seq + 2 wraps to 0.
  auto result = Insert(static_cast<uint16_t>(seq + 2), kNotStart, kEnd);
  ASSERT_EQ(result.packets.size(), 3u);
  EXPECT_EQ(result.packets[0]->SequenceNumber(), 0xFFFE);
  EXPECT_EQ(result.packets[1]->SequenceNumber(), 0xFFFF);
  EXPECT_EQ(result.packets[2]->SequenceNumber(), 0x0000);
}

TEST_F(SFramePacketBufferTest, SinglePacketAtWraparound) {
  auto result = Insert(0xFFFF, kStart, kEnd);
  ASSERT_FALSE(result.packets.empty());
  EXPECT_EQ(result.packets[0]->SequenceNumber(), 0xFFFF);
}

TEST_F(SFramePacketBufferTest, ReorderedPacketUnwrapsToNegative) {
  // First packet at seq 2 unwraps to 2.  Then a reordered packet at raw seq
  // 65535 unwraps to -1 (backward by 3 is shorter than forward by 65533).
  // ToIdx must handle the negative unwrapped value correctly.
  EXPECT_TRUE(Insert(2, kNotStart, kEnd).packets.empty());
  auto result = Insert(0xFFFF, kStart, kNotEnd);
  EXPECT_TRUE(result.packets.empty());
  // Both packets are buffered — insert the middle to complete the frame.
  auto r2 = Insert(0, kNotStart, kNotEnd);
  EXPECT_TRUE(r2.packets.empty());
  auto r3 = Insert(1, kNotStart, kNotEnd);
  ASSERT_EQ(r3.packets.size(), 4u);
  EXPECT_EQ(r3.packets[0]->SequenceNumber(), 0xFFFF);
  EXPECT_EQ(r3.packets[3]->SequenceNumber(), 2);
}

TEST_F(SFramePacketBufferTest, CollisionClearsBuffer) {
  // seq 0 and seq 16 both map to slot 0 in a size-16 buffer.
  EXPECT_TRUE(Insert(0, kStart, kNotEnd, /*timestamp=*/100).packets.empty());
  // Insert 16 which collides with slot 0. Should clear the buffer.
  auto result = Insert(16, kStart, kNotEnd, /*timestamp=*/200);
  EXPECT_TRUE(BufferCleared(result.status));
}

TEST_F(SFramePacketBufferTest, BufferClearedOnCollision) {
  // Create a small buffer so collisions happen quickly.
  SFramePacketBuffer small_buffer(4);

  // Fill all 4 slots with non-colliding packets.
  small_buffer.InsertPacket(MakePacket(0, kStart, kNotEnd, 100));
  small_buffer.InsertPacket(MakePacket(1, kNotStart, kNotEnd, 100));
  small_buffer.InsertPacket(MakePacket(2, kNotStart, kNotEnd, 100));
  small_buffer.InsertPacket(MakePacket(3, kNotStart, kNotEnd, 100));

  // Insert packet that collides (seq 4 maps to slot 0 in size-4 buffer).
  // Should clear and re-insert.
  auto result = small_buffer.InsertPacket(MakePacket(4, kStart, kEnd, 200));
  // Buffer cleared — caller should request a keyframe.
  ASSERT_TRUE(std::holds_alternative<SFramePacketBuffer::InsertResult>(result));
  EXPECT_EQ(std::get<SFramePacketBuffer::InsertResult>(result),
            SFramePacketBuffer::InsertResult::kBufferCleared);
}

TEST_F(SFramePacketBufferTest, VeryOldPacketDropped) {
  // Insert a packet to establish the window.
  Insert(1000, kStart, kEnd);

  // Try to insert a packet far before the window. With buffer_size=16,
  // any packet >= 16 positions back is dropped.
  auto result = Insert(900, kStart, kEnd);
  // Packet is dropped (too old), no assembly.
  EXPECT_TRUE(result.packets.empty());
}

TEST_F(SFramePacketBufferTest, TimestampPreservedInResult) {
  auto result = Insert(100, kStart, kEnd, /*timestamp=*/42000);
  ASSERT_FALSE(result.packets.empty());
  EXPECT_EQ(result.packets[0]->Timestamp(), 42000u);
}

TEST_F(SFramePacketBufferTest, PayloadTypePreservedInResult) {
  auto result = Insert(100, kStart, kEnd, 1000, /*payload_type=*/111);
  ASSERT_FALSE(result.packets.empty());
  EXPECT_EQ(result.packets[0]->PayloadType(), 111);
}

TEST_F(SFramePacketBufferTest, InterleavedFramesAssembleIndependently) {
  // Frame A: seq 10-11 (ts 1000), Frame B: seq 12-13 (ts 2000).
  // Insert them interleaved: A-start, B-start, A-end, B-end.
  EXPECT_TRUE(Insert(10, kStart, kNotEnd, /*timestamp=*/1000).packets.empty());
  EXPECT_TRUE(Insert(12, kStart, kNotEnd, /*timestamp=*/2000).packets.empty());

  // Completing frame A.
  auto r1 = Insert(11, kNotStart, kEnd, /*timestamp=*/1000);
  ASSERT_EQ(r1.packets.size(), 2u);
  EXPECT_EQ(r1.packets[0]->SequenceNumber(), 10);

  // Completing frame B.
  auto r2 = Insert(13, kNotStart, kEnd, /*timestamp=*/2000);
  ASSERT_EQ(r2.packets.size(), 2u);
  EXPECT_EQ(r2.packets[0]->SequenceNumber(), 12);
}

TEST_F(SFramePacketBufferTest, FrameAfterDroppedFrameStillAssembles) {
  // Frame 1 with timestamp mismatch -> dropped.
  Insert(100, kStart, kNotEnd, /*timestamp=*/1000);
  Insert(101, kNotStart, kEnd, /*timestamp=*/9999);  // mismatch

  // Frame 2 is valid.
  auto result = Insert(102, kStart, kEnd, /*timestamp=*/2000);
  ASSERT_FALSE(result.packets.empty());
  EXPECT_EQ(result.packets[0]->SequenceNumber(), 102);
}

TEST_F(SFramePacketBufferTest, ClearToPreventsLatePacketFromMovingWindow) {
  // Insert and assemble frame at seq 100.
  auto r1 = Insert(100, kStart, kEnd, /*timestamp=*/1000);
  ASSERT_FALSE(r1.packets.empty());

  // Advance the window past seq 105.
  Insert(101, kStart, kNotEnd, /*timestamp=*/2000);
  Insert(102, kNotStart, kNotEnd, /*timestamp=*/2000);
  Insert(103, kNotStart, kNotEnd, /*timestamp=*/2000);
  Insert(104, kNotStart, kNotEnd, /*timestamp=*/2000);
  Insert(105, kNotStart, kNotEnd, /*timestamp=*/2000);
  buffer_.ClearTo(105);

  // A late packet from before the cleared point should be dropped.
  auto late = Insert(103, kStart, kEnd, /*timestamp=*/2000);
  EXPECT_TRUE(late.packets.empty());
}

TEST_F(SFramePacketBufferTest, ClearToAllowsNewPacketsAfterClearedPoint) {
  Insert(100, kStart, kEnd, /*timestamp=*/1000);
  buffer_.ClearTo(100);

  // Packet after the cleared point should work fine.
  auto result = Insert(110, kStart, kEnd, /*timestamp=*/2000);
  ASSERT_FALSE(result.packets.empty());
  EXPECT_EQ(result.packets[0]->SequenceNumber(), 110);
}

TEST_F(SFramePacketBufferTest, ClearToDoesNotWipeNewerPacket) {
  // Insert two frames in non-adjacent slots.
  auto r1 = Insert(0, kStart, kEnd, /*timestamp=*/1000);
  ASSERT_FALSE(r1.packets.empty());

  // Packet in a higher slot (2 * kBufferSize away to avoid aliasing).
  Insert(2 * kBufferSize, kStart, kNotEnd, /*timestamp=*/3000);

  // ClearTo the first frame — must not affect the second.
  buffer_.ClearTo(0);

  // Completing the second frame still works.
  auto r2 = Insert(2 * kBufferSize + 1, kNotStart, kEnd, /*timestamp=*/3000);
  ASSERT_EQ(r2.packets.size(), 2u);
  EXPECT_EQ(r2.packets[0]->SequenceNumber(), 2 * kBufferSize);
}

TEST_F(SFramePacketBufferTest, FreedSlotsReuseFullBuffer) {
  // Use a small buffer where we can fill it entirely.
  SFramePacketBuffer small_buffer(4);

  // Frame 1: 3 packets, assembles and frees slots.
  small_buffer.InsertPacket(MakePacket(10, kStart, kNotEnd, 100));
  small_buffer.InsertPacket(MakePacket(11, kNotStart, kNotEnd, 100));
  auto r1 = InsertInto(small_buffer, MakePacket(12, kNotStart, kEnd, 100));
  ASSERT_EQ(r1.packets.size(), 3u);

  // Frame 2: fills all 4 slots using the freed space.
  small_buffer.InsertPacket(MakePacket(13, kStart, kNotEnd, 200));
  small_buffer.InsertPacket(MakePacket(14, kNotStart, kNotEnd, 200));
  small_buffer.InsertPacket(MakePacket(15, kNotStart, kNotEnd, 200));
  auto r2 = InsertInto(small_buffer, MakePacket(16, kNotStart, kEnd, 200));
  ASSERT_EQ(r2.packets.size(), 4u);
  EXPECT_FALSE(BufferCleared(r2.status));
}

TEST_F(SFramePacketBufferTest, FramesAfterClearToWithReordering) {
  // Assemble and clear past a frame.
  Insert(100, kStart, kEnd, /*timestamp=*/1000);
  Insert(101, kStart, kEnd, /*timestamp=*/2000);
  buffer_.ClearTo(101);

  // Insert a later frame first (out of order).
  auto r1 = Insert(110, kStart, kEnd, /*timestamp=*/4000);
  ASSERT_FALSE(r1.packets.empty());
  EXPECT_EQ(r1.packets[0]->SequenceNumber(), 110);

  // Insert a frame between the cleared point and the later frame.
  auto r2 = Insert(105, kStart, kEnd, /*timestamp=*/3000);
  ASSERT_FALSE(r2.packets.empty());
  EXPECT_EQ(r2.packets[0]->SequenceNumber(), 105);
}

TEST_F(SFramePacketBufferTest, TwoStartsWithoutEnd) {
  // S(101) followed by S(102) — two starts, no complete frame.
  EXPECT_TRUE(Insert(101, kStart, kNotEnd).packets.empty());
  EXPECT_TRUE(Insert(102, kStart, kNotEnd, /*timestamp=*/2000).packets.empty());
  // Adding end for second "frame" (102-103) should assemble.
  auto result = Insert(103, kNotStart, kEnd, /*timestamp=*/2000);
  ASSERT_EQ(result.packets.size(), 2u);
  EXPECT_EQ(result.packets[0]->SequenceNumber(), 102);
  EXPECT_EQ(result.packets[1]->SequenceNumber(), 103);
}

TEST_F(SFramePacketBufferTest, TwoEndsWithoutStart) {
  // Two E-bit packets with no preceding S-bit — no frame assembles.
  EXPECT_TRUE(Insert(100, kNotStart, kEnd).packets.empty());
  EXPECT_TRUE(Insert(101, kNotStart, kEnd, /*timestamp=*/2000).packets.empty());
}

TEST_F(SFramePacketBufferTest, EFollowedByE) {
  // S at 100, E at 101, then another E at 102 with different timestamp.
  auto r1 = Insert(100, kStart, kNotEnd, /*timestamp=*/1000);
  EXPECT_TRUE(r1.packets.empty());
  auto r2 = Insert(101, kNotStart, kEnd, /*timestamp=*/1000);
  ASSERT_EQ(r2.packets.size(), 2u);
  // Orphan E at 102 — no start found.
  auto r3 = Insert(102, kNotStart, kEnd, /*timestamp=*/2000);
  EXPECT_TRUE(r3.packets.empty());
}

TEST_F(SFramePacketBufferTest, TBitMismatchInMiddlePacketDropsFrame) {
  // 3-packet frame where the middle packet has a mismatched T-bit.
  EXPECT_TRUE(
      Insert(100, kStart, kNotEnd, 1000, 96, SframeEncryptionLevel::kFrame)
          .packets.empty());
  EXPECT_TRUE(
      Insert(101, kNotStart, kNotEnd, 1000, 96, SframeEncryptionLevel::kPacket)
          .packets.empty());
  auto result =
      Insert(102, kNotStart, kEnd, 1000, 96, SframeEncryptionLevel::kFrame);
  EXPECT_TRUE(result.packets.empty());
  // All slots must be freed — re-inserting a fresh frame at same seqs works.
  auto r2 = Insert(100, kStart, kEnd, /*timestamp=*/5000);
  ASSERT_EQ(r2.packets.size(), 1u);
}

TEST_F(SFramePacketBufferTest, FrameWrapsAroundRingBuffer) {
  // With buffer_size=16, place S near the end and E past the wrap.
  // Seq 14 → slot 14, seq 15 → slot 15, seq 16 → slot 0.
  EXPECT_TRUE(Insert(14, kStart, kNotEnd).packets.empty());
  EXPECT_TRUE(Insert(15, kNotStart, kNotEnd).packets.empty());
  auto result = Insert(16, kNotStart, kEnd);
  ASSERT_EQ(result.packets.size(), 3u);
  EXPECT_EQ(result.packets[0]->SequenceNumber(), 14);
  EXPECT_EQ(result.packets[2]->SequenceNumber(), 16);
}

TEST_F(SFramePacketBufferTest, CollisionDuringMultiPacketFrameClearsBuffer) {
  // Start a multi-packet frame.
  EXPECT_TRUE(Insert(0, kStart, kNotEnd, /*timestamp=*/100).packets.empty());
  EXPECT_TRUE(Insert(1, kNotStart, kNotEnd, /*timestamp=*/100).packets.empty());
  // Insert a packet that collides (seq 16 → slot 0). Buffer clears.
  auto result = Insert(16, kStart, kNotEnd, /*timestamp=*/200);
  EXPECT_TRUE(BufferCleared(result.status));
}

TEST_F(SFramePacketBufferTest, BufferFullBeforeEndPacket) {
  // Small buffer: S and middle packets fill it, E arrives too late.
  SFramePacketBuffer small_buffer(4);
  small_buffer.InsertPacket(MakePacket(0, kStart, kNotEnd, 100));
  small_buffer.InsertPacket(MakePacket(1, kNotStart, kNotEnd, 100));
  small_buffer.InsertPacket(MakePacket(2, kNotStart, kNotEnd, 100));
  small_buffer.InsertPacket(MakePacket(3, kNotStart, kNotEnd, 100));
  // Packet 4 (the end) collides with slot 0; buffer clears.
  auto result = InsertInto(small_buffer, MakePacket(4, kNotStart, kEnd, 100));
  // Buffer cleared — the frame is lost.
  EXPECT_TRUE(BufferCleared(result.status));
  // Only packet 4 remains; it has no S-bit so no frame assembles.
  EXPECT_TRUE(result.packets.empty());
}

}  // namespace
}  // namespace webrtc
