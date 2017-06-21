/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "webrtc/modules/rtp_rtcp/source/rtp_format_vp8.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format_vp8_test_helper.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"
#include "webrtc/typedefs.h"

#define CHECK_ARRAY_SIZE(expected_size, array)                     \
  static_assert(expected_size == sizeof(array) / sizeof(array[0]), \
                "check array size");

namespace webrtc {
namespace {

using ::testing::ElementsAreArray;
using ::testing::make_tuple;

constexpr RtpPacketToSend::ExtensionManager* kNoExtensions = nullptr;
// Payload descriptor
//       0 1 2 3 4 5 6 7
//      +-+-+-+-+-+-+-+-+
//      |X|R|N|S|PartID | (REQUIRED)
//      +-+-+-+-+-+-+-+-+
// X:   |I|L|T|K|  RSV  | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
// I:   |   PictureID   | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
// L:   |   TL0PICIDX   | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
// T/K: |TID:Y| KEYIDX  | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
//
// Payload header
//       0 1 2 3 4 5 6 7
//      +-+-+-+-+-+-+-+-+
//      |Size0|H| VER |P|
//      +-+-+-+-+-+-+-+-+
//      |     Size1     |
//      +-+-+-+-+-+-+-+-+
//      |     Size2     |
//      +-+-+-+-+-+-+-+-+
//      | Bytes 4..N of |
//      | VP8 payload   |
//      :               :
//      +-+-+-+-+-+-+-+-+
//      | OPTIONAL RTP  |
//      | padding       |
//      :               :
//      +-+-+-+-+-+-+-+-+
void VerifyBasicHeader(RTPTypeHeader* type, bool N, bool S, int part_id) {
  ASSERT_TRUE(type != NULL);
  EXPECT_EQ(N, type->Video.codecHeader.VP8.nonReference);
  EXPECT_EQ(S, type->Video.codecHeader.VP8.beginningOfPartition);
  EXPECT_EQ(part_id, type->Video.codecHeader.VP8.partitionId);
}

void VerifyExtensions(RTPTypeHeader* type,
                      int16_t picture_id,   /* I */
                      int16_t tl0_pic_idx,  /* L */
                      uint8_t temporal_idx, /* T */
                      int key_idx /* K */) {
  ASSERT_TRUE(type != NULL);
  EXPECT_EQ(picture_id, type->Video.codecHeader.VP8.pictureId);
  EXPECT_EQ(tl0_pic_idx, type->Video.codecHeader.VP8.tl0PicIdx);
  EXPECT_EQ(temporal_idx, type->Video.codecHeader.VP8.temporalIdx);
  EXPECT_EQ(key_idx, type->Video.codecHeader.VP8.keyIdx);
}
}  // namespace

class RtpPacketizerVp8Test : public ::testing::Test {
 protected:
  RtpPacketizerVp8Test() : helper_(NULL) {}
  virtual void TearDown() { delete helper_; }
  bool Init(const size_t* partition_sizes, size_t num_partitions) {
    hdr_info_.pictureId = kNoPictureId;
    hdr_info_.nonReference = false;
    hdr_info_.temporalIdx = kNoTemporalIdx;
    hdr_info_.layerSync = false;
    hdr_info_.tl0PicIdx = kNoTl0PicIdx;
    hdr_info_.keyIdx = kNoKeyIdx;
    if (helper_ != NULL)
      return false;
    helper_ = new test::RtpFormatVp8TestHelper(&hdr_info_);
    return helper_->Init(partition_sizes, num_partitions);
  }

  RTPVideoHeaderVP8 hdr_info_;
  test::RtpFormatVp8TestHelper* helper_;
};

TEST_F(RtpPacketizerVp8Test, TestStrictMode) {
  const size_t kSizeVector[] = {10, 8, 27};
  const size_t kNumPartitions = GTEST_ARRAY_SIZE_(kSizeVector);
  ASSERT_TRUE(Init(kSizeVector, kNumPartitions));

  hdr_info_.pictureId = 200;  // > 0x7F should produce 2-byte PictureID.
  const size_t kMaxPayloadSize = 13;
  RtpPacketizerVp8 packetizer(hdr_info_, kMaxPayloadSize, 0, kStrict);
  size_t num_packets = packetizer.SetPayloadData(helper_->payload_data(),
                                                 helper_->payload_size(),
                                                 helper_->fragmentation());

  // The expected sizes are obtained by hand.
  const size_t kExpectedSizes[] = {9, 9, 12, 13, 13, 13};
  const int kExpectedPart[] = {0, 0, 1, 2, 2, 2};
  const bool kExpectedFragStart[] = {true, false, true, true, false, false};
  const size_t kExpectedNum = GTEST_ARRAY_SIZE_(kExpectedSizes);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedPart);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedFragStart);
  ASSERT_EQ(num_packets, kExpectedNum);

  helper_->GetAllPacketsAndCheck(&packetizer,
                                 kExpectedSizes,
                                 kExpectedPart,
                                 kExpectedFragStart,
                                 kExpectedNum);
}

