/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_format_h264.h"

#include <memory>
#include <vector>

#include "absl/algorithm/container.h"
#include "api/array_view.h"
#include "common_video/h264/h264_common.h"
#include "modules/rtp_rtcp/mocks/mock_rtp_rtcp.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
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
constexpr size_t kMaxPayloadSize = 1200;
constexpr size_t kLengthFieldLength = 2;
constexpr RtpPacketizer::PayloadSizeLimits kNoLimits;

enum Nalu {
  kSlice = 1,
  kIdr = 5,
  kSei = 6,
  kSps = 7,
  kPps = 8,
  kStapA = 24,
  kFuA = 28
};

static const size_t kNalHeaderSize = 1;
static const size_t kFuAHeaderSize = 2;

// Bit masks for FU (A and B) indicators.
enum NalDefs { kFBit = 0x80, kNriMask = 0x60, kTypeMask = 0x1F };

// Bit masks for FU (A and B) headers.
enum FuDefs { kSBit = 0x80, kEBit = 0x40, kRBit = 0x20 };

// Creates Buffer that looks like nal unit of given size.
rtc::Buffer GenerateNalUnit(size_t size) {
  RTC_CHECK_GT(size, 0);
  rtc::Buffer buffer(size);
  // Set some valid header.
  buffer[0] = kSlice;
  for (size_t i = 1; i < size; ++i) {
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
  rtc::Buffer frame(absl::c_accumulate(nalu_sizes, 0) +
                    kStartCodeSize * nalu_sizes.size());
  size_t offset = 0;
  for (size_t nalu_size : nalu_sizes) {
    EXPECT_GE(nalu_size, 1u);
    // Insert nalu start code
    frame[offset] = 0;
    frame[offset + 1] = 0;
    frame[offset + 2] = 1;
    // Set some valid header.
    frame[offset + 3] = 1;
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

std::vector<RtpPacketToSend> FetchAllPackets(RtpPacketizerH264* packetizer) {
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

// Tests that should work with both packetization mode 0 and
// packetization mode 1.
class RtpPacketizerH264ModeTest
    : public ::testing::TestWithParam<H264PacketizationMode> {};

TEST_P(RtpPacketizerH264ModeTest, SingleNalu) {
  const uint8_t frame[] = {0, 0, 1, kIdr, 0xFF};

  RtpPacketizerH264 packetizer(frame, kNoLimits, GetParam());
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(1));
  EXPECT_THAT(packets[0].payload(), ElementsAre(kIdr, 0xFF));
}

TEST_P(RtpPacketizerH264ModeTest, SingleNaluTwoPackets) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = kMaxPayloadSize;
  rtc::Buffer nalus[] = {GenerateNalUnit(kMaxPayloadSize),
                         GenerateNalUnit(100)};
  rtc::Buffer frame = CreateFrame(nalus);

  RtpPacketizerH264 packetizer(frame, limits, GetParam());
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(2));
  EXPECT_THAT(packets[0].payload(), ElementsAreArray(nalus[0]));
  EXPECT_THAT(packets[1].payload(), ElementsAreArray(nalus[1]));
}

TEST_P(RtpPacketizerH264ModeTest,
       SingleNaluFirstPacketReductionAppliesOnlyToFirstFragment) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 200;
  limits.first_packet_reduction_len = 5;
  rtc::Buffer nalus[] = {GenerateNalUnit(/*size=*/195),
                         GenerateNalUnit(/*size=*/200),
                         GenerateNalUnit(/*size=*/200)};
  rtc::Buffer frame = CreateFrame(nalus);

  RtpPacketizerH264 packetizer(frame, limits, GetParam());
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(3));
  EXPECT_THAT(packets[0].payload(), ElementsAreArray(nalus[0]));
  EXPECT_THAT(packets[1].payload(), ElementsAreArray(nalus[1]));
  EXPECT_THAT(packets[2].payload(), ElementsAreArray(nalus[2]));
}

TEST_P(RtpPacketizerH264ModeTest,
       SingleNaluLastPacketReductionAppliesOnlyToLastFragment) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 200;
  limits.last_packet_reduction_len = 5;
  rtc::Buffer nalus[] = {GenerateNalUnit(/*size=*/200),
                         GenerateNalUnit(/*size=*/200),
                         GenerateNalUnit(/*size=*/195)};
  rtc::Buffer frame = CreateFrame(nalus);

  RtpPacketizerH264 packetizer(frame, limits, GetParam());
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(3));
  EXPECT_THAT(packets[0].payload(), ElementsAreArray(nalus[0]));
  EXPECT_THAT(packets[1].payload(), ElementsAreArray(nalus[1]));
  EXPECT_THAT(packets[2].payload(), ElementsAreArray(nalus[2]));
}

