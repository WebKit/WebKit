/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/video_rtp_depacketizer_av1.h"

#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;

// Signals number of the OBU (fragments) in the packet.
constexpr uint8_t kObuCountOne = 0b00'01'0000;

constexpr uint8_t kObuHeaderSequenceHeader = 0b0'0001'000;
constexpr uint8_t kObuHeaderFrame = 0b0'0110'000;

constexpr uint8_t kObuHeaderHasSize = 0b0'0000'010;

TEST(VideoRtpDepacketizerAv1Test, ParsePassFullRtpPayloadAsCodecPayload) {
  const uint8_t packet[] = {(uint8_t{1} << 7) | kObuCountOne, 1, 2, 3, 4};
  rtc::CopyOnWriteBuffer rtp_payload(packet);
  VideoRtpDepacketizerAv1 depacketizer;
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(rtp_payload);
  ASSERT_TRUE(parsed);
  EXPECT_EQ(parsed->video_payload.size(), sizeof(packet));
  EXPECT_TRUE(parsed->video_payload.cdata() == rtp_payload.cdata());
}

TEST(VideoRtpDepacketizerAv1Test,
     ParseTreatsContinuationFlagAsNotBeginningOfFrame) {
  const uint8_t packet[] = {
      (uint8_t{1} << 7) | kObuCountOne,
      kObuHeaderFrame};  // Value doesn't matter since it is a
                         // continuation of the OBU from previous packet.
  VideoRtpDepacketizerAv1 depacketizer;
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(rtc::CopyOnWriteBuffer(packet));
  ASSERT_TRUE(parsed);
  EXPECT_FALSE(parsed->video_header.is_first_packet_in_frame);
}

TEST(VideoRtpDepacketizerAv1Test,
     ParseTreatsNoContinuationFlagAsBeginningOfFrame) {
  const uint8_t packet[] = {(uint8_t{0} << 7) | kObuCountOne, kObuHeaderFrame};
  VideoRtpDepacketizerAv1 depacketizer;
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(rtc::CopyOnWriteBuffer(packet));
  ASSERT_TRUE(parsed);
  EXPECT_TRUE(parsed->video_header.is_first_packet_in_frame);
}

TEST(VideoRtpDepacketizerAv1Test, ParseTreatsWillContinueFlagAsNotEndOfFrame) {
  const uint8_t packet[] = {(uint8_t{1} << 6) | kObuCountOne, kObuHeaderFrame};
  rtc::CopyOnWriteBuffer rtp_payload(packet);
  VideoRtpDepacketizerAv1 depacketizer;
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(rtp_payload);
  ASSERT_TRUE(parsed);
  EXPECT_FALSE(parsed->video_header.is_last_packet_in_frame);
}

TEST(VideoRtpDepacketizerAv1Test, ParseTreatsNoWillContinueFlagAsEndOfFrame) {
  const uint8_t packet[] = {(uint8_t{0} << 6) | kObuCountOne, kObuHeaderFrame};
  VideoRtpDepacketizerAv1 depacketizer;
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(rtc::CopyOnWriteBuffer(packet));
  ASSERT_TRUE(parsed);
  EXPECT_TRUE(parsed->video_header.is_last_packet_in_frame);
}

TEST(VideoRtpDepacketizerAv1Test,
     ParseUsesNewCodedVideoSequenceBitAsKeyFrameIndidcator) {
  const uint8_t packet[] = {(uint8_t{1} << 3) | kObuCountOne,
                            kObuHeaderSequenceHeader};
  VideoRtpDepacketizerAv1 depacketizer;
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(rtc::CopyOnWriteBuffer(packet));
  ASSERT_TRUE(parsed);
  EXPECT_TRUE(parsed->video_header.is_first_packet_in_frame);
  EXPECT_TRUE(parsed->video_header.frame_type ==
              VideoFrameType::kVideoFrameKey);
}

