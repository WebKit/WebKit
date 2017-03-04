/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Unit tests for PacketBuffer class.

#include "webrtc/api/audio_codecs/builtin_audio_decoder_factory.h"
#include "webrtc/modules/audio_coding/neteq/mock/mock_decoder_database.h"
#include "webrtc/modules/audio_coding/neteq/packet.h"
#include "webrtc/modules/audio_coding/neteq/packet_buffer.h"
#include "webrtc/modules/audio_coding/neteq/tick_timer.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"

using ::testing::Return;
using ::testing::_;

namespace webrtc {

// Helper class to generate packets. Packets must be deleted by the user.
class PacketGenerator {
 public:
  PacketGenerator(uint16_t seq_no, uint32_t ts, uint8_t pt, int frame_size);
  virtual ~PacketGenerator() {}
  void Reset(uint16_t seq_no, uint32_t ts, uint8_t pt, int frame_size);
  Packet NextPacket(int payload_size_bytes);

  uint16_t seq_no_;
  uint32_t ts_;
  uint8_t pt_;
  int frame_size_;
};

PacketGenerator::PacketGenerator(uint16_t seq_no, uint32_t ts, uint8_t pt,
                                 int frame_size) {
  Reset(seq_no, ts, pt, frame_size);
}

void PacketGenerator::Reset(uint16_t seq_no, uint32_t ts, uint8_t pt,
                            int frame_size) {
  seq_no_ = seq_no;
  ts_ = ts;
  pt_ = pt;
  frame_size_ = frame_size;
}

Packet PacketGenerator::NextPacket(int payload_size_bytes) {
  Packet packet;
  packet.sequence_number = seq_no_;
  packet.timestamp = ts_;
  packet.payload_type = pt_;
  packet.payload.SetSize(payload_size_bytes);
  ++seq_no_;
  ts_ += frame_size_;
  return packet;
}

struct PacketsToInsert {
  uint16_t sequence_number;
  uint32_t timestamp;
  uint8_t payload_type;
  bool primary;
  // Order of this packet to appear upon extraction, after inserting a series
  // of packets. A negative number means that it should have been discarded
  // before extraction.
  int extract_order;
};

// Start of test definitions.

TEST(PacketBuffer, CreateAndDestroy) {
  TickTimer tick_timer;
  PacketBuffer* buffer = new PacketBuffer(10, &tick_timer);  // 10 packets.
  EXPECT_TRUE(buffer->Empty());
  delete buffer;
}

TEST(PacketBuffer, InsertPacket) {
  TickTimer tick_timer;
  PacketBuffer buffer(10, &tick_timer);  // 10 packets.
  PacketGenerator gen(17u, 4711u, 0, 10);

  const int payload_len = 100;
  const Packet packet = gen.NextPacket(payload_len);
  EXPECT_EQ(0, buffer.InsertPacket(packet.Clone()));
  uint32_t next_ts;
  EXPECT_EQ(PacketBuffer::kOK, buffer.NextTimestamp(&next_ts));
  EXPECT_EQ(4711u, next_ts);
  EXPECT_FALSE(buffer.Empty());
  EXPECT_EQ(1u, buffer.NumPacketsInBuffer());
  const Packet* next_packet = buffer.PeekNextPacket();
  EXPECT_EQ(packet, *next_packet);  // Compare contents.

  // Do not explicitly flush buffer or delete packet to test that it is deleted
  // with the buffer. (Tested with Valgrind or similar tool.)
}

// Test to flush buffer.
TEST(PacketBuffer, FlushBuffer) {
  TickTimer tick_timer;
  PacketBuffer buffer(10, &tick_timer);  // 10 packets.
  PacketGenerator gen(0, 0, 0, 10);
  const int payload_len = 10;

  // Insert 10 small packets; should be ok.
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(PacketBuffer::kOK,
              buffer.InsertPacket(gen.NextPacket(payload_len)));
  }
  EXPECT_EQ(10u, buffer.NumPacketsInBuffer());
  EXPECT_FALSE(buffer.Empty());

  buffer.Flush();
  // Buffer should delete the payloads itself.
  EXPECT_EQ(0u, buffer.NumPacketsInBuffer());
  EXPECT_TRUE(buffer.Empty());
}