// Verify that we get a minimal number of packets if the partition plus header
// size fits exactly in the maximum packet size.
TEST_F(RtpPacketizerVp8Test, TestStrictEqualTightPartitions) {
  const size_t kSizeVector[] = {10, 10, 10};
  const size_t kNumPartitions = GTEST_ARRAY_SIZE_(kSizeVector);
  ASSERT_TRUE(Init(kSizeVector, kNumPartitions));

  hdr_info_.pictureId = 200;  // > 0x7F should produce 2-byte PictureID.
  const int kMaxPayloadSize = 14;
  RtpPacketizerVp8 packetizer(hdr_info_, kMaxPayloadSize, 0, kStrict);
  size_t num_packets = packetizer.SetPayloadData(helper_->payload_data(),
                                                 helper_->payload_size(),
                                                 helper_->fragmentation());

  // The expected sizes are obtained by hand.
  const size_t kExpectedSizes[] = {14, 14, 14};
  const int kExpectedPart[] = {0, 1, 2};
  const bool kExpectedFragStart[] = {true, true, true};
  const size_t kExpectedNum = GTEST_ARRAY_SIZE_(kExpectedSizes);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedPart);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedFragStart);
  ASSERT_EQ(num_packets, kExpectedNum);

  helper_->GetAllPacketsAndCheck(&packetizer, kExpectedSizes, kExpectedPart,
                                 kExpectedFragStart, kExpectedNum);
}

TEST_F(RtpPacketizerVp8Test, TestAggregateMode) {
  const size_t kSizeVector[] = {60, 10, 10};
  const size_t kNumPartitions = GTEST_ARRAY_SIZE_(kSizeVector);
  ASSERT_TRUE(Init(kSizeVector, kNumPartitions));

  hdr_info_.pictureId = 20;  // <= 0x7F should produce 1-byte PictureID.
  const size_t kMaxPayloadSize = 25;
  RtpPacketizerVp8 packetizer(hdr_info_, kMaxPayloadSize, 0, kAggregate);
  size_t num_packets = packetizer.SetPayloadData(helper_->payload_data(),
                                                 helper_->payload_size(),
                                                 helper_->fragmentation());

  // The expected sizes are obtained by hand.
  const size_t kExpectedSizes[] = {23, 23, 23, 23};
  const int kExpectedPart[] = {0, 0, 0, 1};
  const bool kExpectedFragStart[] = {true, false, false, true};
  const size_t kExpectedNum = GTEST_ARRAY_SIZE_(kExpectedSizes);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedPart);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedFragStart);
  ASSERT_EQ(num_packets, kExpectedNum);

  helper_->GetAllPacketsAndCheck(&packetizer,
                                 kExpectedSizes,
                                 kExpectedPart,
                                 kExpectedFragStart,
                                 kExpectedNum);
}

TEST_F(RtpPacketizerVp8Test, TestAggregateModePacketReductionCauseExtraPacket) {
  const size_t kSizeVector[] = {60, 10, 10};
  const size_t kNumPartitions = GTEST_ARRAY_SIZE_(kSizeVector);
  ASSERT_TRUE(Init(kSizeVector, kNumPartitions));

  hdr_info_.pictureId = 20;  // <= 0x7F should produce 1-byte PictureID.
  const size_t kMaxPayloadSize = 25;
  const size_t kLastPacketReductionLen = 5;
  RtpPacketizerVp8 packetizer(hdr_info_, kMaxPayloadSize,
                              kLastPacketReductionLen, kAggregate);
  size_t num_packets = packetizer.SetPayloadData(helper_->payload_data(),
                                                 helper_->payload_size(),
                                                 helper_->fragmentation());

  // The expected sizes are obtained by hand.
  const size_t kExpectedSizes[] = {23, 23, 23, 13, 13};
  const int kExpectedPart[] = {0, 0, 0, 1, 2};
  const bool kExpectedFragStart[] = {true, false, false, true, true};
  const size_t kExpectedNum = GTEST_ARRAY_SIZE_(kExpectedSizes);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedPart);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedFragStart);
  ASSERT_EQ(num_packets, kExpectedNum);

  helper_->GetAllPacketsAndCheck(&packetizer, kExpectedSizes, kExpectedPart,
                                 kExpectedFragStart, kExpectedNum);
}

