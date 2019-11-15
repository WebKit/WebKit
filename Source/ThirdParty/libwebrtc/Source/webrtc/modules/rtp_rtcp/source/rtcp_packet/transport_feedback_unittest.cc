/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"

#include <limits>
#include <memory>
#include <utility>

#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using rtcp::TransportFeedback;
using ::testing::ElementsAreArray;

static const int kHeaderSize = 20;
static const int kStatusChunkSize = 2;
static const int kSmallDeltaSize = 1;
static const int kLargeDeltaSize = 2;

static const int64_t kDeltaLimit = 0xFF * TransportFeedback::kDeltaScaleFactor;

class FeedbackTester {
 public:
  FeedbackTester() : FeedbackTester(true) {}
  explicit FeedbackTester(bool include_timestamps)
      : expected_size_(kAnySize),
        default_delta_(TransportFeedback::kDeltaScaleFactor * 4),
        include_timestamps_(include_timestamps) {}

  void WithExpectedSize(size_t expected_size) {
    expected_size_ = expected_size;
  }

  void WithDefaultDelta(int64_t delta) { default_delta_ = delta; }

  void WithInput(const uint16_t received_seq[],
                 const int64_t received_ts[],
                 uint16_t length) {
    std::unique_ptr<int64_t[]> temp_timestamps;
    if (received_ts == nullptr) {
      temp_timestamps.reset(new int64_t[length]);
      GenerateReceiveTimestamps(received_seq, length, temp_timestamps.get());
      received_ts = temp_timestamps.get();
    }

    expected_seq_.clear();
    expected_deltas_.clear();
    feedback_.reset(new TransportFeedback(include_timestamps_));
    feedback_->SetBase(received_seq[0], received_ts[0]);
    ASSERT_TRUE(feedback_->IsConsistent());

    int64_t last_time = feedback_->GetBaseTimeUs();
    for (int i = 0; i < length; ++i) {
      int64_t time = received_ts[i];
      EXPECT_TRUE(feedback_->AddReceivedPacket(received_seq[i], time));

      if (last_time != -1) {
        int64_t delta = time - last_time;
        expected_deltas_.push_back(delta);
      }
      last_time = time;
    }
    ASSERT_TRUE(feedback_->IsConsistent());
    expected_seq_.insert(expected_seq_.begin(), &received_seq[0],
                         &received_seq[length]);
  }

  void VerifyPacket() {
    ASSERT_TRUE(feedback_->IsConsistent());
    serialized_ = feedback_->Build();
    VerifyInternal();
    feedback_ =
        TransportFeedback::ParseFrom(serialized_.data(), serialized_.size());
    ASSERT_NE(nullptr, feedback_);
    ASSERT_TRUE(feedback_->IsConsistent());
    EXPECT_EQ(include_timestamps_, feedback_->IncludeTimestamps());
    VerifyInternal();
  }

  static const size_t kAnySize = static_cast<size_t>(0) - 1;

 private:
  void VerifyInternal() {
    if (expected_size_ != kAnySize) {
      // Round up to whole 32-bit words.
      size_t expected_size_words = (expected_size_ + 3) / 4;
      size_t expected_size_bytes = expected_size_words * 4;
      EXPECT_EQ(expected_size_bytes, serialized_.size());
    }

    std::vector<uint16_t> actual_seq_nos;
    std::vector<int64_t> actual_deltas_us;
    for (const auto& packet : feedback_->GetReceivedPackets()) {
      actual_seq_nos.push_back(packet.sequence_number());
      actual_deltas_us.push_back(packet.delta_us());
    }
    EXPECT_THAT(actual_seq_nos, ElementsAreArray(expected_seq_));
    if (include_timestamps_) {
      EXPECT_THAT(actual_deltas_us, ElementsAreArray(expected_deltas_));
    }
  }

  void GenerateReceiveTimestamps(const uint16_t seq[],
                                 const size_t length,
                                 int64_t* timestamps) {
    uint16_t last_seq = seq[0];
    int64_t offset = 0;

    for (size_t i = 0; i < length; ++i) {
      if (seq[i] < last_seq)
        offset += 0x10000 * default_delta_;
      last_seq = seq[i];

      timestamps[i] = offset + (last_seq * default_delta_);
    }
  }