TEST_P(RtpPacketizerH264ModeTest,
       SingleNaluFirstAndLastPacketReductionSumsForSinglePacket) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 200;
  limits.first_packet_reduction_len = 20;
  limits.last_packet_reduction_len = 30;
  rtc::Buffer frame = CreateFrame({150});

  RtpPacketizerH264 packetizer(frame, limits, GetParam());
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  EXPECT_THAT(packets, SizeIs(1));
}

INSTANTIATE_TEST_SUITE_P(
    PacketMode,
    RtpPacketizerH264ModeTest,
    ::testing::Values(H264PacketizationMode::SingleNalUnit,
                      H264PacketizationMode::NonInterleaved));

// Aggregation tests.
TEST(RtpPacketizerH264Test, StapA) {
  rtc::Buffer nalus[] = {GenerateNalUnit(/*size=*/2),
                         GenerateNalUnit(/*size=*/2),
                         GenerateNalUnit(/*size=*/0x123)};
  rtc::Buffer frame = CreateFrame(nalus);

  RtpPacketizerH264 packetizer(frame, kNoLimits,
                               H264PacketizationMode::NonInterleaved);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(1));
  auto payload = packets[0].payload();
  EXPECT_EQ(payload.size(),
            kNalHeaderSize + 3 * kLengthFieldLength + 2 + 2 + 0x123);

  EXPECT_EQ(payload[0], kStapA);
  payload = payload.subview(kNalHeaderSize);
  // 1st fragment.
  EXPECT_THAT(payload.subview(0, kLengthFieldLength),
              ElementsAre(0, 2));  // Size.
  EXPECT_THAT(payload.subview(kLengthFieldLength, 2),
              ElementsAreArray(nalus[0]));
  payload = payload.subview(kLengthFieldLength + 2);
  // 2nd fragment.
  EXPECT_THAT(payload.subview(0, kLengthFieldLength),
              ElementsAre(0, 2));  // Size.
  EXPECT_THAT(payload.subview(kLengthFieldLength, 2),
              ElementsAreArray(nalus[1]));
  payload = payload.subview(kLengthFieldLength + 2);
  // 3rd fragment.
  EXPECT_THAT(payload.subview(0, kLengthFieldLength),
              ElementsAre(0x1, 0x23));  // Size.
  EXPECT_THAT(payload.subview(kLengthFieldLength), ElementsAreArray(nalus[2]));
}

TEST(RtpPacketizerH264Test, SingleNalUnitModeHasNoStapA) {
  // This is the same setup as for the StapA test.
  rtc::Buffer frame = CreateFrame({2, 2, 0x123});

  RtpPacketizerH264 packetizer(frame, kNoLimits,
                               H264PacketizationMode::SingleNalUnit);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  // The three fragments should be returned as three packets.
  ASSERT_THAT(packets, SizeIs(3));
  EXPECT_EQ(packets[0].payload_size(), 2u);
  EXPECT_EQ(packets[1].payload_size(), 2u);
  EXPECT_EQ(packets[2].payload_size(), 0x123u);
}

TEST(RtpPacketizerH264Test, StapARespectsFirstPacketReduction) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1000;
  limits.first_packet_reduction_len = 100;
  const size_t kFirstFragmentSize =
      limits.max_payload_len - limits.first_packet_reduction_len;
  rtc::Buffer nalus[] = {GenerateNalUnit(/*size=*/kFirstFragmentSize),
                         GenerateNalUnit(/*size=*/2),
                         GenerateNalUnit(/*size=*/2)};
  rtc::Buffer frame = CreateFrame(nalus);

  RtpPacketizerH264 packetizer(frame, limits,
                               H264PacketizationMode::NonInterleaved);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(2));
  // Expect 1st packet is single nalu.
  EXPECT_THAT(packets[0].payload(), ElementsAreArray(nalus[0]));
  // Expect 2nd packet is aggregate of last two fragments.
  EXPECT_THAT(packets[1].payload(),
              ElementsAre(kStapA,                          //
                          0, 2, nalus[1][0], nalus[1][1],  //
                          0, 2, nalus[2][0], nalus[2][1]));
}