// Test to fill the buffer over the limits, and verify that it flushes.
TEST(PacketBuffer, OverfillBuffer) {
  TickTimer tick_timer;
  PacketBuffer buffer(10, &tick_timer);  // 10 packets.
  PacketGenerator gen(0, 0, 0, 10);

  // Insert 10 small packets; should be ok.
  const int payload_len = 10;
  int i;
  for (i = 0; i < 10; ++i) {
    EXPECT_EQ(PacketBuffer::kOK,
              buffer.InsertPacket(gen.NextPacket(payload_len)));
  }
  EXPECT_EQ(10u, buffer.NumPacketsInBuffer());
  uint32_t next_ts;
  EXPECT_EQ(PacketBuffer::kOK, buffer.NextTimestamp(&next_ts));
  EXPECT_EQ(0u, next_ts);  // Expect first inserted packet to be first in line.

  const Packet packet = gen.NextPacket(payload_len);
  // Insert 11th packet; should flush the buffer and insert it after flushing.
  EXPECT_EQ(PacketBuffer::kFlushed, buffer.InsertPacket(packet.Clone()));
  EXPECT_EQ(1u, buffer.NumPacketsInBuffer());
  EXPECT_EQ(PacketBuffer::kOK, buffer.NextTimestamp(&next_ts));
  // Expect last inserted packet to be first in line.
  EXPECT_EQ(packet.timestamp, next_ts);

  // Flush buffer to delete all packets.
  buffer.Flush();
}

// Test inserting a list of packets.
TEST(PacketBuffer, InsertPacketList) {
  TickTimer tick_timer;
  PacketBuffer buffer(10, &tick_timer);  // 10 packets.
  PacketGenerator gen(0, 0, 0, 10);
  PacketList list;
  const int payload_len = 10;

  // Insert 10 small packets.
  for (int i = 0; i < 10; ++i) {
    list.push_back(gen.NextPacket(payload_len));
  }

  MockDecoderDatabase decoder_database;
  auto factory = CreateBuiltinAudioDecoderFactory();
  const DecoderDatabase::DecoderInfo info(NetEqDecoder::kDecoderPCMu, factory);
  EXPECT_CALL(decoder_database, GetDecoderInfo(0))
      .WillRepeatedly(Return(&info));
  rtc::Optional<uint8_t> current_pt;
  rtc::Optional<uint8_t> current_cng_pt;
  EXPECT_EQ(PacketBuffer::kOK, buffer.InsertPacketList(&list,
                                                       decoder_database,
                                                       &current_pt,
                                                       &current_cng_pt));
  EXPECT_TRUE(list.empty());  // The PacketBuffer should have depleted the list.
  EXPECT_EQ(10u, buffer.NumPacketsInBuffer());
  EXPECT_EQ(rtc::Optional<uint8_t>(0),
            current_pt);         // Current payload type changed to 0.
  EXPECT_FALSE(current_cng_pt);  // CNG payload type not changed.

  buffer.Flush();  // Clean up.

  EXPECT_CALL(decoder_database, Die());  // Called when object is deleted.
}

// Test inserting a list of packets. Last packet is of a different payload type.
// Expecting the buffer to flush.
// TODO(hlundin): Remove this test when legacy operation is no longer needed.
TEST(PacketBuffer, InsertPacketListChangePayloadType) {
  TickTimer tick_timer;
  PacketBuffer buffer(10, &tick_timer);  // 10 packets.
  PacketGenerator gen(0, 0, 0, 10);
  PacketList list;
  const int payload_len = 10;

  // Insert 10 small packets.
  for (int i = 0; i < 10; ++i) {
    list.push_back(gen.NextPacket(payload_len));
  }
  // Insert 11th packet of another payload type (not CNG).
  {
    Packet packet = gen.NextPacket(payload_len);
    packet.payload_type = 1;
    list.push_back(std::move(packet));
  }

  MockDecoderDatabase decoder_database;
  auto factory = CreateBuiltinAudioDecoderFactory();
  const DecoderDatabase::DecoderInfo info0(NetEqDecoder::kDecoderPCMu, factory);
  EXPECT_CALL(decoder_database, GetDecoderInfo(0))
      .WillRepeatedly(Return(&info0));
  const DecoderDatabase::DecoderInfo info1(NetEqDecoder::kDecoderPCMa, factory);
  EXPECT_CALL(decoder_database, GetDecoderInfo(1))
      .WillRepeatedly(Return(&info1));
  rtc::Optional<uint8_t> current_pt;
  rtc::Optional<uint8_t> current_cng_pt;
  EXPECT_EQ(PacketBuffer::kFlushed, buffer.InsertPacketList(&list,
                                                            decoder_database,
                                                            &current_pt,
                                                            &current_cng_pt));
  EXPECT_TRUE(list.empty());  // The PacketBuffer should have depleted the list.
  EXPECT_EQ(1u, buffer.NumPacketsInBuffer());  // Only the last packet.
  EXPECT_EQ(rtc::Optional<uint8_t>(1),
            current_pt);         // Current payload type changed to 1.
  EXPECT_FALSE(current_cng_pt);  // CNG payload type not changed.

  buffer.Flush();  // Clean up.

  EXPECT_CALL(decoder_database, Die());  // Called when object is deleted.
}

