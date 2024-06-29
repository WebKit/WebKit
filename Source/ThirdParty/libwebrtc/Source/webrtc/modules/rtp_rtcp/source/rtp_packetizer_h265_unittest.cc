/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_packetizer_h265.h"

#include <vector>

#include "common_video/h265/h265_common.h"
#include "modules/rtp_rtcp/mocks/mock_rtp_rtcp.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtp_packet_h265_common.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::SizeIs;

constexpr RtpPacketToSend::ExtensionManager* kNoExtensions = nullptr;
constexpr size_t kMaxPayloadSizeBytes = 1200;
constexpr size_t kH265LengthFieldSizeBytes = 2;
constexpr RtpPacketizer::PayloadSizeLimits kNoLimits;

constexpr size_t kFuHeaderSizeBytes =
    kH265FuHeaderSizeBytes + kH265PayloadHeaderSizeBytes;

// Creates Buffer that looks like nal unit of given size.
rtc::Buffer GenerateNalUnit(size_t size) {
  RTC_CHECK_GT(size, 0);
  rtc::Buffer buffer(size);
  // Set some valid header with type TRAIL_R and temporal id
  buffer[0] = 2;
  buffer[1] = 2;
  for (size_t i = 2; i < size; ++i) {
    buffer[i] = static_cast<uint8_t>(i);
  }
  // Last byte shouldn't be 0, or it may be counted as part of next 4-byte start
  // sequence.
  buffer[size - 1] |= 0x10;
  return buffer;
}

// Create frame consisting of nalus of given size.
rtc::Buffer CreateFrame(std::initializer_list<size_t> nalu_sizes) {
  static constexpr int kStartCodeSize = 3;
  rtc::Buffer frame(absl::c_accumulate(nalu_sizes, size_t{0}) +
                    kStartCodeSize * nalu_sizes.size());
  size_t offset = 0;
  for (size_t nalu_size : nalu_sizes) {
    EXPECT_GE(nalu_size, 1u);
    // Insert nalu start code
    frame[offset] = 0;
    frame[offset + 1] = 0;
    frame[offset + 2] = 1;
    // Set some valid header.
    frame[offset + 3] = 2;
    // Fill payload avoiding accidental start codes
    if (nalu_size > 1) {
      memset(frame.data() + offset + 4, 0x3f, nalu_size - 1);
    }
    offset += (kStartCodeSize + nalu_size);
  }
  return frame;
}

// Create frame consisting of given nalus.
rtc::Buffer CreateFrame(rtc::ArrayView<const rtc::Buffer> nalus) {
  static constexpr int kStartCodeSize = 3;
  int frame_size = 0;
  for (const rtc::Buffer& nalu : nalus) {
    frame_size += (kStartCodeSize + nalu.size());
  }
  rtc::Buffer frame(frame_size);
  size_t offset = 0;
  for (const rtc::Buffer& nalu : nalus) {
    // Insert nalu start code
    frame[offset] = 0;
    frame[offset + 1] = 0;
    frame[offset + 2] = 1;
    // Copy the nalu unit.
    memcpy(frame.data() + offset + 3, nalu.data(), nalu.size());
    offset += (kStartCodeSize + nalu.size());
  }
  return frame;
}

std::vector<RtpPacketToSend> FetchAllPackets(RtpPacketizerH265* packetizer) {
  std::vector<RtpPacketToSend> result;
  size_t num_packets = packetizer->NumPackets();
  result.reserve(num_packets);
  RtpPacketToSend packet(kNoExtensions);
  while (packetizer->NextPacket(&packet)) {
    result.push_back(packet);
  }
  EXPECT_THAT(result, SizeIs(num_packets));
  return result;
}

// Single nalu tests.
TEST(RtpPacketizerH265Test, SingleNalu) {
  const uint8_t frame[] = {0, 0, 1, H265::kIdrWRadl, 0xFF};

  RtpPacketizerH265 packetizer(frame, kNoLimits);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(1));
  EXPECT_THAT(packets[0].payload(), ElementsAre(H265::kIdrWRadl, 0xFF));
}