  std::vector<uint16_t> expected_seq_;
  std::vector<int64_t> expected_deltas_;
  size_t expected_size_;
  int64_t default_delta_;
  std::unique_ptr<TransportFeedback> feedback_;
  rtc::Buffer serialized_;
  bool include_timestamps_;
};

// The following tests use FeedbackTester that simulates received packets as
// specified by the parameters |received_seq[]| and |received_ts[]| (optional).
// The following is verified in these tests:
// - Expected size of serialized packet.
// - Expected sequence numbers and receive deltas.
// - Sequence numbers and receive deltas are persistent after serialization
//   followed by parsing.
// - The internal state of a feedback packet is consistent.
TEST(RtcpPacketTest, TransportFeedbackOneBitVector) {
  const uint16_t kReceived[] = {1, 2, 7, 8, 9, 10, 13};
  const size_t kLength = sizeof(kReceived) / sizeof(uint16_t);
  const size_t kExpectedSizeBytes =
      kHeaderSize + kStatusChunkSize + (kLength * kSmallDeltaSize);

  FeedbackTester test;
  test.WithExpectedSize(kExpectedSizeBytes);
  test.WithInput(kReceived, nullptr, kLength);
  test.VerifyPacket();
}

TEST(RtcpPacketTest, TransportFeedbackOneBitVectorNoRecvDelta) {
  const uint16_t kReceived[] = {1, 2, 7, 8, 9, 10, 13};
  const size_t kLength = sizeof(kReceived) / sizeof(uint16_t);
  const size_t kExpectedSizeBytes = kHeaderSize + kStatusChunkSize;

  FeedbackTester test(/*include_timestamps=*/false);
  test.WithExpectedSize(kExpectedSizeBytes);
  test.WithInput(kReceived, nullptr, kLength);
  test.VerifyPacket();
}

TEST(RtcpPacketTest, TransportFeedbackFullOneBitVector) {
  const uint16_t kReceived[] = {1, 2, 7, 8, 9, 10, 13, 14};
  const size_t kLength = sizeof(kReceived) / sizeof(uint16_t);
  const size_t kExpectedSizeBytes =
      kHeaderSize + kStatusChunkSize + (kLength * kSmallDeltaSize);

  FeedbackTester test;
  test.WithExpectedSize(kExpectedSizeBytes);
  test.WithInput(kReceived, nullptr, kLength);
  test.VerifyPacket();
}

TEST(RtcpPacketTest, TransportFeedbackOneBitVectorWrapReceived) {
  const uint16_t kMax = 0xFFFF;
  const uint16_t kReceived[] = {kMax - 2, kMax - 1, kMax, 0, 1, 2};
  const size_t kLength = sizeof(kReceived) / sizeof(uint16_t);
  const size_t kExpectedSizeBytes =
      kHeaderSize + kStatusChunkSize + (kLength * kSmallDeltaSize);

  FeedbackTester test;
  test.WithExpectedSize(kExpectedSizeBytes);
  test.WithInput(kReceived, nullptr, kLength);
  test.VerifyPacket();
}

TEST(RtcpPacketTest, TransportFeedbackOneBitVectorWrapMissing) {
  const uint16_t kMax = 0xFFFF;
  const uint16_t kReceived[] = {kMax - 2, kMax - 1, 1, 2};
  const size_t kLength = sizeof(kReceived) / sizeof(uint16_t);
  const size_t kExpectedSizeBytes =
      kHeaderSize + kStatusChunkSize + (kLength * kSmallDeltaSize);

  FeedbackTester test;
  test.WithExpectedSize(kExpectedSizeBytes);
  test.WithInput(kReceived, nullptr, kLength);
  test.VerifyPacket();
}