TEST(PacketBuffer, ExtractOrderRedundancy) {
  TickTimer tick_timer;
  PacketBuffer buffer(100, &tick_timer);  // 100 packets.
  const int kPackets = 18;
  const int kFrameSize = 10;
  const int kPayloadLength = 10;

  PacketsToInsert packet_facts[kPackets] = {
    {0xFFFD, 0xFFFFFFD7, 0, true, 0},
    {0xFFFE, 0xFFFFFFE1, 0, true, 1},
    {0xFFFE, 0xFFFFFFD7, 1, false, -1},
    {0xFFFF, 0xFFFFFFEB, 0, true, 2},
    {0xFFFF, 0xFFFFFFE1, 1, false, -1},
    {0x0000, 0xFFFFFFF5, 0, true, 3},
    {0x0000, 0xFFFFFFEB, 1, false, -1},
    {0x0001, 0xFFFFFFFF, 0, true, 4},
    {0x0001, 0xFFFFFFF5, 1, false, -1},
    {0x0002, 0x0000000A, 0, true, 5},
    {0x0002, 0xFFFFFFFF, 1, false, -1},
    {0x0003, 0x0000000A, 1, false, -1},
    {0x0004, 0x0000001E, 0, true, 7},
    {0x0004, 0x00000014, 1, false, 6},
    {0x0005, 0x0000001E, 0, true, -1},
    {0x0005, 0x00000014, 1, false, -1},
    {0x0006, 0x00000028, 0, true, 8},
    {0x0006, 0x0000001E, 1, false, -1},
  };

  const size_t kExpectPacketsInBuffer = 9;

  std::vector<Packet> expect_order(kExpectPacketsInBuffer);

  PacketGenerator gen(0, 0, 0, kFrameSize);

  for (int i = 0; i < kPackets; ++i) {
    gen.Reset(packet_facts[i].sequence_number,
              packet_facts[i].timestamp,
              packet_facts[i].payload_type,
              kFrameSize);
    Packet packet = gen.NextPacket(kPayloadLength);
    packet.priority.red_level = packet_facts[i].primary ? 0 : 1;
    EXPECT_EQ(PacketBuffer::kOK, buffer.InsertPacket(packet.Clone()));
    if (packet_facts[i].extract_order >= 0) {
      expect_order[packet_facts[i].extract_order] = std::move(packet);
    }
  }

  EXPECT_EQ(kExpectPacketsInBuffer, buffer.NumPacketsInBuffer());

  for (size_t i = 0; i < kExpectPacketsInBuffer; ++i) {
    const rtc::Optional<Packet> packet = buffer.GetNextPacket();
    EXPECT_EQ(packet, expect_order[i]);  // Compare contents.
  }
  EXPECT_TRUE(buffer.Empty());
}