TEST(RtpPacketizerH265Test, SingleNaluTwoPackets) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = kMaxPayloadSizeBytes;
  rtc::Buffer nalus[] = {GenerateNalUnit(kMaxPayloadSizeBytes),
                         GenerateNalUnit(100)};
  rtc::Buffer frame = CreateFrame(nalus);

  RtpPacketizerH265 packetizer(frame, limits);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(2));
  EXPECT_THAT(packets[0].payload(), ElementsAreArray(nalus[0]));
  EXPECT_THAT(packets[1].payload(), ElementsAreArray(nalus[1]));
}

TEST(RtpPacketizerH265Test,
     SingleNaluFirstPacketReductionAppliesOnlyToFirstFragment) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 200;
  limits.first_packet_reduction_len = 5;
  rtc::Buffer nalus[] = {GenerateNalUnit(/*size=*/195),
                         GenerateNalUnit(/*size=*/200),
                         GenerateNalUnit(/*size=*/200)};
  rtc::Buffer frame = CreateFrame(nalus);

  RtpPacketizerH265 packetizer(frame, limits);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(3));
  EXPECT_THAT(packets[0].payload(), ElementsAreArray(nalus[0]));
  EXPECT_THAT(packets[1].payload(), ElementsAreArray(nalus[1]));
  EXPECT_THAT(packets[2].payload(), ElementsAreArray(nalus[2]));
}

TEST(RtpPacketizerH265Test,
     SingleNaluLastPacketReductionAppliesOnlyToLastFragment) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 200;
  limits.last_packet_reduction_len = 5;
  rtc::Buffer nalus[] = {GenerateNalUnit(/*size=*/200),
                         GenerateNalUnit(/*size=*/200),
                         GenerateNalUnit(/*size=*/195)};
  rtc::Buffer frame = CreateFrame(nalus);

  RtpPacketizerH265 packetizer(frame, limits);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(3));
  EXPECT_THAT(packets[0].payload(), ElementsAreArray(nalus[0]));
  EXPECT_THAT(packets[1].payload(), ElementsAreArray(nalus[1]));
  EXPECT_THAT(packets[2].payload(), ElementsAreArray(nalus[2]));
}

TEST(RtpPacketizerH265Test,
     SingleNaluFirstAndLastPacketReductionSumsForSinglePacket) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 200;
  limits.first_packet_reduction_len = 20;
  limits.last_packet_reduction_len = 30;
  rtc::Buffer frame = CreateFrame({150});

  RtpPacketizerH265 packetizer(frame, limits);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  EXPECT_THAT(packets, SizeIs(1));
}

// Aggregation tests.
TEST(RtpPacketizerH265Test, ApRespectsNoPacketReduction) {
  rtc::Buffer nalus[] = {GenerateNalUnit(/*size=*/2),
                         GenerateNalUnit(/*size=*/2),
                         GenerateNalUnit(/*size=*/0x123)};
  rtc::Buffer frame = CreateFrame(nalus);

  RtpPacketizerH265 packetizer(frame, kNoLimits);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(1));
  auto payload = packets[0].payload();
  int type = H265::ParseNaluType(payload[0]);
  EXPECT_EQ(payload.size(), kH265NalHeaderSizeBytes +
                                3 * kH265LengthFieldSizeBytes + 2 + 2 + 0x123);

  EXPECT_EQ(type, H265::NaluType::kAp);
  payload = payload.subview(kH265NalHeaderSizeBytes);
  // 1st fragment.
  EXPECT_THAT(payload.subview(0, kH265LengthFieldSizeBytes),
              ElementsAre(0, 2));  // Size.
  EXPECT_THAT(payload.subview(kH265LengthFieldSizeBytes, 2),
              ElementsAreArray(nalus[0]));
  payload = payload.subview(kH265LengthFieldSizeBytes + 2);
  // 2nd fragment.
  EXPECT_THAT(payload.subview(0, kH265LengthFieldSizeBytes),
              ElementsAre(0, 2));  // Size.
  EXPECT_THAT(payload.subview(kH265LengthFieldSizeBytes, 2),
              ElementsAreArray(nalus[1]));
  payload = payload.subview(kH265LengthFieldSizeBytes + 2);
  // 3rd fragment.
  EXPECT_THAT(payload.subview(0, kH265LengthFieldSizeBytes),
              ElementsAre(0x1, 0x23));  // Size.
  EXPECT_THAT(payload.subview(kH265LengthFieldSizeBytes),
              ElementsAreArray(nalus[2]));
}