TEST(RtcpPacketTest, TransportFeedbackTwoBitVector) {
  const uint16_t kReceived[] = {1, 2, 6, 7};
  const size_t kLength = sizeof(kReceived) / sizeof(uint16_t);
  const size_t kExpectedSizeBytes =
      kHeaderSize + kStatusChunkSize + (kLength * kLargeDeltaSize);

  FeedbackTester test;
  test.WithExpectedSize(kExpectedSizeBytes);
  test.WithDefaultDelta(kDeltaLimit + TransportFeedback::kDeltaScaleFactor);
  test.WithInput(kReceived, nullptr, kLength);
  test.VerifyPacket();
}

TEST(RtcpPacketTest, TransportFeedbackTwoBitVectorFull) {
  const uint16_t kReceived[] = {1, 2, 6, 7, 8};
  const size_t kLength = sizeof(kReceived) / sizeof(uint16_t);
  const size_t kExpectedSizeBytes =
      kHeaderSize + (2 * kStatusChunkSize) + (kLength * kLargeDeltaSize);

  FeedbackTester test;
  test.WithExpectedSize(kExpectedSizeBytes);
  test.WithDefaultDelta(kDeltaLimit + TransportFeedback::kDeltaScaleFactor);
  test.WithInput(kReceived, nullptr, kLength);
  test.VerifyPacket();
}

TEST(RtcpPacketTest, TransportFeedbackLargeAndNegativeDeltas) {
  const uint16_t kReceived[] = {1, 2, 6, 7, 8};
  const int64_t kReceiveTimes[] = {
      2000, 1000, 4000, 3000,
      3000 + TransportFeedback::kDeltaScaleFactor * (1 << 8)};
  const size_t kLength = sizeof(kReceived) / sizeof(uint16_t);
  const size_t kExpectedSizeBytes =
      kHeaderSize + kStatusChunkSize + (3 * kLargeDeltaSize) + kSmallDeltaSize;

  FeedbackTester test;
  test.WithExpectedSize(kExpectedSizeBytes);
  test.WithInput(kReceived, kReceiveTimes, kLength);
  test.VerifyPacket();
}

TEST(RtcpPacketTest, TransportFeedbackMaxRle) {
  // Expected chunks created:
  // * 1-bit vector chunk (1xreceived + 13xdropped)
  // * RLE chunk of max length for dropped symbol
  // * 1-bit vector chunk (1xreceived + 13xdropped)

  const size_t kPacketCount = (1 << 13) - 1 + 14;
  const uint16_t kReceived[] = {0, kPacketCount};
  const int64_t kReceiveTimes[] = {1000, 2000};
  const size_t kLength = sizeof(kReceived) / sizeof(uint16_t);
  const size_t kExpectedSizeBytes =
      kHeaderSize + (3 * kStatusChunkSize) + (kLength * kSmallDeltaSize);

  FeedbackTester test;
  test.WithExpectedSize(kExpectedSizeBytes);
  test.WithInput(kReceived, kReceiveTimes, kLength);
  test.VerifyPacket();
}

TEST(RtcpPacketTest, TransportFeedbackMinRle) {
  // Expected chunks created:
  // * 1-bit vector chunk (1xreceived + 13xdropped)
  // * RLE chunk of length 15 for dropped symbol
  // * 1-bit vector chunk (1xreceived + 13xdropped)

  const uint16_t kReceived[] = {0, (14 * 2) + 1};
  const int64_t kReceiveTimes[] = {1000, 2000};
  const size_t kLength = sizeof(kReceived) / sizeof(uint16_t);
  const size_t kExpectedSizeBytes =
      kHeaderSize + (3 * kStatusChunkSize) + (kLength * kSmallDeltaSize);

  FeedbackTester test;
  test.WithExpectedSize(kExpectedSizeBytes);
  test.WithInput(kReceived, kReceiveTimes, kLength);
  test.VerifyPacket();
}

TEST(RtcpPacketTest, TransportFeedbackOneToTwoBitVector) {
  const size_t kTwoBitVectorCapacity = 7;
  const uint16_t kReceived[] = {0, kTwoBitVectorCapacity - 1};
  const int64_t kReceiveTimes[] = {
      0, kDeltaLimit + TransportFeedback::kDeltaScaleFactor};
  const size_t kLength = sizeof(kReceived) / sizeof(uint16_t);
  const size_t kExpectedSizeBytes =
      kHeaderSize + kStatusChunkSize + kSmallDeltaSize + kLargeDeltaSize;

  FeedbackTester test;
  test.WithExpectedSize(kExpectedSizeBytes);
  test.WithInput(kReceived, kReceiveTimes, kLength);
  test.VerifyPacket();
}