TEST(PacketBuffer, DiscardPackets) {
  TickTimer tick_timer;
  PacketBuffer buffer(100, &tick_timer);  // 100 packets.
  const uint16_t start_seq_no = 17;
  const uint32_t start_ts = 4711;
  const uint32_t ts_increment = 10;
  PacketGenerator gen(start_seq_no, start_ts, 0, ts_increment);
  PacketList list;
  const int payload_len = 10;

  // Insert 10 small packets.
  for (int i = 0; i < 10; ++i) {
    buffer.InsertPacket(gen.NextPacket(payload_len));
  }
  EXPECT_EQ(10u, buffer.NumPacketsInBuffer());

  // Discard them one by one and make sure that the right packets are at the
  // front of the buffer.
  uint32_t current_ts = start_ts;
  for (int i = 0; i < 10; ++i) {
    uint32_t ts;
    EXPECT_EQ(PacketBuffer::kOK, buffer.NextTimestamp(&ts));
    EXPECT_EQ(current_ts, ts);
    EXPECT_EQ(PacketBuffer::kOK, buffer.DiscardNextPacket());
    current_ts += ts_increment;
  }
  EXPECT_TRUE(buffer.Empty());
}

TEST(PacketBuffer, Reordering) {
  TickTimer tick_timer;
  PacketBuffer buffer(100, &tick_timer);  // 100 packets.
  const uint16_t start_seq_no = 17;
  const uint32_t start_ts = 4711;
  const uint32_t ts_increment = 10;
  PacketGenerator gen(start_seq_no, start_ts, 0, ts_increment);
  const int payload_len = 10;

  // Generate 10 small packets and insert them into a PacketList. Insert every
  // odd packet to the front, and every even packet to the back, thus creating
  // a (rather strange) reordering.
  PacketList list;
  for (int i = 0; i < 10; ++i) {
    Packet packet = gen.NextPacket(payload_len);
    if (i % 2) {
      list.push_front(std::move(packet));
    } else {
      list.push_back(std::move(packet));
    }
  }

  MockDecoderDatabase decoder_database;
  auto factory = CreateBuiltinAudioDecoderFactory();
  const DecoderDatabase::DecoderInfo info(NetEqDecoder::kDecoderPCMu, factory);
  EXPECT_CALL(decoder_database, GetDecoderInfo(0))
      .WillRepeatedly(Return(&info));
  rtc::Optional<uint8_t> current_pt;
  rtc::Optional<uint8_t> current_cng_pt;

  EXPECT_EQ(PacketBuffer::kOK, buffer.InsertPacketList(&list,
                                                       decoder_database,
                                                       &current_pt,
                                                       &current_cng_pt));
  EXPECT_EQ(10u, buffer.NumPacketsInBuffer());

  // Extract them and make sure that come out in the right order.
  uint32_t current_ts = start_ts;
  for (int i = 0; i < 10; ++i) {
    const rtc::Optional<Packet> packet = buffer.GetNextPacket();
    ASSERT_TRUE(packet);
    EXPECT_EQ(current_ts, packet->timestamp);
    current_ts += ts_increment;
  }
  EXPECT_TRUE(buffer.Empty());

  EXPECT_CALL(decoder_database, Die());  // Called when object is deleted.
}