TEST(RtpPacketizerH264Test, StapARespectsLastPacketReduction) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1000;
  limits.last_packet_reduction_len = 100;
  const size_t kLastFragmentSize =
      limits.max_payload_len - limits.last_packet_reduction_len;
  rtc::Buffer nalus[] = {GenerateNalUnit(/*size=*/2),
                         GenerateNalUnit(/*size=*/2),
                         GenerateNalUnit(/*size=*/kLastFragmentSize)};
  rtc::Buffer frame = CreateFrame(nalus);

  RtpPacketizerH264 packetizer(frame, limits,
                               H264PacketizationMode::NonInterleaved);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(2));
  // Expect 1st packet is aggregate of 1st two fragments.
  EXPECT_THAT(packets[0].payload(),
              ElementsAre(kStapA,                          //
                          0, 2, nalus[0][0], nalus[0][1],  //
                          0, 2, nalus[1][0], nalus[1][1]));
  // Expect 2nd packet is single nalu.
  EXPECT_THAT(packets[1].payload(), ElementsAreArray(nalus[2]));
}

TEST(RtpPacketizerH264Test, TooSmallForStapAHeaders) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1000;
  const size_t kLastFragmentSize =
      limits.max_payload_len - 3 * kLengthFieldLength - 4;
  rtc::Buffer nalus[] = {GenerateNalUnit(/*size=*/2),
                         GenerateNalUnit(/*size=*/2),
                         GenerateNalUnit(/*size=*/kLastFragmentSize)};
  rtc::Buffer frame = CreateFrame(nalus);

  RtpPacketizerH264 packetizer(frame, limits,
                               H264PacketizationMode::NonInterleaved);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(2));
  // Expect 1st packet is aggregate of 1st two fragments.
  EXPECT_THAT(packets[0].payload(),
              ElementsAre(kStapA,                          //
                          0, 2, nalus[0][0], nalus[0][1],  //
                          0, 2, nalus[1][0], nalus[1][1]));
  // Expect 2nd packet is single nalu.
  EXPECT_THAT(packets[1].payload(), ElementsAreArray(nalus[2]));
}

// Fragmentation + aggregation.
TEST(RtpPacketizerH264Test, MixedStapAFUA) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 100;
  const size_t kFuaPayloadSize = 70;
  const size_t kFuaNaluSize = kNalHeaderSize + 2 * kFuaPayloadSize;
  const size_t kStapANaluSize = 20;
  rtc::Buffer nalus[] = {GenerateNalUnit(kFuaNaluSize),
                         GenerateNalUnit(kStapANaluSize),
                         GenerateNalUnit(kStapANaluSize)};
  rtc::Buffer frame = CreateFrame(nalus);

  RtpPacketizerH264 packetizer(frame, limits,
                               H264PacketizationMode::NonInterleaved);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(3));
  // First expect two FU-A packets.
  EXPECT_THAT(packets[0].payload().subview(0, kFuAHeaderSize),
              ElementsAre(kFuA, FuDefs::kSBit | nalus[0][0]));
  EXPECT_THAT(
      packets[0].payload().subview(kFuAHeaderSize),
      ElementsAreArray(nalus[0].data() + kNalHeaderSize, kFuaPayloadSize));

  EXPECT_THAT(packets[1].payload().subview(0, kFuAHeaderSize),
              ElementsAre(kFuA, FuDefs::kEBit | nalus[0][0]));
  EXPECT_THAT(
      packets[1].payload().subview(kFuAHeaderSize),
      ElementsAreArray(nalus[0].data() + kNalHeaderSize + kFuaPayloadSize,
                       kFuaPayloadSize));

  // Then expect one STAP-A packet with two nal units.
  EXPECT_THAT(packets[2].payload()[0], kStapA);
  auto payload = packets[2].payload().subview(kNalHeaderSize);
  EXPECT_THAT(payload.subview(0, kLengthFieldLength),
              ElementsAre(0, kStapANaluSize));
  EXPECT_THAT(payload.subview(kLengthFieldLength, kStapANaluSize),
              ElementsAreArray(nalus[1]));
  payload = payload.subview(kLengthFieldLength + kStapANaluSize);
  EXPECT_THAT(payload.subview(0, kLengthFieldLength),
              ElementsAre(0, kStapANaluSize));
  EXPECT_THAT(payload.subview(kLengthFieldLength), ElementsAreArray(nalus[2]));
}