TEST(RtcpPacketTest, TransportFeedbackOneToTwoBitVectorSimpleSplit) {
  const size_t kTwoBitVectorCapacity = 7;
  const uint16_t kReceived[] = {0, kTwoBitVectorCapacity};
  const int64_t kReceiveTimes[] = {
      0, kDeltaLimit + TransportFeedback::kDeltaScaleFactor};
  const size_t kLength = sizeof(kReceived) / sizeof(uint16_t);
  const size_t kExpectedSizeBytes =
      kHeaderSize + (kStatusChunkSize * 2) + kSmallDeltaSize + kLargeDeltaSize;

  FeedbackTester test;
  test.WithExpectedSize(kExpectedSizeBytes);
  test.WithInput(kReceived, kReceiveTimes, kLength);
  test.VerifyPacket();
}

TEST(RtcpPacketTest, TransportFeedbackOneToTwoBitVectorSplit) {
  // With received small delta = S, received large delta = L, use input
  // SSSSSSSSLSSSSSSSSSSSS. This will cause a 1:2 split at the L.
  // After split there will be two symbols in symbol_vec: SL.

  const int64_t kLargeDelta = TransportFeedback::kDeltaScaleFactor * (1 << 8);
  const size_t kNumPackets = (3 * 7) + 1;
  const size_t kExpectedSizeBytes = kHeaderSize + (kStatusChunkSize * 3) +
                                    (kSmallDeltaSize * (kNumPackets - 1)) +
                                    (kLargeDeltaSize * 1);

  uint16_t kReceived[kNumPackets];
  for (size_t i = 0; i < kNumPackets; ++i)
    kReceived[i] = i;

  int64_t kReceiveTimes[kNumPackets];
  kReceiveTimes[0] = 1000;
  for (size_t i = 1; i < kNumPackets; ++i) {
    int delta = (i == 8) ? kLargeDelta : 1000;
    kReceiveTimes[i] = kReceiveTimes[i - 1] + delta;
  }

  FeedbackTester test;
  test.WithExpectedSize(kExpectedSizeBytes);
  test.WithInput(kReceived, kReceiveTimes, kNumPackets);
  test.VerifyPacket();
}

TEST(RtcpPacketTest, TransportFeedbackAliasing) {
  TransportFeedback feedback;
  feedback.SetBase(0, 0);

  const int kSamples = 100;
  const int64_t kTooSmallDelta = TransportFeedback::kDeltaScaleFactor / 3;

  for (int i = 0; i < kSamples; ++i)
    feedback.AddReceivedPacket(i, i * kTooSmallDelta);

  feedback.Build();

  int64_t accumulated_delta = 0;
  int num_samples = 0;
  for (const auto& packet : feedback.GetReceivedPackets()) {
    accumulated_delta += packet.delta_us();
    int64_t expected_time = num_samples * kTooSmallDelta;
    ++num_samples;

    EXPECT_NEAR(expected_time, accumulated_delta,
                TransportFeedback::kDeltaScaleFactor / 2);
  }
}