TEST(VideoRtpDepacketizerAv1Test,
     ParseUsesUnsetNewCodedVideoSequenceBitAsDeltaFrameIndidcator) {
  const uint8_t packet[] = {(uint8_t{0} << 3) | kObuCountOne,
                            kObuHeaderSequenceHeader};
  VideoRtpDepacketizerAv1 depacketizer;
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(rtc::CopyOnWriteBuffer(packet));
  ASSERT_TRUE(parsed);
  EXPECT_TRUE(parsed->video_header.is_first_packet_in_frame);
  EXPECT_TRUE(parsed->video_header.frame_type ==
              VideoFrameType::kVideoFrameDelta);
}

TEST(VideoRtpDepacketizerAv1Test,
     ParseRejectsPacketWithNewCVSAndContinuationFlagsBothSet) {
  const uint8_t packet[] = {0b10'00'1000 | kObuCountOne,
                            kObuHeaderSequenceHeader};
  VideoRtpDepacketizerAv1 depacketizer;
  ASSERT_FALSE(depacketizer.Parse(rtc::CopyOnWriteBuffer(packet)));
}

TEST(VideoRtpDepacketizerAv1Test, AssembleFrameSetsOBUPayloadSizeWhenAbsent) {
  const uint8_t payload1[] = {0b00'01'0000,  // aggregation header
                              0b0'0110'000,  // /  Frame
                              20, 30, 40};   // \  OBU
  rtc::ArrayView<const uint8_t> payloads[] = {payload1};
  auto frame = VideoRtpDepacketizerAv1().AssembleFrame(payloads);
  ASSERT_TRUE(frame);
  rtc::ArrayView<const uint8_t> frame_view(*frame);
  EXPECT_TRUE(frame_view[0] & kObuHeaderHasSize);
  EXPECT_EQ(frame_view[1], 3);
}

TEST(VideoRtpDepacketizerAv1Test, AssembleFrameSetsOBUPayloadSizeWhenPresent) {
  const uint8_t payload1[] = {0b00'01'0000,  // aggregation header
                              0b0'0110'010,  // /  Frame OBU header
                              3,             // obu_size
                              20,
                              30,
                              40};  // \  obu_payload
  rtc::ArrayView<const uint8_t> payloads[] = {payload1};
  auto frame = VideoRtpDepacketizerAv1().AssembleFrame(payloads);
  ASSERT_TRUE(frame);
  rtc::ArrayView<const uint8_t> frame_view(*frame);
  EXPECT_TRUE(frame_view[0] & kObuHeaderHasSize);
  EXPECT_EQ(frame_view[1], 3);
}

TEST(VideoRtpDepacketizerAv1Test,
     AssembleFrameSetsOBUPayloadSizeAfterExtensionWhenAbsent) {
  const uint8_t payload1[] = {0b00'01'0000,           // aggregation header
                              0b0'0110'100,           // /  Frame
                              0b010'01'000,           // | extension_header
                              20,           30, 40};  // \  OBU
  rtc::ArrayView<const uint8_t> payloads[] = {payload1};
  auto frame = VideoRtpDepacketizerAv1().AssembleFrame(payloads);
  ASSERT_TRUE(frame);
  rtc::ArrayView<const uint8_t> frame_view(*frame);
  EXPECT_TRUE(frame_view[0] & kObuHeaderHasSize);
  EXPECT_EQ(frame_view[2], 3);
}

TEST(VideoRtpDepacketizerAv1Test,
     AssembleFrameSetsOBUPayloadSizeAfterExtensionWhenPresent) {
  const uint8_t payload1[] = {0b00'01'0000,  // aggregation header
                              0b0'0110'110,  // /  Frame OBU header
                              0b010'01'000,  // | extension_header
                              3,             // | obu_size
                              20,
                              30,
                              40};  // \  obu_payload
  rtc::ArrayView<const uint8_t> payloads[] = {payload1};
  auto frame = VideoRtpDepacketizerAv1().AssembleFrame(payloads);
  ASSERT_TRUE(frame);
  rtc::ArrayView<const uint8_t> frame_view(*frame);
  EXPECT_TRUE(frame_view[0] & kObuHeaderHasSize);
  EXPECT_EQ(frame_view[2], 3);
}

