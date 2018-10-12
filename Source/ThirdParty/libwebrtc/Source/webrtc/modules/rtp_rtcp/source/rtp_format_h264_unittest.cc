/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <vector>

#include "api/array_view.h"
#include "common_video/h264/h264_common.h"
#include "modules/include/module_common_types.h"
#include "modules/rtp_rtcp/mocks/mock_rtp_rtcp.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtp_format_h264.h"
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

RTPFragmentationHeader CreateFragmentation(rtc::ArrayView<const size_t> sizes) {
  RTPFragmentationHeader fragmentation;
  fragmentation.VerifyAndAllocateFragmentationHeader(sizes.size());
  size_t offset = 0;
  for (size_t i = 0; i < sizes.size(); ++i) {
    fragmentation.fragmentationOffset[i] = offset;
    fragmentation.fragmentationLength[i] = sizes[i];
    offset += sizes[i];
  }
  return fragmentation;
}

// Create fragmentation with single fragment of same size as |frame|
RTPFragmentationHeader NoFragmentation(rtc::ArrayView<const uint8_t> frame) {
  size_t frame_size[] = {frame.size()};
  return CreateFragmentation(frame_size);
}

// Create frame of given size.
rtc::Buffer CreateFrame(size_t frame_size) {
  rtc::Buffer frame(frame_size);
  // Set some valid header.
  frame[0] = 0x01;
  // Generate payload to detect when shifted payload was put into a packet.
  for (size_t i = 1; i < frame_size; ++i)
    frame[i] = static_cast<uint8_t>(i);
  return frame;
}

// Create frame with size deduced from fragmentation.
rtc::Buffer CreateFrame(const RTPFragmentationHeader& fragmentation) {
  size_t last_frame_index = fragmentation.fragmentationVectorSize - 1;
  size_t frame_size = fragmentation.fragmentationOffset[last_frame_index] +
                      fragmentation.fragmentationLength[last_frame_index];
  rtc::Buffer frame = CreateFrame(frame_size);
  // Set some headers.
  // Tests can expect those are valid but shouln't rely on actual values.
  for (size_t i = 0; i <= last_frame_index; ++i) {
    frame[fragmentation.fragmentationOffset[i]] = i + 1;
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
  const uint8_t frame[2] = {kIdr, 0xFF};

  RtpPacketizerH264 packetizer(frame, kNoLimits, GetParam(),
                               NoFragmentation(frame));
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(1));
  EXPECT_THAT(packets[0].payload(), ElementsAreArray(frame));
}

TEST_P(RtpPacketizerH264ModeTest, SingleNaluTwoPackets) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = kMaxPayloadSize;
  const size_t fragment_sizes[] = {kMaxPayloadSize, 100};
  RTPFragmentationHeader fragmentation = CreateFragmentation(fragment_sizes);
  rtc::Buffer frame = CreateFrame(fragmentation);

  RtpPacketizerH264 packetizer(frame, limits, GetParam(), fragmentation);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(2));
  EXPECT_THAT(packets[0].payload(),
              ElementsAreArray(frame.data(), kMaxPayloadSize));
  EXPECT_THAT(packets[1].payload(),
              ElementsAreArray(frame.data() + kMaxPayloadSize, 100));
}

TEST_P(RtpPacketizerH264ModeTest,
       SingleNaluFirstPacketReductionAppliesOnlyToFirstFragment) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 200;
  limits.first_packet_reduction_len = 5;
  const size_t fragments[] = {195, 200, 200};

  RTPFragmentationHeader fragmentation = CreateFragmentation(fragments);
  rtc::Buffer frame = CreateFrame(fragmentation);

  RtpPacketizerH264 packetizer(frame, limits, GetParam(), fragmentation);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(3));
  const uint8_t* next_fragment = frame.data();
  EXPECT_THAT(packets[0].payload(), ElementsAreArray(next_fragment, 195));
  next_fragment += 195;
  EXPECT_THAT(packets[1].payload(), ElementsAreArray(next_fragment, 200));
  next_fragment += 200;
  EXPECT_THAT(packets[2].payload(), ElementsAreArray(next_fragment, 200));
}

TEST_P(RtpPacketizerH264ModeTest,
       SingleNaluLastPacketReductionAppliesOnlyToLastFragment) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 200;
  limits.last_packet_reduction_len = 5;
  const size_t fragments[] = {200, 200, 195};

  RTPFragmentationHeader fragmentation = CreateFragmentation(fragments);
  rtc::Buffer frame = CreateFrame(fragmentation);

  RtpPacketizerH264 packetizer(frame, limits, GetParam(), fragmentation);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(3));
  const uint8_t* next_fragment = frame.data();
  EXPECT_THAT(packets[0].payload(), ElementsAreArray(next_fragment, 200));
  next_fragment += 200;
  EXPECT_THAT(packets[1].payload(), ElementsAreArray(next_fragment, 200));
  next_fragment += 200;
  EXPECT_THAT(packets[2].payload(), ElementsAreArray(next_fragment, 195));
}