TEST(RtcpPacketTest, TransportFeedbackLimits) {
  // Sequence number wrap above 0x8000.
  std::unique_ptr<TransportFeedback> packet(new TransportFeedback());
  packet->SetBase(0, 0);
  EXPECT_TRUE(packet->AddReceivedPacket(0x0, 0));
  EXPECT_TRUE(packet->AddReceivedPacket(0x8000, 1000));

  packet.reset(new TransportFeedback());
  packet->SetBase(0, 0);
  EXPECT_TRUE(packet->AddReceivedPacket(0x0, 0));
  EXPECT_FALSE(packet->AddReceivedPacket(0x8000 + 1, 1000));

  // Packet status count max 0xFFFF.
  packet.reset(new TransportFeedback());
  packet->SetBase(0, 0);
  EXPECT_TRUE(packet->AddReceivedPacket(0x0, 0));
  EXPECT_TRUE(packet->AddReceivedPacket(0x8000, 1000));
  EXPECT_TRUE(packet->AddReceivedPacket(0xFFFE, 2000));
  EXPECT_FALSE(packet->AddReceivedPacket(0xFFFF, 3000));

  // Too large delta.
  packet.reset(new TransportFeedback());
  packet->SetBase(0, 0);
  int64_t kMaxPositiveTimeDelta = std::numeric_limits<int16_t>::max() *
                                  TransportFeedback::kDeltaScaleFactor;
  EXPECT_FALSE(packet->AddReceivedPacket(
      1, kMaxPositiveTimeDelta + TransportFeedback::kDeltaScaleFactor));
  EXPECT_TRUE(packet->AddReceivedPacket(1, kMaxPositiveTimeDelta));

  // Too large negative delta.
  packet.reset(new TransportFeedback());
  packet->SetBase(0, 0);
  int64_t kMaxNegativeTimeDelta = std::numeric_limits<int16_t>::min() *
                                  TransportFeedback::kDeltaScaleFactor;
  EXPECT_FALSE(packet->AddReceivedPacket(
      1, kMaxNegativeTimeDelta - TransportFeedback::kDeltaScaleFactor));
  EXPECT_TRUE(packet->AddReceivedPacket(1, kMaxNegativeTimeDelta));

  // Base time at maximum value.
  int64_t kMaxBaseTime =
      static_cast<int64_t>(TransportFeedback::kDeltaScaleFactor) * (1L << 8) *
      ((1L << 23) - 1);
  packet.reset(new TransportFeedback());
  packet->SetBase(0, kMaxBaseTime);
  EXPECT_TRUE(packet->AddReceivedPacket(0, kMaxBaseTime));
  // Serialize and de-serialize (verify 24bit parsing).
  rtc::Buffer raw_packet = packet->Build();
  packet = TransportFeedback::ParseFrom(raw_packet.data(), raw_packet.size());
  EXPECT_EQ(kMaxBaseTime, packet->GetBaseTimeUs());

  // Base time above maximum value.
  int64_t kTooLargeBaseTime =
      kMaxBaseTime + (TransportFeedback::kDeltaScaleFactor * (1L << 8));
  packet.reset(new TransportFeedback());
  packet->SetBase(0, kTooLargeBaseTime);
  packet->AddReceivedPacket(0, kTooLargeBaseTime);
  raw_packet = packet->Build();
  packet = TransportFeedback::ParseFrom(raw_packet.data(), raw_packet.size());
  EXPECT_NE(kTooLargeBaseTime, packet->GetBaseTimeUs());

  // TODO(sprang): Once we support max length lower than RTCP length limit,
  // add back test for max size in bytes.
}

TEST(RtcpPacketTest, TransportFeedbackPadding) {
  const size_t kExpectedSizeBytes =
      kHeaderSize + kStatusChunkSize + kSmallDeltaSize;
  const size_t kExpectedSizeWords = (kExpectedSizeBytes + 3) / 4;
  const size_t kExpectedPaddingSizeBytes =
      4 * kExpectedSizeWords - kExpectedSizeBytes;

  TransportFeedback feedback;
  feedback.SetBase(0, 0);
  EXPECT_TRUE(feedback.AddReceivedPacket(0, 0));

  rtc::Buffer packet = feedback.Build();
  EXPECT_EQ(kExpectedSizeWords * 4, packet.size());
  ASSERT_GT(kExpectedSizeWords * 4, kExpectedSizeBytes);
  for (size_t i = kExpectedSizeBytes; i < (kExpectedSizeWords * 4 - 1); ++i)
    EXPECT_EQ(0u, packet[i]);

  EXPECT_EQ(kExpectedPaddingSizeBytes, packet[kExpectedSizeWords * 4 - 1]);

  // Modify packet by adding 4 bytes of padding at the end. Not currently used
  // when we're sending, but need to be able to handle it when receiving.

  const int kPaddingBytes = 4;
  const size_t kExpectedSizeWithPadding =
      (kExpectedSizeWords * 4) + kPaddingBytes;
  uint8_t mod_buffer[kExpectedSizeWithPadding];
  memcpy(mod_buffer, packet.data(), kExpectedSizeWords * 4);
  memset(&mod_buffer[kExpectedSizeWords * 4], 0, kPaddingBytes - 1);
  mod_buffer[kExpectedSizeWithPadding - 1] =
      kPaddingBytes + kExpectedPaddingSizeBytes;
  const uint8_t padding_flag = 1 << 5;
  mod_buffer[0] |= padding_flag;
  ByteWriter<uint16_t>::WriteBigEndian(
      &mod_buffer[2], ByteReader<uint16_t>::ReadBigEndian(&mod_buffer[2]) +
                          ((kPaddingBytes + 3) / 4));

  std::unique_ptr<TransportFeedback> parsed_packet(
      TransportFeedback::ParseFrom(mod_buffer, kExpectedSizeWithPadding));
  ASSERT_TRUE(parsed_packet != nullptr);
  EXPECT_EQ(kExpectedSizeWords * 4, packet.size());  // Padding not included.
}