TEST_F(RtpPacketizerVp8Test, TestAggregateModePacketReduction) {
  const size_t kSizeVector[] = {60, 10, 10};
  const size_t kNumPartitions = GTEST_ARRAY_SIZE_(kSizeVector);
  ASSERT_TRUE(Init(kSizeVector, kNumPartitions));

  hdr_info_.pictureId = 20;  // <= 0x7F should produce 1-byte PictureID.
  const size_t kMaxPayloadSize = 25;
  const size_t kLastPacketReductionLen = 1;
  RtpPacketizerVp8 packetizer(hdr_info_, kMaxPayloadSize,
                              kLastPacketReductionLen, kAggregate);
  size_t num_packets = packetizer.SetPayloadData(helper_->payload_data(),
                                                 helper_->payload_size(),
                                                 helper_->fragmentation());

  // The expected sizes are obtained by hand.
  const size_t kExpectedSizes[] = {23, 23, 23, 23};
  const int kExpectedPart[] = {0, 0, 0, 1};
  const bool kExpectedFragStart[] = {true, false, false, true};
  const size_t kExpectedNum = GTEST_ARRAY_SIZE_(kExpectedSizes);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedPart);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedFragStart);
  ASSERT_EQ(num_packets, kExpectedNum);

  helper_->GetAllPacketsAndCheck(&packetizer, kExpectedSizes, kExpectedPart,
                                 kExpectedFragStart, kExpectedNum);
}

TEST_F(RtpPacketizerVp8Test, TestAggregateModeSmallPartitions) {
  const size_t kSizeVector[] = {3, 4, 2, 5, 2, 4};
  const size_t kNumPartitions = GTEST_ARRAY_SIZE_(kSizeVector);
  ASSERT_TRUE(Init(kSizeVector, kNumPartitions));

  hdr_info_.pictureId = 20;  // <= 0x7F should produce 1-byte PictureID.
  const size_t kMaxPayloadSize = 13;
  RtpPacketizerVp8 packetizer(hdr_info_, kMaxPayloadSize, 0, kAggregate);
  size_t num_packets = packetizer.SetPayloadData(helper_->payload_data(),
                                                 helper_->payload_size(),
                                                 helper_->fragmentation());

  // The expected sizes are obtained by hand.
  const size_t kExpectedSizes[] = {10, 10, 9};
  const int kExpectedPart[] = {0, 2, 4};
  const bool kExpectedFragStart[] = {true, true, true};
  const size_t kExpectedNum = GTEST_ARRAY_SIZE_(kExpectedSizes);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedPart);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedFragStart);
  ASSERT_EQ(num_packets, kExpectedNum);

  helper_->GetAllPacketsAndCheck(&packetizer, kExpectedSizes, kExpectedPart,
                                 kExpectedFragStart, kExpectedNum);
}

TEST_F(RtpPacketizerVp8Test, TestAggregateModeManyPartitions1) {
  const size_t kSizeVector[] = {1600, 200, 200, 200, 200, 200, 200, 200, 200};
  const size_t kNumPartitions = GTEST_ARRAY_SIZE_(kSizeVector);
  ASSERT_TRUE(Init(kSizeVector, kNumPartitions));

  hdr_info_.pictureId = 20;  // <= 0x7F should produce 1-byte PictureID.
  const size_t kMaxPayloadSize = 1000;
  RtpPacketizerVp8 packetizer(hdr_info_, kMaxPayloadSize, 0, kAggregate);
  size_t num_packets = packetizer.SetPayloadData(helper_->payload_data(),
                                                 helper_->payload_size(),
                                                 helper_->fragmentation());

  // The expected sizes are obtained by hand.
  const size_t kExpectedSizes[] = {803, 803, 803, 803};
  const int kExpectedPart[] = {0, 0, 1, 5};
  const bool kExpectedFragStart[] = {true, false, true, true};
  const size_t kExpectedNum = GTEST_ARRAY_SIZE_(kExpectedSizes);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedPart);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedFragStart);
  ASSERT_EQ(num_packets, kExpectedNum);

  helper_->GetAllPacketsAndCheck(&packetizer,
                                 kExpectedSizes,
                                 kExpectedPart,
                                 kExpectedFragStart,
                                 kExpectedNum);
}