TEST_P(RtpPacketizerH264ModeTest,
       SingleNaluFirstAndLastPacketReductionSumsForSinglePacket) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 200;
  limits.first_packet_reduction_len = 20;
  limits.last_packet_reduction_len = 30;
  rtc::Buffer frame = CreateFrame(150);

  RtpPacketizerH264 packetizer(frame, limits, GetParam(),
                               NoFragmentation(frame));
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  EXPECT_THAT(packets, SizeIs(1));
}

INSTANTIATE_TEST_CASE_P(
    PacketMode,
    RtpPacketizerH264ModeTest,
    ::testing::Values(H264PacketizationMode::SingleNalUnit,
                      H264PacketizationMode::NonInterleaved));

// Aggregation tests.
TEST(RtpPacketizerH264Test, StapA) {
  size_t fragments[] = {2, 2, 0x123};

  RTPFragmentationHeader fragmentation = CreateFragmentation(fragments);
  rtc::Buffer frame = CreateFrame(fragmentation);

  RtpPacketizerH264 packetizer(
      frame, kNoLimits, H264PacketizationMode::NonInterleaved, fragmentation);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(1));
  auto payload = packets[0].payload();
  EXPECT_EQ(payload.size(),
            kNalHeaderSize + 3 * kLengthFieldLength + frame.size());

  EXPECT_EQ(payload[0], kStapA);
  payload = payload.subview(kNalHeaderSize);
  // 1st fragment.
  EXPECT_THAT(payload.subview(0, kLengthFieldLength),
              ElementsAre(0, 2));  // Size.
  EXPECT_THAT(payload.subview(kLengthFieldLength, 2),
              ElementsAreArray(frame.data(), 2));
  payload = payload.subview(kLengthFieldLength + 2);
  // 2nd fragment.
  EXPECT_THAT(payload.subview(0, kLengthFieldLength),
              ElementsAre(0, 2));  // Size.
  EXPECT_THAT(payload.subview(kLengthFieldLength, 2),
              ElementsAreArray(frame.data() + 2, 2));
  payload = payload.subview(kLengthFieldLength + 2);
  // 3rd fragment.
  EXPECT_THAT(payload.subview(0, kLengthFieldLength),
              ElementsAre(0x1, 0x23));  // Size.
  EXPECT_THAT(payload.subview(kLengthFieldLength),
              ElementsAreArray(frame.data() + 4, 0x123));
}

TEST(RtpPacketizerH264Test, SingleNalUnitModeHasNoStapA) {
  // This is the same setup as for the StapA test.
  size_t fragments[] = {2, 2, 0x123};
  RTPFragmentationHeader fragmentation = CreateFragmentation(fragments);
  rtc::Buffer frame = CreateFrame(fragmentation);

  RtpPacketizerH264 packetizer(
      frame, kNoLimits, H264PacketizationMode::SingleNalUnit, fragmentation);
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
  size_t fragments[] = {kFirstFragmentSize, 2, 2};
  RTPFragmentationHeader fragmentation = CreateFragmentation(fragments);
  rtc::Buffer frame = CreateFrame(fragmentation);

  RtpPacketizerH264 packetizer(
      frame, limits, H264PacketizationMode::NonInterleaved, fragmentation);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(2));
  // Expect 1st packet is single nalu.
  EXPECT_THAT(packets[0].payload(),
              ElementsAreArray(frame.data(), kFirstFragmentSize));
  // Expect 2nd packet is aggregate of last two fragments.
  const uint8_t* tail = frame.data() + kFirstFragmentSize;
  EXPECT_THAT(packets[1].payload(), ElementsAre(kStapA,                  //
                                                0, 2, tail[0], tail[1],  //
                                                0, 2, tail[2], tail[3]));
}

TEST(RtpPacketizerH264Test, StapARespectsLastPacketReduction) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1000;
  limits.last_packet_reduction_len = 100;
  const size_t kLastFragmentSize =
      limits.max_payload_len - limits.last_packet_reduction_len;
  size_t fragments[] = {2, 2, kLastFragmentSize};
  RTPFragmentationHeader fragmentation = CreateFragmentation(fragments);
  rtc::Buffer frame = CreateFrame(fragmentation);

  RtpPacketizerH264 packetizer(
      frame, limits, H264PacketizationMode::NonInterleaved, fragmentation);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(2));
  // Expect 1st packet is aggregate of 1st two fragments.
  EXPECT_THAT(packets[0].payload(), ElementsAre(kStapA,                    //
                                                0, 2, frame[0], frame[1],  //
                                                0, 2, frame[2], frame[3]));
  // Expect 2nd packet is single nalu.
  EXPECT_THAT(packets[1].payload(),
              ElementsAreArray(frame.data() + 4, kLastFragmentSize));
}