TEST(RtcpPacketTest, TransportFeedbackPaddingBackwardsCompatibility) {
  const size_t kExpectedSizeBytes =
      kHeaderSize + kStatusChunkSize + kSmallDeltaSize;
  const size_t kExpectedSizeWords = (kExpectedSizeBytes + 3) / 4;
  const size_t kExpectedPaddingSizeBytes =
      4 * kExpectedSizeWords - kExpectedSizeBytes;

  TransportFeedback feedback;
  feedback.SetBase(0, 0);
  EXPECT_TRUE(feedback.AddReceivedPacket(0, 0));

  rtc::Buffer packet = feedback.Build();
  EXPECT_EQ(kExpectedSizeWords * 4, packet.size());
  ASSERT_GT(kExpectedSizeWords * 4, kExpectedSizeBytes);
  for (size_t i = kExpectedSizeBytes; i < (kExpectedSizeWords * 4 - 1); ++i)
    EXPECT_EQ(0u, packet[i]);

  EXPECT_GT(kExpectedPaddingSizeBytes, 0u);
  EXPECT_EQ(kExpectedPaddingSizeBytes, packet[kExpectedSizeWords * 4 - 1]);

  // Modify packet by removing padding bit and writing zero at the last padding
  // byte to verify that we can parse packets from old clients, where zero
  // padding of up to three bytes was used without the padding bit being set.
  uint8_t mod_buffer[kExpectedSizeWords * 4];
  memcpy(mod_buffer, packet.data(), kExpectedSizeWords * 4);
  mod_buffer[kExpectedSizeWords * 4 - 1] = 0;
  const uint8_t padding_flag = 1 << 5;
  mod_buffer[0] &= ~padding_flag;  // Unset padding flag.

  std::unique_ptr<TransportFeedback> parsed_packet(
      TransportFeedback::ParseFrom(mod_buffer, kExpectedSizeWords * 4));
  ASSERT_TRUE(parsed_packet != nullptr);
  EXPECT_EQ(kExpectedSizeWords * 4, packet.size());
}

TEST(RtcpPacketTest, TransportFeedbackCorrectlySplitsVectorChunks) {
  const int kOneBitVectorCapacity = 14;
  const int64_t kLargeTimeDelta =
      TransportFeedback::kDeltaScaleFactor * (1 << 8);

  // Test that a number of small deltas followed by a large delta results in a
  // correct split into multiple chunks, as needed.

  for (int deltas = 0; deltas <= kOneBitVectorCapacity + 1; ++deltas) {
    TransportFeedback feedback;
    feedback.SetBase(0, 0);
    for (int i = 0; i < deltas; ++i)
      feedback.AddReceivedPacket(i, i * 1000);
    feedback.AddReceivedPacket(deltas, deltas * 1000 + kLargeTimeDelta);

    rtc::Buffer serialized_packet = feedback.Build();
    std::unique_ptr<TransportFeedback> deserialized_packet =
        TransportFeedback::ParseFrom(serialized_packet.data(),
                                     serialized_packet.size());
    EXPECT_TRUE(deserialized_packet != nullptr);
  }
}