TEST_F(RtpPacketizerVp8Test, TestAggregateModeManyPartitions2) {
  const size_t kSizeVector[] = {1599, 200, 200, 200, 1600, 200, 200, 200, 200};
  const size_t kNumPartitions = GTEST_ARRAY_SIZE_(kSizeVector);
  ASSERT_TRUE(Init(kSizeVector, kNumPartitions));

  hdr_info_.pictureId = 20;  // <= 0x7F should produce 1-byte PictureID.
  const size_t kMaxPayloadSize = 1000;
  RtpPacketizerVp8 packetizer(hdr_info_, kMaxPayloadSize, 0, kAggregate);
  size_t num_packets = packetizer.SetPayloadData(helper_->payload_data(),
                                                 helper_->payload_size(),
                                                 helper_->fragmentation());

  // The expected sizes are obtained by hand.
  const size_t kExpectedSizes[] = {802, 803, 603, 803, 803, 803};
  const int kExpectedPart[] = {0, 0, 1, 4, 4, 5};
  const bool kExpectedFragStart[] = {true, false, true, true, false, true};
  const size_t kExpectedNum = GTEST_ARRAY_SIZE_(kExpectedSizes);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedPart);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedFragStart);
  ASSERT_EQ(num_packets, kExpectedNum);

  helper_->GetAllPacketsAndCheck(&packetizer,
                                 kExpectedSizes,
                                 kExpectedPart,
                                 kExpectedFragStart,
                                 kExpectedNum);
}

TEST_F(RtpPacketizerVp8Test, TestAggregateModeTwoLargePartitions) {
  const size_t kSizeVector[] = {1654, 2268};
  const size_t kNumPartitions = GTEST_ARRAY_SIZE_(kSizeVector);
  ASSERT_TRUE(Init(kSizeVector, kNumPartitions));

  hdr_info_.pictureId = 20;  // <= 0x7F should produce 1-byte PictureID.
  const size_t kMaxPayloadSize = 1460;
  RtpPacketizerVp8 packetizer(hdr_info_, kMaxPayloadSize, 0, kAggregate);
  size_t num_packets = packetizer.SetPayloadData(helper_->payload_data(),
                                                 helper_->payload_size(),
                                                 helper_->fragmentation());

  // The expected sizes are obtained by hand.
  const size_t kExpectedSizes[] = {830, 830, 1137, 1137};
  const int kExpectedPart[] = {0, 0, 1, 1};
  const bool kExpectedFragStart[] = {true, false, true, false};
  const size_t kExpectedNum = GTEST_ARRAY_SIZE_(kExpectedSizes);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedPart);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedFragStart);
  ASSERT_EQ(num_packets, kExpectedNum);

  helper_->GetAllPacketsAndCheck(&packetizer,
                                 kExpectedSizes,
                                 kExpectedPart,
                                 kExpectedFragStart,
                                 kExpectedNum);
}

// Verify that EqualSize mode is forced if fragmentation info is missing.
TEST_F(RtpPacketizerVp8Test, TestEqualSizeModeFallback) {
  const size_t kSizeVector[] = {10, 10, 10};
  const size_t kNumPartitions = GTEST_ARRAY_SIZE_(kSizeVector);
  ASSERT_TRUE(Init(kSizeVector, kNumPartitions));

  hdr_info_.pictureId = 200;          // > 0x7F should produce 2-byte PictureID
  const size_t kMaxPayloadSize = 12;  // Small enough to produce 4 packets.
  RtpPacketizerVp8 packetizer(hdr_info_, kMaxPayloadSize, 0);
  size_t num_packets = packetizer.SetPayloadData(
      helper_->payload_data(), helper_->payload_size(), nullptr);

  // Expecting three full packets, and one with the remainder.
  const size_t kExpectedSizes[] = {11, 11, 12, 12};
  const int kExpectedPart[] = {0, 0, 0, 0};  // Always 0 for equal size mode.
  // Frag start only true for first packet in equal size mode.
  const bool kExpectedFragStart[] = {true, false, false, false};
  const size_t kExpectedNum = GTEST_ARRAY_SIZE_(kExpectedSizes);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedPart);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedFragStart);
  ASSERT_EQ(num_packets, kExpectedNum);

  helper_->set_sloppy_partitioning(true);
  helper_->GetAllPacketsAndCheck(&packetizer,
                                 kExpectedSizes,
                                 kExpectedPart,
                                 kExpectedFragStart,
                                 kExpectedNum);
}