TEST(RtpPacketizerH265Test, ApRespectsFirstPacketReduction) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1000;
  limits.first_packet_reduction_len = 100;
  const size_t kFirstFragmentSize =
      limits.max_payload_len - limits.first_packet_reduction_len;
  rtc::Buffer nalus[] = {GenerateNalUnit(/*size=*/kFirstFragmentSize),
                         GenerateNalUnit(/*size=*/2),
                         GenerateNalUnit(/*size=*/2)};
  rtc::Buffer frame = CreateFrame(nalus);

  RtpPacketizerH265 packetizer(frame, limits);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(2));
  // Expect 1st packet is single nalu.
  EXPECT_THAT(packets[0].payload(), ElementsAreArray(nalus[0]));
  // Expect 2nd packet is aggregate of last two fragments.
  // The size of H265 nal_unit_header is 2 bytes, according to 7.3.1.2
  // in H265 spec. Aggregation packet type is 48, and nuh_temporal_id_plus1
  // is 2, so the nal_unit_header should be "01100000 00000010",
  // which is 96 and 2.
  EXPECT_THAT(packets[1].payload(),
              ElementsAre(96, 2,                           //
                          0, 2, nalus[1][0], nalus[1][1],  //
                          0, 2, nalus[2][0], nalus[2][1]));
}

TEST(RtpPacketizerH265Test, ApRespectsLastPacketReduction) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1000;
  limits.last_packet_reduction_len = 100;
  const size_t kLastFragmentSize =
      limits.max_payload_len - limits.last_packet_reduction_len;
  rtc::Buffer nalus[] = {GenerateNalUnit(/*size=*/2),
                         GenerateNalUnit(/*size=*/2),
                         GenerateNalUnit(/*size=*/kLastFragmentSize)};
  rtc::Buffer frame = CreateFrame(nalus);

  RtpPacketizerH265 packetizer(frame, limits);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(2));
  // Expect 1st packet is aggregate of 1st two fragments.
  EXPECT_THAT(packets[0].payload(),
              ElementsAre(96, 2,                           //
                          0, 2, nalus[0][0], nalus[0][1],  //
                          0, 2, nalus[1][0], nalus[1][1]));
  // Expect 2nd packet is single nalu.
  EXPECT_THAT(packets[1].payload(), ElementsAreArray(nalus[2]));
}

TEST(RtpPacketizerH265Test, TooSmallForApHeaders) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1000;
  const size_t kLastFragmentSize =
      limits.max_payload_len - 3 * kH265LengthFieldSizeBytes - 4;
  rtc::Buffer nalus[] = {GenerateNalUnit(/*size=*/2),
                         GenerateNalUnit(/*size=*/2),
                         GenerateNalUnit(/*size=*/kLastFragmentSize)};
  rtc::Buffer frame = CreateFrame(nalus);

  RtpPacketizerH265 packetizer(frame, limits);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(2));
  // Expect 1st packet is aggregate of 1st two fragments.
  EXPECT_THAT(packets[0].payload(),
              ElementsAre(96, 2,                           //
                          0, 2, nalus[0][0], nalus[0][1],  //
                          0, 2, nalus[1][0], nalus[1][1]));
  // Expect 2nd packet is single nalu.
  EXPECT_THAT(packets[1].payload(), ElementsAreArray(nalus[2]));
}

TEST(RtpPacketizerH265Test, LastFragmentFitsInSingleButNotLastPacket) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1178;
  limits.first_packet_reduction_len = 0;
  limits.last_packet_reduction_len = 20;
  limits.single_packet_reduction_len = 20;
  // Actual sizes, which triggered this bug.
  rtc::Buffer frame = CreateFrame({20, 8, 18, 1161});

  RtpPacketizerH265 packetizer(frame, limits);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  // Last packet has to be of correct size.
  // Incorrect implementation might miss this constraint and not split the last
  // fragment in two packets.
  EXPECT_LE(static_cast<int>(packets.back().payload_size()),
            limits.max_payload_len - limits.last_packet_reduction_len);
}