TEST(RtcpPacketTest, TransportFeedbackMoveConstructor) {
  const int kSamples = 100;
  const int64_t kDelta = TransportFeedback::kDeltaScaleFactor;
  const uint16_t kBaseSeqNo = 7531;
  const int64_t kBaseTimestampUs = 123456789;
  const uint8_t kFeedbackSeqNo = 90;

  TransportFeedback feedback;
  feedback.SetBase(kBaseSeqNo, kBaseTimestampUs);
  feedback.SetFeedbackSequenceNumber(kFeedbackSeqNo);
  for (int i = 0; i < kSamples; ++i) {
    feedback.AddReceivedPacket(kBaseSeqNo + i, kBaseTimestampUs + i * kDelta);
  }
  EXPECT_TRUE(feedback.IsConsistent());

  TransportFeedback feedback_copy(feedback);
  EXPECT_TRUE(feedback_copy.IsConsistent());
  EXPECT_TRUE(feedback.IsConsistent());
  EXPECT_EQ(feedback_copy.Build(), feedback.Build());

  TransportFeedback moved(std::move(feedback));
  EXPECT_TRUE(moved.IsConsistent());
  EXPECT_TRUE(feedback.IsConsistent());
  EXPECT_EQ(moved.Build(), feedback_copy.Build());
}

TEST(TransportFeedbackTest, ReportsMissingPackets) {
  const uint16_t kBaseSeqNo = 1000;
  const int64_t kBaseTimestampUs = 10000;
  const uint8_t kFeedbackSeqNo = 90;
  TransportFeedback feedback_builder(/*include_timestamps*/ true);
  feedback_builder.SetBase(kBaseSeqNo, kBaseTimestampUs);
  feedback_builder.SetFeedbackSequenceNumber(kFeedbackSeqNo);
  feedback_builder.AddReceivedPacket(kBaseSeqNo + 0, kBaseTimestampUs);
  // Packet losses indicated by jump in sequence number.
  feedback_builder.AddReceivedPacket(kBaseSeqNo + 3, kBaseTimestampUs + 2000);
  rtc::Buffer coded = feedback_builder.Build();

  rtcp::CommonHeader header;
  header.Parse(coded.data(), coded.size());
  TransportFeedback feedback(/*include_timestamps*/ true,
                             /*include_lost*/ true);
  feedback.Parse(header);
  auto packets = feedback.GetAllPackets();
  EXPECT_TRUE(packets[0].received());
  EXPECT_FALSE(packets[1].received());
  EXPECT_FALSE(packets[2].received());
  EXPECT_TRUE(packets[3].received());
}

TEST(TransportFeedbackTest, ReportsMissingPacketsWithoutTimestamps) {
  const uint16_t kBaseSeqNo = 1000;
  const uint8_t kFeedbackSeqNo = 90;
  TransportFeedback feedback_builder(/*include_timestamps*/ false);
  feedback_builder.SetBase(kBaseSeqNo, 10000);
  feedback_builder.SetFeedbackSequenceNumber(kFeedbackSeqNo);
  feedback_builder.AddReceivedPacket(kBaseSeqNo + 0, /*timestamp_us*/ 0);
  // Packet losses indicated by jump in sequence number.
  feedback_builder.AddReceivedPacket(kBaseSeqNo + 3, /*timestamp_us*/ 0);
  rtc::Buffer coded = feedback_builder.Build();

  rtcp::CommonHeader header;
  header.Parse(coded.data(), coded.size());
  TransportFeedback feedback(/*include_timestamps*/ true,
                             /*include_lost*/ true);
  feedback.Parse(header);
  auto packets = feedback.GetAllPackets();
  EXPECT_TRUE(packets[0].received());
  EXPECT_FALSE(packets[1].received());
  EXPECT_FALSE(packets[2].received());
  EXPECT_TRUE(packets[3].received());
}
}  // namespace
}  // namespace webrtc