TEST_F(RtpPacketizerVp8Test, TestEqualSizeWithLastPacketReduction) {
  const size_t kSizeVector[] = {30, 10, 3};
  const size_t kNumPartitions = GTEST_ARRAY_SIZE_(kSizeVector);
  ASSERT_TRUE(Init(kSizeVector, kNumPartitions));

  hdr_info_.pictureId = 200;          // > 0x7F should produce 2-byte PictureID
  const size_t kMaxPayloadSize = 15;  // Small enough to produce 5 packets.
  const size_t kLastPacketReduction = 5;
  RtpPacketizerVp8 packetizer(hdr_info_, kMaxPayloadSize, kLastPacketReduction);
  size_t num_packets = packetizer.SetPayloadData(
      helper_->payload_data(), helper_->payload_size(), nullptr);

  // Calculated by hand. VP8 payload descriptors are 4 byte each. 5 packets is
  // minimum possible to fit 43 payload bytes into packets with capacity of
  // 15 - 4 = 11 and leave 5 free bytes in the last packet. All packets are
  // almost equal in size, even last packet if counted with free space (which
  // will be filled up the stack by extra long RTP header).
  const size_t kExpectedSizes[] = {13, 13, 14, 14, 9};
  const int kExpectedPart[] = {0, 0, 0, 0, 0};  // Always 0 for equal size mode.
  // Frag start only true for first packet in equal size mode.
  const bool kExpectedFragStart[] = {true, false, false, false, false};
  const size_t kExpectedNum = GTEST_ARRAY_SIZE_(kExpectedSizes);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedPart);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedFragStart);
  ASSERT_EQ(num_packets, kExpectedNum);

  helper_->set_sloppy_partitioning(true);
  helper_->GetAllPacketsAndCheck(&packetizer, kExpectedSizes, kExpectedPart,
                                 kExpectedFragStart, kExpectedNum);
}

// Verify that non-reference bit is set. EqualSize mode fallback is expected.
TEST_F(RtpPacketizerVp8Test, TestNonReferenceBit) {
  const size_t kSizeVector[] = {10, 10, 10};
  const size_t kNumPartitions = GTEST_ARRAY_SIZE_(kSizeVector);
  ASSERT_TRUE(Init(kSizeVector, kNumPartitions));

  hdr_info_.nonReference = true;
  const size_t kMaxPayloadSize = 25;  // Small enough to produce two packets.
  RtpPacketizerVp8 packetizer(hdr_info_, kMaxPayloadSize, 0);
  size_t num_packets = packetizer.SetPayloadData(
      helper_->payload_data(), helper_->payload_size(), nullptr);

  // EqualSize mode => First packet full; other not.
  const size_t kExpectedSizes[] = {16, 16};
  const int kExpectedPart[] = {0, 0};  // Always 0 for equal size mode.
  // Frag start only true for first packet in equal size mode.
  const bool kExpectedFragStart[] = {true, false};
  const size_t kExpectedNum = GTEST_ARRAY_SIZE_(kExpectedSizes);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedPart);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedFragStart);
  ASSERT_EQ(num_packets, kExpectedNum);

  helper_->set_sloppy_partitioning(true);
  helper_->GetAllPacketsAndCheck(&packetizer,
                                 kExpectedSizes,
                                 kExpectedPart,
                                 kExpectedFragStart,
                                 kExpectedNum);
}

// Verify Tl0PicIdx and TID fields, and layerSync bit.
TEST_F(RtpPacketizerVp8Test, TestTl0PicIdxAndTID) {
  const size_t kSizeVector[] = {10, 10, 10};
  const size_t kNumPartitions = GTEST_ARRAY_SIZE_(kSizeVector);
  ASSERT_TRUE(Init(kSizeVector, kNumPartitions));

  hdr_info_.tl0PicIdx = 117;
  hdr_info_.temporalIdx = 2;
  hdr_info_.layerSync = true;
  // kMaxPayloadSize is only limited by allocated buffer size.
  const size_t kMaxPayloadSize = helper_->buffer_size();
  RtpPacketizerVp8 packetizer(hdr_info_, kMaxPayloadSize, 0, kAggregate);
  size_t num_packets = packetizer.SetPayloadData(helper_->payload_data(),
                                                 helper_->payload_size(),
                                                 helper_->fragmentation());

  // Expect one single packet of payload_size() + 4 bytes header.
  const size_t kExpectedSizes[1] = {helper_->payload_size() + 4};
  const int kExpectedPart[1] = {0};  // Packet starts with partition 0.
  const bool kExpectedFragStart[1] = {true};
  const size_t kExpectedNum = GTEST_ARRAY_SIZE_(kExpectedSizes);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedPart);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedFragStart);
  ASSERT_EQ(num_packets, kExpectedNum);

  helper_->GetAllPacketsAndCheck(&packetizer,
                                 kExpectedSizes,
                                 kExpectedPart,
                                 kExpectedFragStart,
                                 kExpectedNum);
}