TEST(VideoRtpDepacketizerAv1Test, AssembleFrameFromOnePacketWithOneObu) {
  const uint8_t payload1[] = {0b00'01'0000,  // aggregation header
                              0b0'0110'000,  // /  Frame
                              20};           // \  OBU
  rtc::ArrayView<const uint8_t> payloads[] = {payload1};
  auto frame = VideoRtpDepacketizerAv1().AssembleFrame(payloads);
  ASSERT_TRUE(frame);
  EXPECT_THAT(rtc::ArrayView<const uint8_t>(*frame),
              ElementsAre(0b0'0110'010, 1, 20));
}

TEST(VideoRtpDepacketizerAv1Test, AssembleFrameFromOnePacketWithTwoObus) {
  const uint8_t payload1[] = {0b00'10'0000,  // aggregation header
                              2,             // /  Sequence
                              0b0'0001'000,  // |  Header
                              10,            // \  OBU
                              0b0'0110'000,  // /  Frame
                              20};           // \  OBU
  rtc::ArrayView<const uint8_t> payloads[] = {payload1};
  auto frame = VideoRtpDepacketizerAv1().AssembleFrame(payloads);
  ASSERT_TRUE(frame);
  EXPECT_THAT(rtc::ArrayView<const uint8_t>(*frame),
              ElementsAre(0b0'0001'010, 1, 10,    // Sequence Header OBU
                          0b0'0110'010, 1, 20));  // Frame OBU
}

TEST(VideoRtpDepacketizerAv1Test, AssembleFrameFromTwoPacketsWithOneObu) {
  const uint8_t payload1[] = {0b01'01'0000,  // aggregation header
                              0b0'0110'000, 20, 30};
  const uint8_t payload2[] = {0b10'01'0000,  // aggregation header
                              40};
  rtc::ArrayView<const uint8_t> payloads[] = {payload1, payload2};
  auto frame = VideoRtpDepacketizerAv1().AssembleFrame(payloads);
  ASSERT_TRUE(frame);
  EXPECT_THAT(rtc::ArrayView<const uint8_t>(*frame),
              ElementsAre(0b0'0110'010, 3, 20, 30, 40));
}

TEST(VideoRtpDepacketizerAv1Test, AssembleFrameFromTwoPacketsWithTwoObu) {
  const uint8_t payload1[] = {0b01'10'0000,  // aggregation header
                              2,             // /  Sequence
                              0b0'0001'000,  // |  Header
                              10,            // \  OBU
                              0b0'0110'000,  //
                              20,
                              30};           //
  const uint8_t payload2[] = {0b10'01'0000,  // aggregation header
                              40};           //
  rtc::ArrayView<const uint8_t> payloads[] = {payload1, payload2};
  auto frame = VideoRtpDepacketizerAv1().AssembleFrame(payloads);
  ASSERT_TRUE(frame);
  EXPECT_THAT(rtc::ArrayView<const uint8_t>(*frame),
              ElementsAre(0b0'0001'010, 1, 10,            // SH
                          0b0'0110'010, 3, 20, 30, 40));  // Frame
}