// Splits frame with payload size `frame_payload_size` without fragmentation,
// Returns sizes of the payloads excluding FU headers.
std::vector<int> TestFu(size_t frame_payload_size,
                        const RtpPacketizer::PayloadSizeLimits& limits) {
  rtc::Buffer nalu[] = {
      GenerateNalUnit(kH265NalHeaderSizeBytes + frame_payload_size)};
  rtc::Buffer frame = CreateFrame(nalu);

  RtpPacketizerH265 packetizer(frame, limits);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  EXPECT_GE(packets.size(), 2u);  // Single packet indicates it is not FU.
  std::vector<uint16_t> fu_header;
  std::vector<int> payload_sizes;

  for (const RtpPacketToSend& packet : packets) {
    auto payload = packet.payload();
    EXPECT_GT(payload.size(), kFuHeaderSizeBytes);
    // FU header is after the 2-bytes size PayloadHdr according to 4.4.3 in spec
    fu_header.push_back(payload[2]);
    payload_sizes.push_back(payload.size() - kFuHeaderSizeBytes);
  }

  EXPECT_TRUE(fu_header.front() & kH265SBitMask);
  EXPECT_TRUE(fu_header.back() & kH265EBitMask);
  // Clear S and E bits before testing all are duplicating same original header.
  fu_header.front() &= ~kH265SBitMask;
  fu_header.back() &= ~kH265EBitMask;
  uint8_t nalu_type = (nalu[0][0] & kH265TypeMask) >> 1;
  EXPECT_THAT(fu_header, Each(Eq(nalu_type)));

  return payload_sizes;
}

// Fragmentation tests.
TEST(RtpPacketizerH265Test, FuOddSize) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1200;
  EXPECT_THAT(TestFu(1200, limits), ElementsAre(600, 600));
}

TEST(RtpPacketizerH265Test, FuWithFirstPacketReduction) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1200;
  limits.first_packet_reduction_len = 4;
  limits.single_packet_reduction_len = 4;
  EXPECT_THAT(TestFu(1198, limits), ElementsAre(597, 601));
}

TEST(RtpPacketizerH265Test, FuWithLastPacketReduction) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1200;
  limits.last_packet_reduction_len = 4;
  limits.single_packet_reduction_len = 4;
  EXPECT_THAT(TestFu(1198, limits), ElementsAre(601, 597));
}

TEST(RtpPacketizerH265Test, FuWithSinglePacketReduction) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1199;
  limits.single_packet_reduction_len = 200;
  EXPECT_THAT(TestFu(1000, limits), ElementsAre(500, 500));
}

TEST(RtpPacketizerH265Test, FuEvenSize) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1200;
  EXPECT_THAT(TestFu(1201, limits), ElementsAre(600, 601));
}

TEST(RtpPacketizerH265Test, FuRounding) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1448;
  EXPECT_THAT(TestFu(10123, limits),
              ElementsAre(1265, 1265, 1265, 1265, 1265, 1266, 1266, 1266));
}

TEST(RtpPacketizerH265Test, FuBig) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1200;
  // Generate 10 full sized packets, leave room for FU headers.
  EXPECT_THAT(
      TestFu(10 * (1200 - kFuHeaderSizeBytes), limits),
      ElementsAre(1197, 1197, 1197, 1197, 1197, 1197, 1197, 1197, 1197, 1197));
}

struct PacketInfo {
  bool first_fragment = false;
  bool last_fragment = false;
  bool aggregated = false;
  int nalu_index = 0;
  int nalu_number = 0;
  int payload_size = 0;
  int start_offset = 0;
};

struct MixedApFuTestParams {
  std::vector<int> nalus;
  int expect_packetsSize = 0;
  std::vector<PacketInfo> expected_packets;
};

class RtpPacketizerH265ParametrizedTest
    : public ::testing::TestWithParam<MixedApFuTestParams> {};