// Verify KeyIdx field.
TEST_F(RtpPacketizerVp8Test, TestKeyIdx) {
  const size_t kSizeVector[] = {10, 10, 10};
  const size_t kNumPartitions = GTEST_ARRAY_SIZE_(kSizeVector);
  ASSERT_TRUE(Init(kSizeVector, kNumPartitions));

  hdr_info_.keyIdx = 17;
  // kMaxPayloadSize is only limited by allocated buffer size.
  const size_t kMaxPayloadSize = helper_->buffer_size();
  RtpPacketizerVp8 packetizer(hdr_info_, kMaxPayloadSize, 0, kAggregate);
  size_t num_packets = packetizer.SetPayloadData(helper_->payload_data(),
                                                 helper_->payload_size(),
                                                 helper_->fragmentation());

  // Expect one single packet of payload_size() + 3 bytes header.
  const size_t kExpectedSizes[1] = {helper_->payload_size() + 3};
  const int kExpectedPart[1] = {0};  // Packet starts with partition 0.
  const bool kExpectedFragStart[1] = {true};
  const size_t kExpectedNum = GTEST_ARRAY_SIZE_(kExpectedSizes);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedPart);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedFragStart);
  ASSERT_EQ(num_packets, kExpectedNum);

  helper_->GetAllPacketsAndCheck(&packetizer,
                                 kExpectedSizes,
                                 kExpectedPart,
                                 kExpectedFragStart,
                                 kExpectedNum);
}

// Verify TID field and KeyIdx field in combination.
TEST_F(RtpPacketizerVp8Test, TestTIDAndKeyIdx) {
  const size_t kSizeVector[] = {10, 10, 10};
  const size_t kNumPartitions = GTEST_ARRAY_SIZE_(kSizeVector);
  ASSERT_TRUE(Init(kSizeVector, kNumPartitions));

  hdr_info_.temporalIdx = 1;
  hdr_info_.keyIdx = 5;
  // kMaxPayloadSize is only limited by allocated buffer size.
  const size_t kMaxPayloadSize = helper_->buffer_size();
  RtpPacketizerVp8 packetizer(hdr_info_, kMaxPayloadSize, 0, kAggregate);
  size_t num_packets = packetizer.SetPayloadData(helper_->payload_data(),
                                                 helper_->payload_size(),
                                                 helper_->fragmentation());

  // Expect one single packet of payload_size() + 3 bytes header.
  const size_t kExpectedSizes[1] = {helper_->payload_size() + 3};
  const int kExpectedPart[1] = {0};  // Packet starts with partition 0.
  const bool kExpectedFragStart[1] = {true};
  const size_t kExpectedNum = GTEST_ARRAY_SIZE_(kExpectedSizes);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedPart);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedFragStart);
  ASSERT_EQ(num_packets, kExpectedNum);

  helper_->GetAllPacketsAndCheck(&packetizer,
                                 kExpectedSizes,
                                 kExpectedPart,
                                 kExpectedFragStart,
                                 kExpectedNum);
}

class RtpDepacketizerVp8Test : public ::testing::Test {
 protected:
  RtpDepacketizerVp8Test()
      : depacketizer_(RtpDepacketizer::Create(kRtpVideoVp8)) {}

  void ExpectPacket(RtpDepacketizer::ParsedPayload* parsed_payload,
                    const uint8_t* data,
                    size_t length) {
    ASSERT_TRUE(parsed_payload != NULL);
    EXPECT_THAT(std::vector<uint8_t>(
                    parsed_payload->payload,
                    parsed_payload->payload + parsed_payload->payload_length),
                ::testing::ElementsAreArray(data, length));
  }

  std::unique_ptr<RtpDepacketizer> depacketizer_;
};