TEST(RtpPacketizerH264Test, TooSmallForStapAHeaders) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1000;
  const size_t kLastFragmentSize =
      limits.max_payload_len - 3 * kLengthFieldLength - 4;
  size_t fragments[] = {2, 2, kLastFragmentSize};
  RTPFragmentationHeader fragmentation = CreateFragmentation(fragments);
  rtc::Buffer frame = CreateFrame(fragmentation);

  RtpPacketizerH264 packetizer(
      frame, limits, H264PacketizationMode::NonInterleaved, fragmentation);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(2));
  // Expect 1st packet is aggregate of 1st two fragments.
  EXPECT_THAT(packets[0].payload(), ElementsAre(kStapA,                    //
                                                0, 2, frame[0], frame[1],  //
                                                0, 2, frame[2], frame[3]));
  // Expect 2nd packet is single nalu.
  EXPECT_THAT(packets[1].payload(),
              ElementsAreArray(frame.data() + 4, kLastFragmentSize));
}

// Fragmentation + aggregation.
TEST(RtpPacketizerH264Test, MixedStapAFUA) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 100;
  const size_t kFuaPayloadSize = 70;
  const size_t kFuaNaluSize = kNalHeaderSize + 2 * kFuaPayloadSize;
  const size_t kStapANaluSize = 20;
  size_t fragments[] = {kFuaNaluSize, kStapANaluSize, kStapANaluSize};
  RTPFragmentationHeader fragmentation = CreateFragmentation(fragments);
  rtc::Buffer frame = CreateFrame(fragmentation);

  RtpPacketizerH264 packetizer(
      frame, limits, H264PacketizationMode::NonInterleaved, fragmentation);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(3));
  const uint8_t* next_fragment = frame.data() + kNalHeaderSize;
  // First expect two FU-A packets.
  EXPECT_THAT(packets[0].payload().subview(0, kFuAHeaderSize),
              ElementsAre(kFuA, FuDefs::kSBit | frame[0]));
  EXPECT_THAT(packets[0].payload().subview(kFuAHeaderSize),
              ElementsAreArray(next_fragment, kFuaPayloadSize));
  next_fragment += kFuaPayloadSize;

  EXPECT_THAT(packets[1].payload().subview(0, kFuAHeaderSize),
              ElementsAre(kFuA, FuDefs::kEBit | frame[0]));
  EXPECT_THAT(packets[1].payload().subview(kFuAHeaderSize),
              ElementsAreArray(next_fragment, kFuaPayloadSize));
  next_fragment += kFuaPayloadSize;

  // Then expect one STAP-A packet with two nal units.
  EXPECT_THAT(packets[2].payload()[0], kStapA);
  auto payload = packets[2].payload().subview(kNalHeaderSize);
  EXPECT_THAT(payload.subview(0, kLengthFieldLength),
              ElementsAre(0, kStapANaluSize));
  EXPECT_THAT(payload.subview(kLengthFieldLength, kStapANaluSize),
              ElementsAreArray(next_fragment, kStapANaluSize));
  payload = payload.subview(kLengthFieldLength + kStapANaluSize);
  next_fragment += kStapANaluSize;
  EXPECT_THAT(payload.subview(0, kLengthFieldLength),
              ElementsAre(0, kStapANaluSize));
  EXPECT_THAT(payload.subview(kLengthFieldLength),
              ElementsAreArray(next_fragment, kStapANaluSize));
}

// Splits frame with payload size |frame_payload_size| without fragmentation,
// Returns sizes of the payloads excluding fua headers.
std::vector<int> TestFua(size_t frame_payload_size,
                         const RtpPacketizer::PayloadSizeLimits& limits) {
  rtc::Buffer frame = CreateFrame(kNalHeaderSize + frame_payload_size);

  RtpPacketizerH264 packetizer(frame, limits,
                               H264PacketizationMode::NonInterleaved,
                               NoFragmentation(frame));
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  RTC_CHECK_GE(packets.size(), 2);  // Single packet indicates it is not FuA.
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
  EXPECT_THAT(fua_header, Each(Eq((kFuA << 8) | frame[0])));

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
  EXPECT_THAT(TestFua(1198, limits), ElementsAre(597, 601));
}

TEST(RtpPacketizerH264Test, FUAWithLastPacketReduction) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1200;
  limits.last_packet_reduction_len = 4;
  EXPECT_THAT(TestFua(1198, limits), ElementsAre(601, 597));
}