TEST(VideoRtpDepacketizerAv1Test,
     AssembleFrameFromTwoPacketsWithManyObusSomeWithExtensions) {
  const uint8_t payload1[] = {0b01'00'0000,  // aggregation header
                              2,             // /
                              0b0'0001'000,  // |  Sequence Header
                              10,            // \  OBU
                              2,             // /
                              0b0'0101'000,  // |  Metadata OBU
                              20,            // \  without extension
                              4,             // /
                              0b0'0101'100,  // |  Metadata OBU
                              0b001'10'000,  // |  with extension
                              20,            // |
                              30,            // \  metadata payload
                              5,             // /
                              0b0'0110'100,  // |  Frame OBU
                              0b001'10'000,  // |  with extension
                              40,            // |
                              50,            // |
                              60};           // |
  const uint8_t payload2[] = {0b10'01'0000,  // aggregation header
                              70, 80, 90};   // \  tail of the frame OBU

  rtc::ArrayView<const uint8_t> payloads[] = {payload1, payload2};
  auto frame = VideoRtpDepacketizerAv1().AssembleFrame(payloads);
  ASSERT_TRUE(frame);
  EXPECT_THAT(rtc::ArrayView<const uint8_t>(*frame),
              ElementsAre(  // Sequence header OBU
                  0b0'0001'010, 1, 10,
                  // Metadata OBU without extension
                  0b0'0101'010, 1, 20,
                  // Metadata OBU with extenion
                  0b0'0101'110, 0b001'10'000, 2, 20, 30,
                  // Frame OBU with extension
                  0b0'0110'110, 0b001'10'000, 6, 40, 50, 60, 70, 80, 90));
}

TEST(VideoRtpDepacketizerAv1Test, AssembleFrameWithOneObuFromManyPackets) {
  const uint8_t payload1[] = {0b01'01'0000,  // aggregation header
                              0b0'0110'000, 11, 12};
  const uint8_t payload2[] = {0b11'01'0000,  // aggregation header
                              13, 14};
  const uint8_t payload3[] = {0b11'01'0000,  // aggregation header
                              15, 16, 17};
  const uint8_t payload4[] = {0b10'01'0000,  // aggregation header
                              18};

  rtc::ArrayView<const uint8_t> payloads[] = {payload1, payload2, payload3,
                                              payload4};
  auto frame = VideoRtpDepacketizerAv1().AssembleFrame(payloads);
  ASSERT_TRUE(frame);
  EXPECT_THAT(rtc::ArrayView<const uint8_t>(*frame),
              ElementsAre(0b0'0110'010, 8, 11, 12, 13, 14, 15, 16, 17, 18));
}

TEST(VideoRtpDepacketizerAv1Test,
     AssembleFrameFromManyPacketsWithSomeObuBorderAligned) {
  const uint8_t payload1[] = {0b01'10'0000,  // aggregation header
                              3,             // size of the 1st fragment
                              0b0'0011'000,  // Frame header OBU
                              11,
                              12,
                              0b0'0100'000,  // Tile group OBU
                              21,
                              22,
                              23};
  const uint8_t payload2[] = {0b10'01'0000,  // aggregation header
                              24, 25, 26, 27};
  // payload2 ends an OBU, payload3 starts a new one.
  const uint8_t payload3[] = {0b01'10'0000,  // aggregation header
                              3,             // size of the 1st fragment
                              0b0'0111'000,  // Redundant frame header OBU
                              11,
                              12,
                              0b0'0100'000,  // Tile group OBU
                              31,
                              32};
  const uint8_t payload4[] = {0b10'01'0000,  // aggregation header
                              33, 34, 35, 36};
  rtc::ArrayView<const uint8_t> payloads[] = {payload1, payload2, payload3,
                                              payload4};
  auto frame = VideoRtpDepacketizerAv1().AssembleFrame(payloads);
  ASSERT_TRUE(frame);
  EXPECT_THAT(rtc::ArrayView<const uint8_t>(*frame),
              ElementsAre(0b0'0011'010, 2, 11, 12,  // Frame header
                          0b0'0100'010, 7, 21, 22, 23, 24, 25, 26, 27,  //
                          0b0'0111'010, 2, 11, 12,                      //
                          0b0'0100'010, 6, 31, 32, 33, 34, 35, 36));
}