// Fragmentation + aggregation mixed testing.
TEST_P(RtpPacketizerH265ParametrizedTest, MixedApFu) {
  RtpPacketizer::PayloadSizeLimits limits;
  const MixedApFuTestParams params = GetParam();
  limits.max_payload_len = 100;
  std::vector<rtc::Buffer> nalus;
  nalus.reserve(params.nalus.size());

  // Generate nalus according to size specified in paramters
  for (size_t index = 0; index < params.nalus.size(); index++) {
    nalus.push_back(GenerateNalUnit(params.nalus[index]));
  }
  rtc::Buffer frame = CreateFrame(nalus);

  RtpPacketizerH265 packetizer(frame, limits);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(params.expect_packetsSize));
  for (int i = 0; i < params.expect_packetsSize; i++) {
    PacketInfo expected_packet = params.expected_packets[i];
    if (expected_packet.aggregated) {
      int type = H265::ParseNaluType(packets[i].payload()[0]);
      EXPECT_THAT(type, H265::NaluType::kAp);
      auto payload = packets[i].payload().subview(kH265NalHeaderSizeBytes);
      int offset = 0;
      // Generated AP packet header and payload align
      for (int j = expected_packet.nalu_index; j < expected_packet.nalu_number;
           j++) {
        EXPECT_THAT(payload.subview(0, kH265LengthFieldSizeBytes),
                    ElementsAre(0, nalus[j].size()));
        EXPECT_THAT(payload.subview(offset + kH265LengthFieldSizeBytes,
                                    nalus[j].size()),
                    ElementsAreArray(nalus[j]));
        offset += kH265LengthFieldSizeBytes + nalus[j].size();
      }
    } else {
      uint8_t fu_header = 0;
      fu_header |= (expected_packet.first_fragment ? kH265SBitMask : 0);
      fu_header |= (expected_packet.last_fragment ? kH265EBitMask : 0);
      fu_header |= H265::NaluType::kTrailR;
      EXPECT_THAT(packets[i].payload().subview(0, kFuHeaderSizeBytes),
                  ElementsAre(98, 2, fu_header));
      EXPECT_THAT(packets[i].payload().subview(kFuHeaderSizeBytes),
                  ElementsAreArray(nalus[expected_packet.nalu_index].data() +
                                       kH265NalHeaderSizeBytes +
                                       expected_packet.start_offset,
                                   expected_packet.payload_size));
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    RtpPacketizerH265Test,
    RtpPacketizerH265ParametrizedTest,
    testing::Values(
        // FU + AP + FU.
        // GenerateNalUnit will include 2 bytes nalu header, for FU packet split
        // calculation, this 2-byte nalu header length should be excluded.
        MixedApFuTestParams{.nalus = {140, 20, 20, 160},
                            .expect_packetsSize = 5,
                            .expected_packets = {{.first_fragment = true,
                                                  .nalu_index = 0,
                                                  .payload_size = 69,
                                                  .start_offset = 0},
                                                 {.last_fragment = true,
                                                  .nalu_index = 0,
                                                  .payload_size = 69,
                                                  .start_offset = 69},
                                                 {.aggregated = true,
                                                  .nalu_index = 1,
                                                  .nalu_number = 2},
                                                 {.first_fragment = true,
                                                  .nalu_index = 3,
                                                  .payload_size = 79,
                                                  .start_offset = 0},
                                                 {.last_fragment = true,
                                                  .nalu_index = 3,
                                                  .payload_size = 79,
                                                  .start_offset = 79}}},
        // AP + FU + AP
        MixedApFuTestParams{
            .nalus = {20, 20, 160, 30, 30},
            .expect_packetsSize = 4,
            .expected_packets = {
                {.aggregated = true, .nalu_index = 0, .nalu_number = 2},
                {.first_fragment = true,
                 .nalu_index = 2,
                 .payload_size = 79,
                 .start_offset = 0},
                {.last_fragment = true,
                 .nalu_index = 2,
                 .payload_size = 79,
                 .start_offset = 79},
                {.aggregated = true, .nalu_index = 3, .nalu_number = 2}}}));

}  // namespace
}  // namespace webrtc