// The test first inserts a packet with narrow-band CNG, then a packet with
// wide-band speech. The expected behavior of the packet buffer is to detect a
// change in sample rate, even though no speech packet has been inserted before,
// and flush out the CNG packet.
TEST(PacketBuffer, CngFirstThenSpeechWithNewSampleRate) {
  TickTimer tick_timer;
  PacketBuffer buffer(10, &tick_timer);  // 10 packets.
  const uint8_t kCngPt = 13;
  const int kPayloadLen = 10;
  const uint8_t kSpeechPt = 100;

  MockDecoderDatabase decoder_database;
  auto factory = CreateBuiltinAudioDecoderFactory();
  const DecoderDatabase::DecoderInfo info_cng(NetEqDecoder::kDecoderCNGnb,
                                              factory);
  EXPECT_CALL(decoder_database, GetDecoderInfo(kCngPt))
      .WillRepeatedly(Return(&info_cng));
  const DecoderDatabase::DecoderInfo info_speech(NetEqDecoder::kDecoderPCM16Bwb,
                                                 factory);
  EXPECT_CALL(decoder_database, GetDecoderInfo(kSpeechPt))
      .WillRepeatedly(Return(&info_speech));

  // Insert first packet, which is narrow-band CNG.
  PacketGenerator gen(0, 0, kCngPt, 10);
  PacketList list;
  list.push_back(gen.NextPacket(kPayloadLen));
  rtc::Optional<uint8_t> current_pt;
  rtc::Optional<uint8_t> current_cng_pt;
  EXPECT_EQ(PacketBuffer::kOK,
            buffer.InsertPacketList(&list, decoder_database, &current_pt,
                                    &current_cng_pt));
  EXPECT_TRUE(list.empty());
  EXPECT_EQ(1u, buffer.NumPacketsInBuffer());
  ASSERT_TRUE(buffer.PeekNextPacket());
  EXPECT_EQ(kCngPt, buffer.PeekNextPacket()->payload_type);
  EXPECT_FALSE(current_pt);  // Current payload type not set.
  EXPECT_EQ(rtc::Optional<uint8_t>(kCngPt),
            current_cng_pt);  // CNG payload type set.

  // Insert second packet, which is wide-band speech.
  {
    Packet packet = gen.NextPacket(kPayloadLen);
    packet.payload_type = kSpeechPt;
    list.push_back(std::move(packet));
  }
  // Expect the buffer to flush out the CNG packet, since it does not match the
  // new speech sample rate.
  EXPECT_EQ(PacketBuffer::kFlushed,
            buffer.InsertPacketList(&list, decoder_database, &current_pt,
                                    &current_cng_pt));
  EXPECT_TRUE(list.empty());
  EXPECT_EQ(1u, buffer.NumPacketsInBuffer());
  ASSERT_TRUE(buffer.PeekNextPacket());
  EXPECT_EQ(kSpeechPt, buffer.PeekNextPacket()->payload_type);

  EXPECT_EQ(rtc::Optional<uint8_t>(kSpeechPt),
            current_pt);         // Current payload type set.
  EXPECT_FALSE(current_cng_pt);  // CNG payload type reset.

  buffer.Flush();                        // Clean up.
  EXPECT_CALL(decoder_database, Die());  // Called when object is deleted.
}

TEST(PacketBuffer, Failures) {
  const uint16_t start_seq_no = 17;
  const uint32_t start_ts = 4711;
  const uint32_t ts_increment = 10;
  int payload_len = 100;
  PacketGenerator gen(start_seq_no, start_ts, 0, ts_increment);
  TickTimer tick_timer;

  PacketBuffer* buffer = new PacketBuffer(100, &tick_timer);  // 100 packets.
  {
    Packet packet = gen.NextPacket(payload_len);
    packet.payload.Clear();
    EXPECT_EQ(PacketBuffer::kInvalidPacket,
              buffer->InsertPacket(std::move(packet)));
  }
  // Buffer should still be empty. Test all empty-checks.
  uint32_t temp_ts;
  EXPECT_EQ(PacketBuffer::kBufferEmpty, buffer->NextTimestamp(&temp_ts));
  EXPECT_EQ(PacketBuffer::kBufferEmpty,
            buffer->NextHigherTimestamp(0, &temp_ts));
  EXPECT_EQ(NULL, buffer->PeekNextPacket());
  EXPECT_FALSE(buffer->GetNextPacket());
  EXPECT_EQ(PacketBuffer::kBufferEmpty, buffer->DiscardNextPacket());
  EXPECT_EQ(0, buffer->DiscardAllOldPackets(0));  // 0 packets discarded.

  // Insert one packet to make the buffer non-empty.
  EXPECT_EQ(PacketBuffer::kOK,
            buffer->InsertPacket(gen.NextPacket(payload_len)));
  EXPECT_EQ(PacketBuffer::kInvalidPointer, buffer->NextTimestamp(NULL));
  EXPECT_EQ(PacketBuffer::kInvalidPointer,
            buffer->NextHigherTimestamp(0, NULL));
  delete buffer;

  // Insert packet list of three packets, where the second packet has an invalid
  // payload.  Expect first packet to be inserted, and the remaining two to be
  // discarded.
  buffer = new PacketBuffer(100, &tick_timer);  // 100 packets.
  PacketList list;
  list.push_back(gen.NextPacket(payload_len));  // Valid packet.
  {
    Packet packet = gen.NextPacket(payload_len);
    packet.payload.Clear();  // Invalid.
    list.push_back(std::move(packet));
  }
  list.push_back(gen.NextPacket(payload_len));  // Valid packet.
  MockDecoderDatabase decoder_database;
  auto factory = CreateBuiltinAudioDecoderFactory();
  const DecoderDatabase::DecoderInfo info(NetEqDecoder::kDecoderPCMu, factory);
  EXPECT_CALL(decoder_database, GetDecoderInfo(0))
      .WillRepeatedly(Return(&info));
  rtc::Optional<uint8_t> current_pt;
  rtc::Optional<uint8_t> current_cng_pt;
  EXPECT_EQ(PacketBuffer::kInvalidPacket,
            buffer->InsertPacketList(&list,
                                     decoder_database,
                                     &current_pt,
                                     &current_cng_pt));
  EXPECT_TRUE(list.empty());  // The PacketBuffer should have depleted the list.
  EXPECT_EQ(1u, buffer->NumPacketsInBuffer());
  delete buffer;
  EXPECT_CALL(decoder_database, Die());  // Called when object is deleted.
}