TEST(VideoRtpDepacketizerAv1Test,
     AssembleFrameFromOnePacketsOneObuPayloadSize127Bytes) {
  uint8_t payload1[4 + 127];
  memset(payload1, 0, sizeof(payload1));
  payload1[0] = 0b00'00'0000;  // aggregation header
  payload1[1] = 0x80;          // leb128 encoded size of 128 bytes
  payload1[2] = 0x01;          // in two bytes
  payload1[3] = 0b0'0110'000;  // obu_header with size and extension bits unset.
  payload1[4 + 42] = 0x42;
  rtc::ArrayView<const uint8_t> payloads[] = {payload1};
  auto frame = VideoRtpDepacketizerAv1().AssembleFrame(payloads);
  ASSERT_TRUE(frame);
  EXPECT_EQ(frame->size(), 2 + 127u);
  rtc::ArrayView<const uint8_t> frame_view(*frame);
  EXPECT_EQ(frame_view[0], 0b0'0110'010);  // obu_header with size bit set.
  EXPECT_EQ(frame_view[1], 127);  // obu payload size, 1 byte enough to encode.
  // Check 'random' byte from the payload is at the same 'random' offset.
  EXPECT_EQ(frame_view[2 + 42], 0x42);
}

TEST(VideoRtpDepacketizerAv1Test,
     AssembleFrameFromTwoPacketsOneObuPayloadSize128Bytes) {
  uint8_t payload1[3 + 32];
  memset(payload1, 0, sizeof(payload1));
  payload1[0] = 0b01'00'0000;  // aggregation header
  payload1[1] = 33;            // leb128 encoded size of 33 bytes in one byte
  payload1[2] = 0b0'0110'000;  // obu_header with size and extension bits unset.
  payload1[3 + 10] = 0x10;
  uint8_t payload2[2 + 96];
  memset(payload2, 0, sizeof(payload2));
  payload2[0] = 0b10'00'0000;  // aggregation header
  payload2[1] = 96;            // leb128 encoded size of 96 bytes in one byte
  payload2[2 + 20] = 0x20;

  rtc::ArrayView<const uint8_t> payloads[] = {payload1, payload2};
  auto frame = VideoRtpDepacketizerAv1().AssembleFrame(payloads);
  ASSERT_TRUE(frame);
  EXPECT_EQ(frame->size(), 3 + 128u);
  rtc::ArrayView<const uint8_t> frame_view(*frame);
  EXPECT_EQ(frame_view[0], 0b0'0110'010);  // obu_header with size bit set.
  EXPECT_EQ(frame_view[1], 0x80);          // obu payload size of 128 bytes.
  EXPECT_EQ(frame_view[2], 0x01);          // encoded in two byes
  // Check two 'random' byte from the payload is at the same 'random' offset.
  EXPECT_EQ(frame_view[3 + 10], 0x10);
  EXPECT_EQ(frame_view[3 + 32 + 20], 0x20);
}

TEST(VideoRtpDepacketizerAv1Test,
     AssembleFrameFromAlmostEmptyPacketStartingAnOBU) {
  const uint8_t payload1[] = {0b01'01'0000};
  const uint8_t payload2[] = {0b10'01'0000, 0b0'0110'000, 10, 20, 30};
  rtc::ArrayView<const uint8_t> payloads[] = {payload1, payload2};

  auto frame = VideoRtpDepacketizerAv1().AssembleFrame(payloads);
  ASSERT_TRUE(frame);
  EXPECT_THAT(rtc::ArrayView<const uint8_t>(*frame),
              ElementsAre(0b0'0110'010, 3, 10, 20, 30));
}

TEST(VideoRtpDepacketizerAv1Test,
     AssembleFrameFromAlmostEmptyPacketFinishingAnOBU) {
  const uint8_t payload1[] = {0b01'01'0000, 0b0'0110'000, 10, 20, 30};
  const uint8_t payload2[] = {0b10'01'0000};
  rtc::ArrayView<const uint8_t> payloads[] = {payload1, payload2};

  auto frame = VideoRtpDepacketizerAv1().AssembleFrame(payloads);
  ASSERT_TRUE(frame);
  EXPECT_THAT(rtc::ArrayView<const uint8_t>(*frame),
              ElementsAre(0b0'0110'010, 3, 10, 20, 30));
}

}  // namespace
}  // namespace webrtc