TEST_F(RtpDepacketizerVp8Test, BasicHeader) {
  const uint8_t kHeaderLength = 1;
  uint8_t packet[4] = {0};
  packet[0] = 0x14;  // Binary 0001 0100; S = 1, PartID = 4.
  packet[1] = 0x01;  // P frame.
  RtpDepacketizer::ParsedPayload payload;

  ASSERT_TRUE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
  ExpectPacket(
      &payload, packet + kHeaderLength, sizeof(packet) - kHeaderLength);

  EXPECT_EQ(kVideoFrameDelta, payload.frame_type);
  EXPECT_EQ(kRtpVideoVp8, payload.type.Video.codec);
  VerifyBasicHeader(&payload.type, 0, 1, 4);
  VerifyExtensions(
      &payload.type, kNoPictureId, kNoTl0PicIdx, kNoTemporalIdx, kNoKeyIdx);
}

TEST_F(RtpDepacketizerVp8Test, PictureID) {
  const uint8_t kHeaderLength1 = 3;
  const uint8_t kHeaderLength2 = 4;
  const uint8_t kPictureId = 17;
  uint8_t packet[10] = {0};
  packet[0] = 0xA0;
  packet[1] = 0x80;
  packet[2] = kPictureId;
  RtpDepacketizer::ParsedPayload payload;

  ASSERT_TRUE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
  ExpectPacket(
      &payload, packet + kHeaderLength1, sizeof(packet) - kHeaderLength1);
  EXPECT_EQ(kVideoFrameDelta, payload.frame_type);
  EXPECT_EQ(kRtpVideoVp8, payload.type.Video.codec);
  VerifyBasicHeader(&payload.type, 1, 0, 0);
  VerifyExtensions(
      &payload.type, kPictureId, kNoTl0PicIdx, kNoTemporalIdx, kNoKeyIdx);

  // Re-use packet, but change to long PictureID.
  packet[2] = 0x80 | kPictureId;
  packet[3] = kPictureId;

  payload = RtpDepacketizer::ParsedPayload();
  ASSERT_TRUE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
  ExpectPacket(
      &payload, packet + kHeaderLength2, sizeof(packet) - kHeaderLength2);
  VerifyBasicHeader(&payload.type, 1, 0, 0);
  VerifyExtensions(&payload.type,
                   (kPictureId << 8) + kPictureId,
                   kNoTl0PicIdx,
                   kNoTemporalIdx,
                   kNoKeyIdx);
}

TEST_F(RtpDepacketizerVp8Test, Tl0PicIdx) {
  const uint8_t kHeaderLength = 3;
  const uint8_t kTl0PicIdx = 17;
  uint8_t packet[13] = {0};
  packet[0] = 0x90;
  packet[1] = 0x40;
  packet[2] = kTl0PicIdx;
  RtpDepacketizer::ParsedPayload payload;

  ASSERT_TRUE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
  ExpectPacket(
      &payload, packet + kHeaderLength, sizeof(packet) - kHeaderLength);
  EXPECT_EQ(kVideoFrameKey, payload.frame_type);
  EXPECT_EQ(kRtpVideoVp8, payload.type.Video.codec);
  VerifyBasicHeader(&payload.type, 0, 1, 0);
  VerifyExtensions(
      &payload.type, kNoPictureId, kTl0PicIdx, kNoTemporalIdx, kNoKeyIdx);
}

TEST_F(RtpDepacketizerVp8Test, TIDAndLayerSync) {
  const uint8_t kHeaderLength = 3;
  uint8_t packet[10] = {0};
  packet[0] = 0x88;
  packet[1] = 0x20;
  packet[2] = 0x80;  // TID(2) + LayerSync(false)
  RtpDepacketizer::ParsedPayload payload;

  ASSERT_TRUE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
  ExpectPacket(
      &payload, packet + kHeaderLength, sizeof(packet) - kHeaderLength);
  EXPECT_EQ(kVideoFrameDelta, payload.frame_type);
  EXPECT_EQ(kRtpVideoVp8, payload.type.Video.codec);
  VerifyBasicHeader(&payload.type, 0, 0, 8);
  VerifyExtensions(&payload.type, kNoPictureId, kNoTl0PicIdx, 2, kNoKeyIdx);
  EXPECT_FALSE(payload.type.Video.codecHeader.VP8.layerSync);
}