// Test packet comparison function.
// The function should return true if the first packet "goes before" the second.
TEST(PacketBuffer, ComparePackets) {
  PacketGenerator gen(0, 0, 0, 10);
  Packet a(gen.NextPacket(10));  // SN = 0, TS = 0.
  Packet b(gen.NextPacket(10));  // SN = 1, TS = 10.
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a != b);
  EXPECT_TRUE(a < b);
  EXPECT_FALSE(a > b);
  EXPECT_TRUE(a <= b);
  EXPECT_FALSE(a >= b);

  // Testing wrap-around case; 'a' is earlier but has a larger timestamp value.
  a.timestamp = 0xFFFFFFFF - 10;
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a != b);
  EXPECT_TRUE(a < b);
  EXPECT_FALSE(a > b);
  EXPECT_TRUE(a <= b);
  EXPECT_FALSE(a >= b);

  // Test equal packets.
  EXPECT_TRUE(a == a);
  EXPECT_FALSE(a != a);
  EXPECT_FALSE(a < a);
  EXPECT_FALSE(a > a);
  EXPECT_TRUE(a <= a);
  EXPECT_TRUE(a >= a);

  // Test equal timestamps but different sequence numbers (0 and 1).
  a.timestamp = b.timestamp;
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a != b);
  EXPECT_TRUE(a < b);
  EXPECT_FALSE(a > b);
  EXPECT_TRUE(a <= b);
  EXPECT_FALSE(a >= b);

  // Test equal timestamps but different sequence numbers (32767 and 1).
  a.sequence_number = 0xFFFF;
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a != b);
  EXPECT_TRUE(a < b);
  EXPECT_FALSE(a > b);
  EXPECT_TRUE(a <= b);
  EXPECT_FALSE(a >= b);

  // Test equal timestamps and sequence numbers, but differing priorities.
  a.sequence_number = b.sequence_number;
  a.priority = {1, 0};
  b.priority = {0, 0};
  // a after b
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a != b);
  EXPECT_FALSE(a < b);
  EXPECT_TRUE(a > b);
  EXPECT_FALSE(a <= b);
  EXPECT_TRUE(a >= b);

  Packet c(gen.NextPacket(0));  // SN = 2, TS = 20.
  Packet d(gen.NextPacket(0));  // SN = 3, TS = 20.
  c.timestamp = b.timestamp;
  d.timestamp = b.timestamp;
  c.sequence_number = b.sequence_number;
  d.sequence_number = b.sequence_number;
  c.priority = {1, 1};
  d.priority = {0, 1};
  // c after d
  EXPECT_FALSE(c == d);
  EXPECT_TRUE(c != d);
  EXPECT_FALSE(c < d);
  EXPECT_TRUE(c > d);
  EXPECT_FALSE(c <= d);
  EXPECT_TRUE(c >= d);

  // c after a
  EXPECT_FALSE(c == a);
  EXPECT_TRUE(c != a);
  EXPECT_FALSE(c < a);
  EXPECT_TRUE(c > a);
  EXPECT_FALSE(c <= a);
  EXPECT_TRUE(c >= a);

  // c after b
  EXPECT_FALSE(c == b);
  EXPECT_TRUE(c != b);
  EXPECT_FALSE(c < b);
  EXPECT_TRUE(c > b);
  EXPECT_FALSE(c <= b);
  EXPECT_TRUE(c >= b);

  // a after d
  EXPECT_FALSE(a == d);
  EXPECT_TRUE(a != d);
  EXPECT_FALSE(a < d);
  EXPECT_TRUE(a > d);
  EXPECT_FALSE(a <= d);
  EXPECT_TRUE(a >= d);

  // d after b
  EXPECT_FALSE(d == b);
  EXPECT_TRUE(d != b);
  EXPECT_FALSE(d < b);
  EXPECT_TRUE(d > b);
  EXPECT_FALSE(d <= b);
  EXPECT_TRUE(d >= b);
}