TEST(RtpPacketizerH264Test, LastFragmentFitsInSingleButNotLastPacket) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1178;
  limits.first_packet_reduction_len = 0;
  limits.last_packet_reduction_len = 20;
  limits.single_packet_reduction_len = 20;
  // Actual sizes, which triggered this bug.
  rtc::Buffer frame = CreateFrame({20, 8, 18, 1161});

  RtpPacketizerH264 packetizer(frame, limits,
                               H264PacketizationMode::NonInterleaved);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  // Last packet has to be of correct size.
  // Incorrect implementation might miss this constraint and not split the last
  // fragment in two packets.
  EXPECT_LE(static_cast<int>(packets.back().payload_size()),
            limits.max_payload_len - limits.last_packet_reduction_len);
}

// Splits frame with payload size `frame_payload_size` without fragmentation,
// Returns sizes of the payloads excluding fua headers.
std::vector<int> TestFua(size_t frame_payload_size,
                         const RtpPacketizer::PayloadSizeLimits& limits) {
  rtc::Buffer nalu[] = {GenerateNalUnit(kNalHeaderSize + frame_payload_size)};
  rtc::Buffer frame = CreateFrame(nalu);

  RtpPacketizerH264 packetizer(frame, limits,
                               H264PacketizationMode::NonInterleaved);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  EXPECT_GE(packets.size(), 2u);  // Single packet indicates it is not FuA.
  std::vector<uint16_t> fua_header;
  std::vector<int> payload_sizes;

  for (const RtpPacketToSend& packet : packets) {
    auto payload = packet.payload();
    EXPECT_GT(payload.size(), kFuAHeaderSize);
    fua_header.push_back((payload[0] << 8) | payload[1]);
    payload_sizes.push_back(payload.size() - kFuAHeaderSize);
  }

  EXPECT_TRUE(fua_header.front() & FuDefs::kSBit);
  EXPECT_TRUE(fua_header.back() & FuDefs::kEBit);
  // Clear S and E bits before testing all are duplicating same original header.
  fua_header.front() &= ~FuDefs::kSBit;
  fua_header.back() &= ~FuDefs::kEBit;
  EXPECT_THAT(fua_header, Each(Eq((kFuA << 8) | nalu[0][0])));

  return payload_sizes;
}

// Fragmentation tests.
TEST(RtpPacketizerH264Test, FUAOddSize) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1200;
  EXPECT_THAT(TestFua(1200, limits), ElementsAre(600, 600));
}

TEST(RtpPacketizerH264Test, FUAWithFirstPacketReduction) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1200;
  limits.first_packet_reduction_len = 4;
  limits.single_packet_reduction_len = 4;
  EXPECT_THAT(TestFua(1198, limits), ElementsAre(597, 601));
}

TEST(RtpPacketizerH264Test, FUAWithLastPacketReduction) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1200;
  limits.last_packet_reduction_len = 4;
  limits.single_packet_reduction_len = 4;
  EXPECT_THAT(TestFua(1198, limits), ElementsAre(601, 597));
}

TEST(RtpPacketizerH264Test, FUAWithSinglePacketReduction) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1199;
  limits.single_packet_reduction_len = 200;
  EXPECT_THAT(TestFua(1000, limits), ElementsAre(500, 500));
}

TEST(RtpPacketizerH264Test, FUAEvenSize) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1200;
  EXPECT_THAT(TestFua(1201, limits), ElementsAre(600, 601));
}

TEST(RtpPacketizerH264Test, FUARounding) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1448;
  EXPECT_THAT(TestFua(10123, limits),
              ElementsAre(1265, 1265, 1265, 1265, 1265, 1266, 1266, 1266));
}

TEST(RtpPacketizerH264Test, FUABig) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1200;
  // Generate 10 full sized packets, leave room for FU-A headers.
  EXPECT_THAT(
      TestFua(10 * (1200 - kFuAHeaderSize), limits),
      ElementsAre(1198, 1198, 1198, 1198, 1198, 1198, 1198, 1198, 1198, 1198));
}

TEST(RtpPacketizerH264Test, RejectsOverlongDataInPacketizationMode0) {
  RtpPacketizer::PayloadSizeLimits limits;
  rtc::Buffer frame = CreateFrame({kMaxPayloadSize + 1});

  RtpPacketizerH264 packetizer(frame, limits,
                               H264PacketizationMode::SingleNalUnit);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  EXPECT_THAT(packets, IsEmpty());
}
}  // namespace
}  // namespace webrtc