TEST_F(RtpDepacketizerVp8Test, KeyIdx) {
  const uint8_t kHeaderLength = 3;
  const uint8_t kKeyIdx = 17;
  uint8_t packet[10] = {0};
  packet[0] = 0x88;
  packet[1] = 0x10;  // K = 1.
  packet[2] = kKeyIdx;
  RtpDepacketizer::ParsedPayload payload;

  ASSERT_TRUE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
  ExpectPacket(
      &payload, packet + kHeaderLength, sizeof(packet) - kHeaderLength);
  EXPECT_EQ(kVideoFrameDelta, payload.frame_type);
  EXPECT_EQ(kRtpVideoVp8, payload.type.Video.codec);
  VerifyBasicHeader(&payload.type, 0, 0, 8);
  VerifyExtensions(
      &payload.type, kNoPictureId, kNoTl0PicIdx, kNoTemporalIdx, kKeyIdx);
}

TEST_F(RtpDepacketizerVp8Test, MultipleExtensions) {
  const uint8_t kHeaderLength = 6;
  uint8_t packet[10] = {0};
  packet[0] = 0x88;
  packet[1] = 0x80 | 0x40 | 0x20 | 0x10;
  packet[2] = 0x80 | 17;           // PictureID, high 7 bits.
  packet[3] = 17;                  // PictureID, low 8 bits.
  packet[4] = 42;                  // Tl0PicIdx.
  packet[5] = 0x40 | 0x20 | 0x11;  // TID(1) + LayerSync(true) + KEYIDX(17).
  RtpDepacketizer::ParsedPayload payload;

  ASSERT_TRUE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
  ExpectPacket(
      &payload, packet + kHeaderLength, sizeof(packet) - kHeaderLength);
  EXPECT_EQ(kVideoFrameDelta, payload.frame_type);
  EXPECT_EQ(kRtpVideoVp8, payload.type.Video.codec);
  VerifyBasicHeader(&payload.type, 0, 0, 8);
  VerifyExtensions(&payload.type, (17 << 8) + 17, 42, 1, 17);
}

TEST_F(RtpDepacketizerVp8Test, TooShortHeader) {
  uint8_t packet[4] = {0};
  packet[0] = 0x88;
  packet[1] = 0x80 | 0x40 | 0x20 | 0x10;  // All extensions are enabled...
  packet[2] = 0x80 | 17;  // ... but only 2 bytes PictureID is provided.
  packet[3] = 17;         // PictureID, low 8 bits.
  RtpDepacketizer::ParsedPayload payload;

  EXPECT_FALSE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
}

TEST_F(RtpDepacketizerVp8Test, TestWithPacketizer) {
  const uint8_t kHeaderLength = 5;
  uint8_t data[10] = {0};
  RtpPacketToSend packet(kNoExtensions);
  RTPVideoHeaderVP8 input_header;
  input_header.nonReference = true;
  input_header.pictureId = 300;
  input_header.temporalIdx = 1;
  input_header.layerSync = false;
  input_header.tl0PicIdx = kNoTl0PicIdx;  // Disable.
  input_header.keyIdx = 31;
  RtpPacketizerVp8 packetizer(input_header, 20, 0);
  EXPECT_EQ(packetizer.SetPayloadData(data, 10, NULL), 1u);
  ASSERT_TRUE(packetizer.NextPacket(&packet));
  EXPECT_TRUE(packet.Marker());

  auto rtp_payload = packet.payload();
  RtpDepacketizer::ParsedPayload payload;
  ASSERT_TRUE(
      depacketizer_->Parse(&payload, rtp_payload.data(), rtp_payload.size()));
  auto vp8_payload = rtp_payload.subview(kHeaderLength);
  ExpectPacket(&payload, vp8_payload.data(), vp8_payload.size());
  EXPECT_EQ(kVideoFrameKey, payload.frame_type);
  EXPECT_EQ(kRtpVideoVp8, payload.type.Video.codec);
  VerifyBasicHeader(&payload.type, 1, 1, 0);
  VerifyExtensions(&payload.type,
                   input_header.pictureId,
                   input_header.tl0PicIdx,
                   input_header.temporalIdx,
                   input_header.keyIdx);
  EXPECT_EQ(payload.type.Video.codecHeader.VP8.layerSync,
            input_header.layerSync);
}

TEST_F(RtpDepacketizerVp8Test, TestEmptyPayload) {
  // Using a wild pointer to crash on accesses from inside the depacketizer.
  uint8_t* garbage_ptr = reinterpret_cast<uint8_t*>(0x4711);
  RtpDepacketizer::ParsedPayload payload;
  EXPECT_FALSE(depacketizer_->Parse(&payload, garbage_ptr, 0));
}
}  // namespace webrtc