namespace {
void TestIsObsoleteTimestamp(uint32_t limit_timestamp) {
  // Check with zero horizon, which implies that the horizon is at 2^31, i.e.,
  // half the timestamp range.
  static const uint32_t kZeroHorizon = 0;
  static const uint32_t k2Pow31Minus1 = 0x7FFFFFFF;
  // Timestamp on the limit is not old.
  EXPECT_FALSE(PacketBuffer::IsObsoleteTimestamp(
      limit_timestamp, limit_timestamp, kZeroHorizon));
  // 1 sample behind is old.
  EXPECT_TRUE(PacketBuffer::IsObsoleteTimestamp(
      limit_timestamp - 1, limit_timestamp, kZeroHorizon));
  // 2^31 - 1 samples behind is old.
  EXPECT_TRUE(PacketBuffer::IsObsoleteTimestamp(
      limit_timestamp - k2Pow31Minus1, limit_timestamp, kZeroHorizon));
  // 1 sample ahead is not old.
  EXPECT_FALSE(PacketBuffer::IsObsoleteTimestamp(
      limit_timestamp + 1, limit_timestamp, kZeroHorizon));
  // If |t1-t2|=2^31 and t1>t2, t2 is older than t1 but not the opposite.
  uint32_t other_timestamp = limit_timestamp + (1 << 31);
  uint32_t lowest_timestamp = std::min(limit_timestamp, other_timestamp);
  uint32_t highest_timestamp = std::max(limit_timestamp, other_timestamp);
  EXPECT_TRUE(PacketBuffer::IsObsoleteTimestamp(
      lowest_timestamp, highest_timestamp, kZeroHorizon));
  EXPECT_FALSE(PacketBuffer::IsObsoleteTimestamp(
      highest_timestamp, lowest_timestamp, kZeroHorizon));

  // Fixed horizon at 10 samples.
  static const uint32_t kHorizon = 10;
  // Timestamp on the limit is not old.
  EXPECT_FALSE(PacketBuffer::IsObsoleteTimestamp(
      limit_timestamp, limit_timestamp, kHorizon));
  // 1 sample behind is old.
  EXPECT_TRUE(PacketBuffer::IsObsoleteTimestamp(
      limit_timestamp - 1, limit_timestamp, kHorizon));
  // 9 samples behind is old.
  EXPECT_TRUE(PacketBuffer::IsObsoleteTimestamp(
      limit_timestamp - 9, limit_timestamp, kHorizon));
  // 10 samples behind is not old.
  EXPECT_FALSE(PacketBuffer::IsObsoleteTimestamp(
      limit_timestamp - 10, limit_timestamp, kHorizon));
  // 2^31 - 1 samples behind is not old.
  EXPECT_FALSE(PacketBuffer::IsObsoleteTimestamp(
      limit_timestamp - k2Pow31Minus1, limit_timestamp, kHorizon));
  // 1 sample ahead is not old.
  EXPECT_FALSE(PacketBuffer::IsObsoleteTimestamp(
      limit_timestamp + 1, limit_timestamp, kHorizon));
  // 2^31 samples ahead is not old.
  EXPECT_FALSE(PacketBuffer::IsObsoleteTimestamp(
      limit_timestamp + (1 << 31), limit_timestamp, kHorizon));
}
}  // namespace

// Test the IsObsoleteTimestamp method with different limit timestamps.
TEST(PacketBuffer, IsObsoleteTimestamp) {
  TestIsObsoleteTimestamp(0);
  TestIsObsoleteTimestamp(1);
  TestIsObsoleteTimestamp(0xFFFFFFFF);  // -1 in uint32_t.
  TestIsObsoleteTimestamp(0x80000000);  // 2^31.
  TestIsObsoleteTimestamp(0x80000001);  // 2^31 + 1.
  TestIsObsoleteTimestamp(0x7FFFFFFF);  // 2^31 - 1.
}
}  // namespace webrtc