TEST(RtpPacketizerH264Test, FUAWithFirstAndLastPacketReduction) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1199;
  limits.first_packet_reduction_len = 100;
  limits.last_packet_reduction_len = 100;
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
  rtc::Buffer frame = CreateFrame(kMaxPayloadSize + 1);
  RTPFragmentationHeader fragmentation = NoFragmentation(frame);

  RtpPacketizerH264 packetizer(
      frame, limits, H264PacketizationMode::SingleNalUnit, fragmentation);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  EXPECT_THAT(packets, IsEmpty());
}

const uint8_t kStartSequence[] = {0x00, 0x00, 0x00, 0x01};
const uint8_t kOriginalSps[] = {kSps, 0x00, 0x00, 0x03, 0x03,
                                0xF4, 0x05, 0x03, 0xC7, 0xC0};
const uint8_t kRewrittenSps[] = {kSps, 0x00, 0x00, 0x03, 0x03, 0xF4, 0x05, 0x03,
                                 0xC7, 0xE0, 0x1B, 0x41, 0x10, 0x8D, 0x00};
const uint8_t kIdrOne[] = {kIdr, 0xFF, 0x00, 0x00, 0x04};
const uint8_t kIdrTwo[] = {kIdr, 0xFF, 0x00, 0x11};

class RtpPacketizerH264TestSpsRewriting : public ::testing::Test {
 public:
  void SetUp() override {
    fragmentation_header_.VerifyAndAllocateFragmentationHeader(3);
    fragmentation_header_.fragmentationVectorSize = 3;
    in_buffer_.AppendData(kStartSequence);

    fragmentation_header_.fragmentationOffset[0] = in_buffer_.size();
    fragmentation_header_.fragmentationLength[0] = sizeof(kOriginalSps);
    in_buffer_.AppendData(kOriginalSps);

    fragmentation_header_.fragmentationOffset[1] = in_buffer_.size();
    fragmentation_header_.fragmentationLength[1] = sizeof(kIdrOne);
    in_buffer_.AppendData(kIdrOne);

    fragmentation_header_.fragmentationOffset[2] = in_buffer_.size();
    fragmentation_header_.fragmentationLength[2] = sizeof(kIdrTwo);
    in_buffer_.AppendData(kIdrTwo);
  }

 protected:
  rtc::Buffer in_buffer_;
  RTPFragmentationHeader fragmentation_header_;
};

TEST_F(RtpPacketizerH264TestSpsRewriting, FuASps) {
  const size_t kHeaderOverhead = kFuAHeaderSize + 1;

  // Set size to fragment SPS into two FU-A packets.
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = sizeof(kOriginalSps) - 2 + kHeaderOverhead;
  RtpPacketizerH264 packetizer(in_buffer_, limits,
                               H264PacketizationMode::NonInterleaved,
                               fragmentation_header_);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  size_t offset = H264::kNaluTypeSize;
  size_t length = packets[0].payload_size() - kFuAHeaderSize;
  EXPECT_THAT(packets[0].payload().subview(kFuAHeaderSize),
              ElementsAreArray(&kRewrittenSps[offset], length));
  offset += length;

  length = packets[1].payload_size() - kFuAHeaderSize;
  EXPECT_THAT(packets[1].payload().subview(kFuAHeaderSize),
              ElementsAreArray(&kRewrittenSps[offset], length));
  offset += length;

  EXPECT_EQ(offset, sizeof(kRewrittenSps));
}

TEST_F(RtpPacketizerH264TestSpsRewriting, StapASps) {
  const size_t kHeaderOverhead = kFuAHeaderSize + 1;
  const size_t kExpectedTotalSize = H264::kNaluTypeSize +  // Stap-A type.
                                    sizeof(kRewrittenSps) + sizeof(kIdrOne) +
                                    sizeof(kIdrTwo) + (kLengthFieldLength * 3);

  // Set size to include SPS and the rest of the packets in a Stap-A package.
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = kExpectedTotalSize + kHeaderOverhead;

  RtpPacketizerH264 packetizer(in_buffer_, limits,
                               H264PacketizationMode::NonInterleaved,
                               fragmentation_header_);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(1));
  EXPECT_EQ(packets[0].payload_size(), kExpectedTotalSize);
  EXPECT_THAT(
      packets[0].payload().subview(H264::kNaluTypeSize + kLengthFieldLength,
                                   sizeof(kRewrittenSps)),
      ElementsAreArray(kRewrittenSps));
}

struct H264ParsedPayload : public RtpDepacketizer::ParsedPayload {
  RTPVideoHeaderH264& h264() {
    return absl::get<RTPVideoHeaderH264>(video.video_type_header);
  }
};

class RtpDepacketizerH264Test : public ::testing::Test {
 protected:
  RtpDepacketizerH264Test()
      : depacketizer_(RtpDepacketizer::Create(kVideoCodecH264)) {}

  void ExpectPacket(H264ParsedPayload* parsed_payload,
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

TEST_F(RtpDepacketizerH264Test, TestSingleNalu) {
  uint8_t packet[2] = {0x05, 0xFF};  // F=0, NRI=0, Type=5 (IDR).
  H264ParsedPayload payload;

  ASSERT_TRUE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
  ExpectPacket(&payload, packet, sizeof(packet));
  EXPECT_EQ(kVideoFrameKey, payload.frame_type);
  EXPECT_EQ(kVideoCodecH264, payload.video_header().codec);
  EXPECT_TRUE(payload.video_header().is_first_packet_in_frame);
  EXPECT_EQ(kH264SingleNalu, payload.h264().packetization_type);
  EXPECT_EQ(kIdr, payload.h264().nalu_type);
}

TEST_F(RtpDepacketizerH264Test, TestSingleNaluSpsWithResolution) {
  uint8_t packet[] = {kSps, 0x7A, 0x00, 0x1F, 0xBC, 0xD9, 0x40, 0x50,
                      0x05, 0xBA, 0x10, 0x00, 0x00, 0x03, 0x00, 0xC0,
                      0x00, 0x00, 0x03, 0x2A, 0xE0, 0xF1, 0x83, 0x25};
  H264ParsedPayload payload;

  ASSERT_TRUE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
  ExpectPacket(&payload, packet, sizeof(packet));
  EXPECT_EQ(kVideoFrameKey, payload.frame_type);
  EXPECT_EQ(kVideoCodecH264, payload.video_header().codec);
  EXPECT_TRUE(payload.video_header().is_first_packet_in_frame);
  EXPECT_EQ(kH264SingleNalu, payload.h264().packetization_type);
  EXPECT_EQ(1280u, payload.video_header().width);
  EXPECT_EQ(720u, payload.video_header().height);
}

TEST_F(RtpDepacketizerH264Test, TestStapAKey) {
  // clang-format off
  const NaluInfo kExpectedNalus[] = { {H264::kSps, 0, -1},
                                      {H264::kPps, 1, 2},
                                      {H264::kIdr, -1, 0} };
  uint8_t packet[] = {kStapA,  // F=0, NRI=0, Type=24.
                      // Length, nal header, payload.
                      0, 0x18, kExpectedNalus[0].type,
                        0x7A, 0x00, 0x1F, 0xBC, 0xD9, 0x40, 0x50, 0x05, 0xBA,
                        0x10, 0x00, 0x00, 0x03, 0x00, 0xC0, 0x00, 0x00, 0x03,
                        0x2A, 0xE0, 0xF1, 0x83, 0x25,
                      0, 0xD, kExpectedNalus[1].type,
                        0x69, 0xFC, 0x0, 0x0, 0x3, 0x0, 0x7, 0xFF, 0xFF, 0xFF,
                        0xF6, 0x40,
                      0, 0xB, kExpectedNalus[2].type,
                        0x85, 0xB8, 0x0, 0x4, 0x0, 0x0, 0x13, 0x93, 0x12, 0x0};
  // clang-format on

  H264ParsedPayload payload;
  ASSERT_TRUE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
  ExpectPacket(&payload, packet, sizeof(packet));
  EXPECT_EQ(kVideoFrameKey, payload.frame_type);
  EXPECT_EQ(kVideoCodecH264, payload.video_header().codec);
  EXPECT_TRUE(payload.video_header().is_first_packet_in_frame);
  const RTPVideoHeaderH264& h264 = payload.h264();
  EXPECT_EQ(kH264StapA, h264.packetization_type);
  // NALU type for aggregated packets is the type of the first packet only.
  EXPECT_EQ(kSps, h264.nalu_type);
  ASSERT_EQ(3u, h264.nalus_length);
  for (size_t i = 0; i < h264.nalus_length; ++i) {
    EXPECT_EQ(kExpectedNalus[i].type, h264.nalus[i].type)
        << "Failed parsing nalu " << i;
    EXPECT_EQ(kExpectedNalus[i].sps_id, h264.nalus[i].sps_id)
        << "Failed parsing nalu " << i;
    EXPECT_EQ(kExpectedNalus[i].pps_id, h264.nalus[i].pps_id)
        << "Failed parsing nalu " << i;
  }
}

TEST_F(RtpDepacketizerH264Test, TestStapANaluSpsWithResolution) {
  uint8_t packet[] = {kStapA,  // F=0, NRI=0, Type=24.
                               // Length (2 bytes), nal header, payload.
                      0x00, 0x19, kSps, 0x7A, 0x00, 0x1F, 0xBC, 0xD9, 0x40,
                      0x50, 0x05, 0xBA, 0x10, 0x00, 0x00, 0x03, 0x00, 0xC0,
                      0x00, 0x00, 0x03, 0x2A, 0xE0, 0xF1, 0x83, 0x25, 0x80,
                      0x00, 0x03, kIdr, 0xFF, 0x00, 0x00, 0x04, kIdr, 0xFF,
                      0x00, 0x11};

  H264ParsedPayload payload;

  ASSERT_TRUE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
  ExpectPacket(&payload, packet, sizeof(packet));
  EXPECT_EQ(kVideoFrameKey, payload.frame_type);
  EXPECT_EQ(kVideoCodecH264, payload.video_header().codec);
  EXPECT_TRUE(payload.video_header().is_first_packet_in_frame);
  EXPECT_EQ(kH264StapA, payload.h264().packetization_type);
  EXPECT_EQ(1280u, payload.video_header().width);
  EXPECT_EQ(720u, payload.video_header().height);
}

TEST_F(RtpDepacketizerH264Test, TestEmptyStapARejected) {
  uint8_t lone_empty_packet[] = {kStapA, 0x00, 0x00};

  uint8_t leading_empty_packet[] = {kStapA, 0x00, 0x00, 0x00, 0x04,
                                    kIdr,   0xFF, 0x00, 0x11};

  uint8_t middle_empty_packet[] = {kStapA, 0x00, 0x03, kIdr, 0xFF, 0x00, 0x00,
                                   0x00,   0x00, 0x04, kIdr, 0xFF, 0x00, 0x11};

  uint8_t trailing_empty_packet[] = {kStapA, 0x00, 0x03, kIdr,
                                     0xFF,   0x00, 0x00, 0x00};

  H264ParsedPayload payload;

  EXPECT_FALSE(depacketizer_->Parse(&payload, lone_empty_packet,
                                    sizeof(lone_empty_packet)));
  EXPECT_FALSE(depacketizer_->Parse(&payload, leading_empty_packet,
                                    sizeof(leading_empty_packet)));
  EXPECT_FALSE(depacketizer_->Parse(&payload, middle_empty_packet,
                                    sizeof(middle_empty_packet)));
  EXPECT_FALSE(depacketizer_->Parse(&payload, trailing_empty_packet,
                                    sizeof(trailing_empty_packet)));
}

TEST_F(RtpDepacketizerH264Test, DepacketizeWithRewriting) {
  rtc::Buffer in_buffer;
  rtc::Buffer out_buffer;

  uint8_t kHeader[2] = {kStapA};
  in_buffer.AppendData(kHeader, 1);
  out_buffer.AppendData(kHeader, 1);

  ByteWriter<uint16_t>::WriteBigEndian(kHeader, sizeof(kOriginalSps));
  in_buffer.AppendData(kHeader, 2);
  in_buffer.AppendData(kOriginalSps);
  ByteWriter<uint16_t>::WriteBigEndian(kHeader, sizeof(kRewrittenSps));
  out_buffer.AppendData(kHeader, 2);
  out_buffer.AppendData(kRewrittenSps);

  ByteWriter<uint16_t>::WriteBigEndian(kHeader, sizeof(kIdrOne));
  in_buffer.AppendData(kHeader, 2);
  in_buffer.AppendData(kIdrOne);
  out_buffer.AppendData(kHeader, 2);
  out_buffer.AppendData(kIdrOne);

  ByteWriter<uint16_t>::WriteBigEndian(kHeader, sizeof(kIdrTwo));
  in_buffer.AppendData(kHeader, 2);
  in_buffer.AppendData(kIdrTwo);
  out_buffer.AppendData(kHeader, 2);
  out_buffer.AppendData(kIdrTwo);

  H264ParsedPayload payload;
  EXPECT_TRUE(
      depacketizer_->Parse(&payload, in_buffer.data(), in_buffer.size()));

  std::vector<uint8_t> expected_packet_payload(
      out_buffer.data(), &out_buffer.data()[out_buffer.size()]);

  EXPECT_THAT(
      expected_packet_payload,
      ::testing::ElementsAreArray(payload.payload, payload.payload_length));
}

TEST_F(RtpDepacketizerH264Test, DepacketizeWithDoubleRewriting) {
  rtc::Buffer in_buffer;
  rtc::Buffer out_buffer;

  uint8_t kHeader[2] = {kStapA};
  in_buffer.AppendData(kHeader, 1);
  out_buffer.AppendData(kHeader, 1);

  // First SPS will be kept...
  ByteWriter<uint16_t>::WriteBigEndian(kHeader, sizeof(kOriginalSps));
  in_buffer.AppendData(kHeader, 2);
  in_buffer.AppendData(kOriginalSps);
  out_buffer.AppendData(kHeader, 2);
  out_buffer.AppendData(kOriginalSps);

  // ...only the second one will be rewritten.
  ByteWriter<uint16_t>::WriteBigEndian(kHeader, sizeof(kOriginalSps));
  in_buffer.AppendData(kHeader, 2);
  in_buffer.AppendData(kOriginalSps);
  ByteWriter<uint16_t>::WriteBigEndian(kHeader, sizeof(kRewrittenSps));
  out_buffer.AppendData(kHeader, 2);
  out_buffer.AppendData(kRewrittenSps);

  ByteWriter<uint16_t>::WriteBigEndian(kHeader, sizeof(kIdrOne));
  in_buffer.AppendData(kHeader, 2);
  in_buffer.AppendData(kIdrOne);
  out_buffer.AppendData(kHeader, 2);
  out_buffer.AppendData(kIdrOne);

  ByteWriter<uint16_t>::WriteBigEndian(kHeader, sizeof(kIdrTwo));
  in_buffer.AppendData(kHeader, 2);
  in_buffer.AppendData(kIdrTwo);
  out_buffer.AppendData(kHeader, 2);
  out_buffer.AppendData(kIdrTwo);

  H264ParsedPayload payload;
  EXPECT_TRUE(
      depacketizer_->Parse(&payload, in_buffer.data(), in_buffer.size()));

  std::vector<uint8_t> expected_packet_payload(
      out_buffer.data(), &out_buffer.data()[out_buffer.size()]);

  EXPECT_THAT(
      expected_packet_payload,
      ::testing::ElementsAreArray(payload.payload, payload.payload_length));
}

TEST_F(RtpDepacketizerH264Test, TestStapADelta) {
  uint8_t packet[16] = {kStapA,  // F=0, NRI=0, Type=24.
                                 // Length, nal header, payload.
                        0, 0x02, kSlice, 0xFF, 0, 0x03, kSlice, 0xFF, 0x00, 0,
                        0x04, kSlice, 0xFF, 0x00, 0x11};
  H264ParsedPayload payload;

  ASSERT_TRUE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
  ExpectPacket(&payload, packet, sizeof(packet));
  EXPECT_EQ(kVideoFrameDelta, payload.frame_type);
  EXPECT_EQ(kVideoCodecH264, payload.video_header().codec);
  EXPECT_TRUE(payload.video_header().is_first_packet_in_frame);
  EXPECT_EQ(kH264StapA, payload.h264().packetization_type);
  // NALU type for aggregated packets is the type of the first packet only.
  EXPECT_EQ(kSlice, payload.h264().nalu_type);
}

TEST_F(RtpDepacketizerH264Test, TestFuA) {
  // clang-format off
  uint8_t packet1[] = {
      kFuA,          // F=0, NRI=0, Type=28.
      kSBit | kIdr,  // FU header.
      0x85, 0xB8, 0x0, 0x4, 0x0, 0x0, 0x13, 0x93, 0x12, 0x0  // Payload.
  };
  // clang-format on
  const uint8_t kExpected1[] = {kIdr, 0x85, 0xB8, 0x0,  0x4, 0x0,
                                0x0,  0x13, 0x93, 0x12, 0x0};

  uint8_t packet2[] = {
      kFuA,  // F=0, NRI=0, Type=28.
      kIdr,  // FU header.
      0x02   // Payload.
  };
  const uint8_t kExpected2[] = {0x02};

  uint8_t packet3[] = {
      kFuA,          // F=0, NRI=0, Type=28.
      kEBit | kIdr,  // FU header.
      0x03           // Payload.
  };
  const uint8_t kExpected3[] = {0x03};

  H264ParsedPayload payload;

  // We expect that the first packet is one byte shorter since the FU-A header
  // has been replaced by the original nal header.
  ASSERT_TRUE(depacketizer_->Parse(&payload, packet1, sizeof(packet1)));
  ExpectPacket(&payload, kExpected1, sizeof(kExpected1));
  EXPECT_EQ(kVideoFrameKey, payload.frame_type);
  EXPECT_EQ(kVideoCodecH264, payload.video_header().codec);
  EXPECT_TRUE(payload.video_header().is_first_packet_in_frame);
  const RTPVideoHeaderH264& h264 = payload.h264();
  EXPECT_EQ(kH264FuA, h264.packetization_type);
  EXPECT_EQ(kIdr, h264.nalu_type);
  ASSERT_EQ(1u, h264.nalus_length);
  EXPECT_EQ(static_cast<H264::NaluType>(kIdr), h264.nalus[0].type);
  EXPECT_EQ(-1, h264.nalus[0].sps_id);
  EXPECT_EQ(0, h264.nalus[0].pps_id);

  // Following packets will be 2 bytes shorter since they will only be appended
  // onto the first packet.
  payload = H264ParsedPayload();
  ASSERT_TRUE(depacketizer_->Parse(&payload, packet2, sizeof(packet2)));
  ExpectPacket(&payload, kExpected2, sizeof(kExpected2));
  EXPECT_EQ(kVideoFrameKey, payload.frame_type);
  EXPECT_EQ(kVideoCodecH264, payload.video_header().codec);
  EXPECT_FALSE(payload.video_header().is_first_packet_in_frame);
  {
    const RTPVideoHeaderH264& h264 = payload.h264();
    EXPECT_EQ(kH264FuA, h264.packetization_type);
    EXPECT_EQ(kIdr, h264.nalu_type);
    // NALU info is only expected for the first FU-A packet.
    EXPECT_EQ(0u, h264.nalus_length);
  }

  payload = H264ParsedPayload();
  ASSERT_TRUE(depacketizer_->Parse(&payload, packet3, sizeof(packet3)));
  ExpectPacket(&payload, kExpected3, sizeof(kExpected3));
  EXPECT_EQ(kVideoFrameKey, payload.frame_type);
  EXPECT_EQ(kVideoCodecH264, payload.video_header().codec);
  EXPECT_FALSE(payload.video_header().is_first_packet_in_frame);
  {
    const RTPVideoHeaderH264& h264 = payload.h264();
    EXPECT_EQ(kH264FuA, h264.packetization_type);
    EXPECT_EQ(kIdr, h264.nalu_type);
    // NALU info is only expected for the first FU-A packet.
    ASSERT_EQ(0u, h264.nalus_length);
  }
}

TEST_F(RtpDepacketizerH264Test, TestEmptyPayload) {
  // Using a wild pointer to crash on accesses from inside the depacketizer.
  uint8_t* garbage_ptr = reinterpret_cast<uint8_t*>(0x4711);
  H264ParsedPayload payload;
  EXPECT_FALSE(depacketizer_->Parse(&payload, garbage_ptr, 0));
}

TEST_F(RtpDepacketizerH264Test, TestTruncatedFuaNalu) {
  const uint8_t kPayload[] = {0x9c};
  H264ParsedPayload payload;
  EXPECT_FALSE(depacketizer_->Parse(&payload, kPayload, sizeof(kPayload)));
}

TEST_F(RtpDepacketizerH264Test, TestTruncatedSingleStapANalu) {
  const uint8_t kPayload[] = {0xd8, 0x27};
  H264ParsedPayload payload;
  EXPECT_FALSE(depacketizer_->Parse(&payload, kPayload, sizeof(kPayload)));
}

TEST_F(RtpDepacketizerH264Test, TestStapAPacketWithTruncatedNalUnits) {
  const uint8_t kPayload[] = {0x58, 0xCB, 0xED, 0xDF};
  H264ParsedPayload payload;
  EXPECT_FALSE(depacketizer_->Parse(&payload, kPayload, sizeof(kPayload)));
}

TEST_F(RtpDepacketizerH264Test, TestTruncationJustAfterSingleStapANalu) {
  const uint8_t kPayload[] = {0x38, 0x27, 0x27};
  H264ParsedPayload payload;
  EXPECT_FALSE(depacketizer_->Parse(&payload, kPayload, sizeof(kPayload)));
}

TEST_F(RtpDepacketizerH264Test, TestShortSpsPacket) {
  const uint8_t kPayload[] = {0x27, 0x80, 0x00};
  H264ParsedPayload payload;
  EXPECT_TRUE(depacketizer_->Parse(&payload, kPayload, sizeof(kPayload)));
}

TEST_F(RtpDepacketizerH264Test, TestSeiPacket) {
  const uint8_t kPayload[] = {
      kSei,                   // F=0, NRI=0, Type=6.
      0x03, 0x03, 0x03, 0x03  // Payload.
  };
  H264ParsedPayload payload;
  ASSERT_TRUE(depacketizer_->Parse(&payload, kPayload, sizeof(kPayload)));
  const RTPVideoHeaderH264& h264 = payload.h264();
  EXPECT_EQ(kVideoFrameDelta, payload.frame_type);
  EXPECT_EQ(kH264SingleNalu, h264.packetization_type);
  EXPECT_EQ(kSei, h264.nalu_type);
  ASSERT_EQ(1u, h264.nalus_length);
  EXPECT_EQ(static_cast<H264::NaluType>(kSei), h264.nalus[0].type);
  EXPECT_EQ(-1, h264.nalus[0].sps_id);
  EXPECT_EQ(-1, h264.nalus[0].pps_id);
}

}  // namespace
}  // namespace webrtc
