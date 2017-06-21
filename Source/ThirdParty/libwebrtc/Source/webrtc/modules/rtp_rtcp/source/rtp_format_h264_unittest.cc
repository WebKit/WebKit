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

#include "webrtc/base/array_view.h"
#include "webrtc/common_video/h264/h264_common.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/rtp_rtcp/mocks/mock_rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAreArray;

constexpr RtpPacketToSend::ExtensionManager* kNoExtensions = nullptr;
const size_t kMaxPayloadSize = 1200;
const size_t kLengthFieldLength = 2;

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

void CreateThreeFragments(RTPFragmentationHeader* fragmentation,
                          size_t frameSize,
                          size_t payloadOffset) {
  fragmentation->VerifyAndAllocateFragmentationHeader(3);
  fragmentation->fragmentationOffset[0] = 0;
  fragmentation->fragmentationLength[0] = 2;
  fragmentation->fragmentationOffset[1] = 2;
  fragmentation->fragmentationLength[1] = 2;
  fragmentation->fragmentationOffset[2] = 4;
  fragmentation->fragmentationLength[2] =
      kNalHeaderSize + frameSize - payloadOffset;
}

RtpPacketizer* CreateH264Packetizer(H264PacketizationMode mode,
                                    size_t max_payload_size,
                                    size_t last_packet_reduction) {
  RTPVideoTypeHeader type_header;
  type_header.H264.packetization_mode = mode;
  return RtpPacketizer::Create(kRtpVideoH264, max_payload_size,
                               last_packet_reduction, &type_header,
                               kEmptyFrame);
}

void VerifyFua(size_t fua_index,
               const uint8_t* expected_payload,
               int offset,
               rtc::ArrayView<const uint8_t> packet,
               const std::vector<size_t>& expected_sizes) {
  ASSERT_EQ(expected_sizes[fua_index] + kFuAHeaderSize, packet.size())
      << "FUA index: " << fua_index;
  const uint8_t kFuIndicator = 0x1C;  // F=0, NRI=0, Type=28.
  EXPECT_EQ(kFuIndicator, packet[0]) << "FUA index: " << fua_index;
  bool should_be_last_fua = (fua_index == expected_sizes.size() - 1);
  uint8_t fu_header = 0;
  if (fua_index == 0)
    fu_header = 0x85;  // S=1, E=0, R=0, Type=5.
  else if (should_be_last_fua)
    fu_header = 0x45;  // S=0, E=1, R=0, Type=5.
  else
    fu_header = 0x05;  // S=0, E=0, R=0, Type=5.
  EXPECT_EQ(fu_header, packet[1]) << "FUA index: " << fua_index;
  std::vector<uint8_t> expected_packet_payload(
      &expected_payload[offset],
      &expected_payload[offset + expected_sizes[fua_index]]);
  EXPECT_THAT(expected_packet_payload,
              ElementsAreArray(&packet[2], expected_sizes[fua_index]))
      << "FUA index: " << fua_index;
}

void TestFua(size_t frame_size,
             size_t max_payload_size,
             size_t last_packet_reduction,
             const std::vector<size_t>& expected_sizes) {
  std::unique_ptr<uint8_t[]> frame;
  frame.reset(new uint8_t[frame_size]);
  frame[0] = 0x05;  // F=0, NRI=0, Type=5.
  for (size_t i = 0; i < frame_size - kNalHeaderSize; ++i) {
    frame[i + kNalHeaderSize] = i;
  }
  RTPFragmentationHeader fragmentation;
  fragmentation.VerifyAndAllocateFragmentationHeader(1);
  fragmentation.fragmentationOffset[0] = 0;
  fragmentation.fragmentationLength[0] = frame_size;
  std::unique_ptr<RtpPacketizer> packetizer(
      CreateH264Packetizer(H264PacketizationMode::NonInterleaved,
                           max_payload_size, last_packet_reduction));
  EXPECT_EQ(
      expected_sizes.size(),
      packetizer->SetPayloadData(frame.get(), frame_size, &fragmentation));

  RtpPacketToSend packet(kNoExtensions);
  ASSERT_LE(max_payload_size, packet.FreeCapacity());
  size_t offset = kNalHeaderSize;
  for (size_t i = 0; i < expected_sizes.size(); ++i) {
    ASSERT_TRUE(packetizer->NextPacket(&packet));
    VerifyFua(i, frame.get(), offset, packet.payload(), expected_sizes);
    offset += expected_sizes[i];
  }

  EXPECT_FALSE(packetizer->NextPacket(&packet));
}

size_t GetExpectedNaluOffset(const RTPFragmentationHeader& fragmentation,
                             size_t start_index,
                             size_t nalu_index) {
  assert(nalu_index < fragmentation.fragmentationVectorSize);
  size_t expected_nalu_offset = kNalHeaderSize;  // STAP-A header.
  for (size_t i = start_index; i < nalu_index; ++i) {
    expected_nalu_offset +=
        kLengthFieldLength + fragmentation.fragmentationLength[i];
  }
  return expected_nalu_offset;
}

void VerifyStapAPayload(const RTPFragmentationHeader& fragmentation,
                        size_t first_stapa_index,
                        size_t nalu_index,
                        rtc::ArrayView<const uint8_t> frame,
                        rtc::ArrayView<const uint8_t> packet) {
  size_t expected_payload_offset =
      GetExpectedNaluOffset(fragmentation, first_stapa_index, nalu_index) +
      kLengthFieldLength;
  size_t offset = fragmentation.fragmentationOffset[nalu_index];
  const uint8_t* expected_payload = &frame[offset];
  size_t expected_payload_length =
      fragmentation.fragmentationLength[nalu_index];
  ASSERT_LE(offset + expected_payload_length, frame.size());
  ASSERT_LE(expected_payload_offset + expected_payload_length, packet.size());
  std::vector<uint8_t> expected_payload_vector(
      expected_payload, &expected_payload[expected_payload_length]);
  EXPECT_THAT(expected_payload_vector,
              ElementsAreArray(&packet[expected_payload_offset],
                               expected_payload_length));
}

void VerifySingleNaluPayload(const RTPFragmentationHeader& fragmentation,
                             size_t nalu_index,
                             rtc::ArrayView<const uint8_t> frame,
                             rtc::ArrayView<const uint8_t> packet) {
  auto fragment = frame.subview(fragmentation.fragmentationOffset[nalu_index],
                                fragmentation.fragmentationLength[nalu_index]);
  EXPECT_THAT(packet, ElementsAreArray(fragment.begin(), fragment.end()));
}
}  // namespace

// Tests that should work with both packetization mode 0 and
// packetization mode 1.
class RtpPacketizerH264ModeTest
    : public ::testing::TestWithParam<H264PacketizationMode> {};

TEST_P(RtpPacketizerH264ModeTest, TestSingleNalu) {
  const uint8_t frame[2] = {0x05, 0xFF};  // F=0, NRI=0, Type=5.
  RTPFragmentationHeader fragmentation;
  fragmentation.VerifyAndAllocateFragmentationHeader(1);
  fragmentation.fragmentationOffset[0] = 0;
  fragmentation.fragmentationLength[0] = sizeof(frame);
  std::unique_ptr<RtpPacketizer> packetizer(
      CreateH264Packetizer(GetParam(), kMaxPayloadSize, 0));
  ASSERT_EQ(1u,
            packetizer->SetPayloadData(frame, sizeof(frame), &fragmentation));
  RtpPacketToSend packet(kNoExtensions);
  ASSERT_LE(kMaxPayloadSize, packet.FreeCapacity());
  ASSERT_TRUE(packetizer->NextPacket(&packet));
  EXPECT_EQ(2u, packet.payload_size());
  VerifySingleNaluPayload(fragmentation, 0, frame, packet.payload());
  EXPECT_FALSE(packetizer->NextPacket(&packet));
}

TEST_P(RtpPacketizerH264ModeTest, TestSingleNaluTwoPackets) {
  const size_t kFrameSize = kMaxPayloadSize + 100;
  uint8_t frame[kFrameSize] = {0};
  for (size_t i = 0; i < kFrameSize; ++i)
    frame[i] = i;
  RTPFragmentationHeader fragmentation;
  fragmentation.VerifyAndAllocateFragmentationHeader(2);
  fragmentation.fragmentationOffset[0] = 0;
  fragmentation.fragmentationLength[0] = kMaxPayloadSize;
  fragmentation.fragmentationOffset[1] = kMaxPayloadSize;
  fragmentation.fragmentationLength[1] = 100;
  // Set NAL headers.
  frame[fragmentation.fragmentationOffset[0]] = 0x01;
  frame[fragmentation.fragmentationOffset[1]] = 0x01;

  std::unique_ptr<RtpPacketizer> packetizer(
      CreateH264Packetizer(GetParam(), kMaxPayloadSize, 0));
  ASSERT_EQ(2u, packetizer->SetPayloadData(frame, kFrameSize, &fragmentation));

  RtpPacketToSend packet(kNoExtensions);
  ASSERT_TRUE(packetizer->NextPacket(&packet));
  ASSERT_EQ(fragmentation.fragmentationOffset[1], packet.payload_size());
  VerifySingleNaluPayload(fragmentation, 0, frame, packet.payload());

  ASSERT_TRUE(packetizer->NextPacket(&packet));
  ASSERT_EQ(fragmentation.fragmentationLength[1], packet.payload_size());
  VerifySingleNaluPayload(fragmentation, 1, frame, packet.payload());

  EXPECT_FALSE(packetizer->NextPacket(&packet));
}

INSTANTIATE_TEST_CASE_P(
    PacketMode,
    RtpPacketizerH264ModeTest,
    ::testing::Values(H264PacketizationMode::SingleNalUnit,
                      H264PacketizationMode::NonInterleaved));

TEST(RtpPacketizerH264Test, TestStapA) {
  const size_t kFrameSize =
      kMaxPayloadSize - 3 * kLengthFieldLength - kNalHeaderSize;
  uint8_t frame[kFrameSize] = {0x07, 0xFF,  // F=0, NRI=0, Type=7 (SPS).
                               0x08, 0xFF,  // F=0, NRI=0, Type=8 (PPS).
                               0x05};       // F=0, NRI=0, Type=5 (IDR).
  const size_t kPayloadOffset = 5;
  for (size_t i = 0; i < kFrameSize - kPayloadOffset; ++i)
    frame[i + kPayloadOffset] = i;
  RTPFragmentationHeader fragmentation;
  CreateThreeFragments(&fragmentation, kFrameSize, kPayloadOffset);
  std::unique_ptr<RtpPacketizer> packetizer(CreateH264Packetizer(
      H264PacketizationMode::NonInterleaved, kMaxPayloadSize, 0));
  ASSERT_EQ(1u, packetizer->SetPayloadData(frame, kFrameSize, &fragmentation));

  RtpPacketToSend packet(kNoExtensions);
  ASSERT_LE(kMaxPayloadSize, packet.FreeCapacity());
  ASSERT_TRUE(packetizer->NextPacket(&packet));
  size_t expected_packet_size =
      kNalHeaderSize + 3 * kLengthFieldLength + kFrameSize;
  ASSERT_EQ(expected_packet_size, packet.payload_size());

  for (size_t i = 0; i < fragmentation.fragmentationVectorSize; ++i)
    VerifyStapAPayload(fragmentation, 0, i, frame, packet.payload());

  EXPECT_FALSE(packetizer->NextPacket(&packet));
}

TEST(RtpPacketizerH264Test, TestStapARespectsPacketReduction) {
  const size_t kLastPacketReduction = 100;
  const size_t kFrameSize = kMaxPayloadSize - 1 - kLastPacketReduction;
  uint8_t frame[kFrameSize] = {0x07, 0xFF,  // F=0, NRI=0, Type=7.
                               0x08, 0xFF,  // F=0, NRI=0, Type=8.
                               0x05};       // F=0, NRI=0, Type=5.
  const size_t kPayloadOffset = 5;
  for (size_t i = 0; i < kFrameSize - kPayloadOffset; ++i)
    frame[i + kPayloadOffset] = i;
  RTPFragmentationHeader fragmentation;
  fragmentation.VerifyAndAllocateFragmentationHeader(3);
  fragmentation.fragmentationOffset[0] = 0;
  fragmentation.fragmentationLength[0] = 2;
  fragmentation.fragmentationOffset[1] = 2;
  fragmentation.fragmentationLength[1] = 2;
  fragmentation.fragmentationOffset[2] = 4;
  fragmentation.fragmentationLength[2] =
      kNalHeaderSize + kFrameSize - kPayloadOffset;
  std::unique_ptr<RtpPacketizer> packetizer(
      CreateH264Packetizer(H264PacketizationMode::NonInterleaved,
                           kMaxPayloadSize, kLastPacketReduction));
  ASSERT_EQ(2u, packetizer->SetPayloadData(frame, kFrameSize, &fragmentation));

  RtpPacketToSend packet(kNoExtensions);
  ASSERT_LE(kMaxPayloadSize, packet.FreeCapacity());
  ASSERT_TRUE(packetizer->NextPacket(&packet));
  size_t expected_packet_size = kNalHeaderSize;
  for (size_t i = 0; i < 2; ++i) {
    expected_packet_size +=
        kLengthFieldLength + fragmentation.fragmentationLength[i];
  }
  ASSERT_EQ(expected_packet_size, packet.payload_size());
  for (size_t i = 0; i < 2; ++i)
    VerifyStapAPayload(fragmentation, 0, i, frame, packet.payload());

  ASSERT_TRUE(packetizer->NextPacket(&packet));
  expected_packet_size = fragmentation.fragmentationLength[2];
  ASSERT_EQ(expected_packet_size, packet.payload_size());
  VerifySingleNaluPayload(fragmentation, 2, frame, packet.payload());

  EXPECT_FALSE(packetizer->NextPacket(&packet));
}

TEST(RtpPacketizerH264Test, TestSingleNalUnitModeHasNoStapA) {
  // This is the same setup as for the TestStapA test.
  const size_t kFrameSize =
      kMaxPayloadSize - 3 * kLengthFieldLength - kNalHeaderSize;
  uint8_t frame[kFrameSize] = {0x07, 0xFF,  // F=0, NRI=0, Type=7 (SPS).
                               0x08, 0xFF,  // F=0, NRI=0, Type=8 (PPS).
                               0x05};       // F=0, NRI=0, Type=5 (IDR).
  const size_t kPayloadOffset = 5;
  for (size_t i = 0; i < kFrameSize - kPayloadOffset; ++i)
    frame[i + kPayloadOffset] = i;
  RTPFragmentationHeader fragmentation;
  CreateThreeFragments(&fragmentation, kFrameSize, kPayloadOffset);
  std::unique_ptr<RtpPacketizer> packetizer(CreateH264Packetizer(
      H264PacketizationMode::SingleNalUnit, kMaxPayloadSize, 0));
  packetizer->SetPayloadData(frame, kFrameSize, &fragmentation);

  RtpPacketToSend packet(kNoExtensions);
  // The three fragments should be returned as three packets.
  ASSERT_TRUE(packetizer->NextPacket(&packet));
  ASSERT_TRUE(packetizer->NextPacket(&packet));
  ASSERT_TRUE(packetizer->NextPacket(&packet));
  EXPECT_FALSE(packetizer->NextPacket(&packet));
}

TEST(RtpPacketizerH264Test, TestTooSmallForStapAHeaders) {
  const size_t kFrameSize = kMaxPayloadSize - 1;
  uint8_t frame[kFrameSize] = {0x07, 0xFF,  // F=0, NRI=0, Type=7.
                               0x08, 0xFF,  // F=0, NRI=0, Type=8.
                               0x05};       // F=0, NRI=0, Type=5.
  const size_t kPayloadOffset = 5;
  for (size_t i = 0; i < kFrameSize - kPayloadOffset; ++i)
    frame[i + kPayloadOffset] = i;
  RTPFragmentationHeader fragmentation;
  fragmentation.VerifyAndAllocateFragmentationHeader(3);
  fragmentation.fragmentationOffset[0] = 0;
  fragmentation.fragmentationLength[0] = 2;
  fragmentation.fragmentationOffset[1] = 2;
  fragmentation.fragmentationLength[1] = 2;
  fragmentation.fragmentationOffset[2] = 4;
  fragmentation.fragmentationLength[2] =
      kNalHeaderSize + kFrameSize - kPayloadOffset;
  std::unique_ptr<RtpPacketizer> packetizer(CreateH264Packetizer(
      H264PacketizationMode::NonInterleaved, kMaxPayloadSize, 0));
  ASSERT_EQ(2u, packetizer->SetPayloadData(frame, kFrameSize, &fragmentation));

  RtpPacketToSend packet(kNoExtensions);
  ASSERT_LE(kMaxPayloadSize, packet.FreeCapacity());
  ASSERT_TRUE(packetizer->NextPacket(&packet));
  size_t expected_packet_size = kNalHeaderSize;
  for (size_t i = 0; i < 2; ++i) {
    expected_packet_size +=
        kLengthFieldLength + fragmentation.fragmentationLength[i];
  }
  ASSERT_EQ(expected_packet_size, packet.payload_size());
  for (size_t i = 0; i < 2; ++i)
    VerifyStapAPayload(fragmentation, 0, i, frame, packet.payload());

  ASSERT_TRUE(packetizer->NextPacket(&packet));
  expected_packet_size = fragmentation.fragmentationLength[2];
  ASSERT_EQ(expected_packet_size, packet.payload_size());
  VerifySingleNaluPayload(fragmentation, 2, frame, packet.payload());

  EXPECT_FALSE(packetizer->NextPacket(&packet));
}

TEST(RtpPacketizerH264Test, TestMixedStapA_FUA) {
  const size_t kFuaNaluSize = 2 * (kMaxPayloadSize - 100);
  const size_t kStapANaluSize = 100;
  RTPFragmentationHeader fragmentation;
  fragmentation.VerifyAndAllocateFragmentationHeader(3);
  fragmentation.fragmentationOffset[0] = 0;
  fragmentation.fragmentationLength[0] = kFuaNaluSize;
  fragmentation.fragmentationOffset[1] = kFuaNaluSize;
  fragmentation.fragmentationLength[1] = kStapANaluSize;
  fragmentation.fragmentationOffset[2] = kFuaNaluSize + kStapANaluSize;
  fragmentation.fragmentationLength[2] = kStapANaluSize;
  const size_t kFrameSize = kFuaNaluSize + 2 * kStapANaluSize;
  uint8_t frame[kFrameSize];
  size_t nalu_offset = 0;
  for (size_t i = 0; i < fragmentation.fragmentationVectorSize; ++i) {
    nalu_offset = fragmentation.fragmentationOffset[i];
    frame[nalu_offset] = 0x05;  // F=0, NRI=0, Type=5.
    for (size_t j = 1; j < fragmentation.fragmentationLength[i]; ++j) {
      frame[nalu_offset + j] = i + j;
    }
  }
  std::unique_ptr<RtpPacketizer> packetizer(CreateH264Packetizer(
      H264PacketizationMode::NonInterleaved, kMaxPayloadSize, 0));
  ASSERT_EQ(3u, packetizer->SetPayloadData(frame, kFrameSize, &fragmentation));

  // First expecting two FU-A packets.
  std::vector<size_t> fua_sizes;
  fua_sizes.push_back(1099);
  fua_sizes.push_back(1100);
  RtpPacketToSend packet(kNoExtensions);
  ASSERT_LE(kMaxPayloadSize, packet.FreeCapacity());
  int fua_offset = kNalHeaderSize;
  for (size_t i = 0; i < 2; ++i) {
    ASSERT_TRUE(packetizer->NextPacket(&packet));
    VerifyFua(i, frame, fua_offset, packet.payload(), fua_sizes);
    fua_offset += fua_sizes[i];
  }
  // Then expecting one STAP-A packet with two nal units.
  ASSERT_TRUE(packetizer->NextPacket(&packet));
  size_t expected_packet_size =
      kNalHeaderSize + 2 * kLengthFieldLength + 2 * kStapANaluSize;
  ASSERT_EQ(expected_packet_size, packet.payload_size());
  for (size_t i = 1; i < fragmentation.fragmentationVectorSize; ++i)
    VerifyStapAPayload(fragmentation, 1, i, frame, packet.payload());

  EXPECT_FALSE(packetizer->NextPacket(&packet));
}

TEST(RtpPacketizerH264Test, TestFUAOddSize) {
  const size_t kExpectedPayloadSizes[2] = {600, 600};
  TestFua(
      kMaxPayloadSize + 1, kMaxPayloadSize, 0,
      std::vector<size_t>(kExpectedPayloadSizes,
                          kExpectedPayloadSizes +
                              sizeof(kExpectedPayloadSizes) / sizeof(size_t)));
}

TEST(RtpPacketizerH264Test, TestFUAWithLastPacketReduction) {
  const size_t kExpectedPayloadSizes[2] = {601, 597};
  TestFua(
      kMaxPayloadSize - 1, kMaxPayloadSize, 4,
      std::vector<size_t>(kExpectedPayloadSizes,
                          kExpectedPayloadSizes +
                              sizeof(kExpectedPayloadSizes) / sizeof(size_t)));
}

TEST(RtpPacketizerH264Test, TestFUAEvenSize) {
  const size_t kExpectedPayloadSizes[2] = {600, 601};
  TestFua(
      kMaxPayloadSize + 2, kMaxPayloadSize, 0,
      std::vector<size_t>(kExpectedPayloadSizes,
                          kExpectedPayloadSizes +
                              sizeof(kExpectedPayloadSizes) / sizeof(size_t)));
}

TEST(RtpPacketizerH264Test, TestFUARounding) {
  const size_t kExpectedPayloadSizes[8] = {1265, 1265, 1265, 1265,
                                           1265, 1266, 1266, 1266};
  TestFua(
      10124, 1448, 0,
      std::vector<size_t>(kExpectedPayloadSizes,
                          kExpectedPayloadSizes +
                              sizeof(kExpectedPayloadSizes) / sizeof(size_t)));
}

TEST(RtpPacketizerH264Test, TestFUABig) {
  const size_t kExpectedPayloadSizes[10] = {1198, 1198, 1198, 1198, 1198,
                                            1198, 1198, 1198, 1198, 1198};
  // Generate 10 full sized packets, leave room for FU-A headers minus the NALU
  // header.
  TestFua(
      10 * (kMaxPayloadSize - kFuAHeaderSize) + kNalHeaderSize, kMaxPayloadSize,
      0,
      std::vector<size_t>(kExpectedPayloadSizes,
                          kExpectedPayloadSizes +
                              sizeof(kExpectedPayloadSizes) / sizeof(size_t)));
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

TEST(RtpPacketizerH264DeathTest, SendOverlongDataInPacketizationMode0) {
  const size_t kFrameSize = kMaxPayloadSize + 1;
  uint8_t frame[kFrameSize] = {0};
  for (size_t i = 0; i < kFrameSize; ++i)
    frame[i] = i;
  RTPFragmentationHeader fragmentation;
  fragmentation.VerifyAndAllocateFragmentationHeader(1);
  fragmentation.fragmentationOffset[0] = 0;
  fragmentation.fragmentationLength[0] = kFrameSize;
  // Set NAL headers.
  frame[fragmentation.fragmentationOffset[0]] = 0x01;

  std::unique_ptr<RtpPacketizer> packetizer(CreateH264Packetizer(
      H264PacketizationMode::SingleNalUnit, kMaxPayloadSize, 0));
  EXPECT_DEATH(packetizer->SetPayloadData(frame, kFrameSize, &fragmentation),
               "payload_size");
}

#endif  // RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

namespace {
const uint8_t kStartSequence[] = {0x00, 0x00, 0x00, 0x01};
const uint8_t kOriginalSps[] = {kSps, 0x00, 0x00, 0x03, 0x03,
                                0xF4, 0x05, 0x03, 0xC7, 0xC0};
const uint8_t kRewrittenSps[] = {kSps, 0x00, 0x00, 0x03, 0x03, 0xF4, 0x05, 0x03,
                                 0xC7, 0xE0, 0x1B, 0x41, 0x10, 0x8D, 0x00};
const uint8_t kIdrOne[] = {kIdr, 0xFF, 0x00, 0x00, 0x04};
const uint8_t kIdrTwo[] = {kIdr, 0xFF, 0x00, 0x11};
}

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
  std::unique_ptr<RtpPacketizer> packetizer_;
};

TEST_F(RtpPacketizerH264TestSpsRewriting, FuASps) {
  const size_t kHeaderOverhead = kFuAHeaderSize + 1;

  // Set size to fragment SPS into two FU-A packets.
  packetizer_.reset(
      CreateH264Packetizer(H264PacketizationMode::NonInterleaved,
                           sizeof(kOriginalSps) - 2 + kHeaderOverhead, 0));

  packetizer_->SetPayloadData(in_buffer_.data(), in_buffer_.size(),
                              &fragmentation_header_);

  RtpPacketToSend packet(kNoExtensions);
  ASSERT_LE(sizeof(kOriginalSps) + kHeaderOverhead, packet.FreeCapacity());

  EXPECT_TRUE(packetizer_->NextPacket(&packet));
  size_t offset = H264::kNaluTypeSize;
  size_t length = packet.payload_size() - kFuAHeaderSize;
  EXPECT_THAT(packet.payload().subview(kFuAHeaderSize),
              ElementsAreArray(&kRewrittenSps[offset], length));
  offset += length;

  EXPECT_TRUE(packetizer_->NextPacket(&packet));
  length = packet.payload_size() - kFuAHeaderSize;
  EXPECT_THAT(packet.payload().subview(kFuAHeaderSize),
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
  packetizer_.reset(CreateH264Packetizer(H264PacketizationMode::NonInterleaved,
                                         kExpectedTotalSize + kHeaderOverhead,
                                         0));

  packetizer_->SetPayloadData(in_buffer_.data(), in_buffer_.size(),
                              &fragmentation_header_);

  RtpPacketToSend packet(kNoExtensions);
  ASSERT_LE(kExpectedTotalSize + kHeaderOverhead, packet.FreeCapacity());

  EXPECT_TRUE(packetizer_->NextPacket(&packet));
  EXPECT_EQ(kExpectedTotalSize, packet.payload_size());
  EXPECT_THAT(packet.payload().subview(H264::kNaluTypeSize + kLengthFieldLength,
                                       sizeof(kRewrittenSps)),
              ElementsAreArray(kRewrittenSps));
}

class RtpDepacketizerH264Test : public ::testing::Test {
 protected:
  RtpDepacketizerH264Test()
      : depacketizer_(RtpDepacketizer::Create(kRtpVideoH264)) {}

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

TEST_F(RtpDepacketizerH264Test, TestSingleNalu) {
  uint8_t packet[2] = {0x05, 0xFF};  // F=0, NRI=0, Type=5 (IDR).
  RtpDepacketizer::ParsedPayload payload;

  ASSERT_TRUE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
  ExpectPacket(&payload, packet, sizeof(packet));
  EXPECT_EQ(kVideoFrameKey, payload.frame_type);
  EXPECT_EQ(kRtpVideoH264, payload.type.Video.codec);
  EXPECT_TRUE(payload.type.Video.is_first_packet_in_frame);
  EXPECT_EQ(kH264SingleNalu,
            payload.type.Video.codecHeader.H264.packetization_type);
  EXPECT_EQ(kIdr, payload.type.Video.codecHeader.H264.nalu_type);
}

TEST_F(RtpDepacketizerH264Test, TestSingleNaluSpsWithResolution) {
  uint8_t packet[] = {kSps, 0x7A, 0x00, 0x1F, 0xBC, 0xD9, 0x40, 0x50,
                      0x05, 0xBA, 0x10, 0x00, 0x00, 0x03, 0x00, 0xC0,
                      0x00, 0x00, 0x03, 0x2A, 0xE0, 0xF1, 0x83, 0x25};
  RtpDepacketizer::ParsedPayload payload;

  ASSERT_TRUE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
  ExpectPacket(&payload, packet, sizeof(packet));
  EXPECT_EQ(kVideoFrameKey, payload.frame_type);
  EXPECT_EQ(kRtpVideoH264, payload.type.Video.codec);
  EXPECT_TRUE(payload.type.Video.is_first_packet_in_frame);
  EXPECT_EQ(kH264SingleNalu,
            payload.type.Video.codecHeader.H264.packetization_type);
  EXPECT_EQ(1280u, payload.type.Video.width);
  EXPECT_EQ(720u, payload.type.Video.height);
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

  RtpDepacketizer::ParsedPayload payload;
  ASSERT_TRUE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
  ExpectPacket(&payload, packet, sizeof(packet));
  EXPECT_EQ(kVideoFrameKey, payload.frame_type);
  EXPECT_EQ(kRtpVideoH264, payload.type.Video.codec);
  EXPECT_TRUE(payload.type.Video.is_first_packet_in_frame);
  const RTPVideoHeaderH264& h264 = payload.type.Video.codecHeader.H264;
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

  RtpDepacketizer::ParsedPayload payload;

  ASSERT_TRUE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
  ExpectPacket(&payload, packet, sizeof(packet));
  EXPECT_EQ(kVideoFrameKey, payload.frame_type);
  EXPECT_EQ(kRtpVideoH264, payload.type.Video.codec);
  EXPECT_TRUE(payload.type.Video.is_first_packet_in_frame);
  EXPECT_EQ(kH264StapA, payload.type.Video.codecHeader.H264.packetization_type);
  EXPECT_EQ(1280u, payload.type.Video.width);
  EXPECT_EQ(720u, payload.type.Video.height);
}

TEST_F(RtpDepacketizerH264Test, TestEmptyStapARejected) {
  uint8_t lone_empty_packet[] = {kStapA, 0x00, 0x00};

  uint8_t leading_empty_packet[] = {kStapA, 0x00, 0x00, 0x00, 0x04,
                                    kIdr,   0xFF, 0x00, 0x11};

  uint8_t middle_empty_packet[] = {kStapA, 0x00, 0x03, kIdr, 0xFF, 0x00, 0x00,
                                   0x00,   0x00, 0x04, kIdr, 0xFF, 0x00, 0x11};

  uint8_t trailing_empty_packet[] = {kStapA, 0x00, 0x03, kIdr,
                                     0xFF,   0x00, 0x00, 0x00};

  RtpDepacketizer::ParsedPayload payload;

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

  RtpDepacketizer::ParsedPayload payload;
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

  RtpDepacketizer::ParsedPayload payload;
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
                        0,      0x02, kSlice, 0xFF,   0,    0x03, kSlice, 0xFF,
                        0x00,   0,    0x04,   kSlice, 0xFF, 0x00, 0x11};
  RtpDepacketizer::ParsedPayload payload;

  ASSERT_TRUE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
  ExpectPacket(&payload, packet, sizeof(packet));
  EXPECT_EQ(kVideoFrameDelta, payload.frame_type);
  EXPECT_EQ(kRtpVideoH264, payload.type.Video.codec);
  EXPECT_TRUE(payload.type.Video.is_first_packet_in_frame);
  EXPECT_EQ(kH264StapA, payload.type.Video.codecHeader.H264.packetization_type);
  // NALU type for aggregated packets is the type of the first packet only.
  EXPECT_EQ(kSlice, payload.type.Video.codecHeader.H264.nalu_type);
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

  RtpDepacketizer::ParsedPayload payload;

  // We expect that the first packet is one byte shorter since the FU-A header
  // has been replaced by the original nal header.
  ASSERT_TRUE(depacketizer_->Parse(&payload, packet1, sizeof(packet1)));
  ExpectPacket(&payload, kExpected1, sizeof(kExpected1));
  EXPECT_EQ(kVideoFrameKey, payload.frame_type);
  EXPECT_EQ(kRtpVideoH264, payload.type.Video.codec);
  EXPECT_TRUE(payload.type.Video.is_first_packet_in_frame);
  const RTPVideoHeaderH264& h264 = payload.type.Video.codecHeader.H264;
  EXPECT_EQ(kH264FuA, h264.packetization_type);
  EXPECT_EQ(kIdr, h264.nalu_type);
  ASSERT_EQ(1u, h264.nalus_length);
  EXPECT_EQ(static_cast<H264::NaluType>(kIdr), h264.nalus[0].type);
  EXPECT_EQ(-1, h264.nalus[0].sps_id);
  EXPECT_EQ(0, h264.nalus[0].pps_id);

  // Following packets will be 2 bytes shorter since they will only be appended
  // onto the first packet.
  payload = RtpDepacketizer::ParsedPayload();
  ASSERT_TRUE(depacketizer_->Parse(&payload, packet2, sizeof(packet2)));
  ExpectPacket(&payload, kExpected2, sizeof(kExpected2));
  EXPECT_EQ(kVideoFrameKey, payload.frame_type);
  EXPECT_EQ(kRtpVideoH264, payload.type.Video.codec);
  EXPECT_FALSE(payload.type.Video.is_first_packet_in_frame);
  {
    const RTPVideoHeaderH264& h264 = payload.type.Video.codecHeader.H264;
    EXPECT_EQ(kH264FuA, h264.packetization_type);
    EXPECT_EQ(kIdr, h264.nalu_type);
    // NALU info is only expected for the first FU-A packet.
    EXPECT_EQ(0u, h264.nalus_length);
  }

  payload = RtpDepacketizer::ParsedPayload();
  ASSERT_TRUE(depacketizer_->Parse(&payload, packet3, sizeof(packet3)));
  ExpectPacket(&payload, kExpected3, sizeof(kExpected3));
  EXPECT_EQ(kVideoFrameKey, payload.frame_type);
  EXPECT_EQ(kRtpVideoH264, payload.type.Video.codec);
  EXPECT_FALSE(payload.type.Video.is_first_packet_in_frame);
  {
    const RTPVideoHeaderH264& h264 = payload.type.Video.codecHeader.H264;
    EXPECT_EQ(kH264FuA, h264.packetization_type);
    EXPECT_EQ(kIdr, h264.nalu_type);
    // NALU info is only expected for the first FU-A packet.
    ASSERT_EQ(0u, h264.nalus_length);
  }
}

TEST_F(RtpDepacketizerH264Test, TestEmptyPayload) {
  // Using a wild pointer to crash on accesses from inside the depacketizer.
  uint8_t* garbage_ptr = reinterpret_cast<uint8_t*>(0x4711);
  RtpDepacketizer::ParsedPayload payload;
  EXPECT_FALSE(depacketizer_->Parse(&payload, garbage_ptr, 0));
}

TEST_F(RtpDepacketizerH264Test, TestTruncatedFuaNalu) {
  const uint8_t kPayload[] = {0x9c};
  RtpDepacketizer::ParsedPayload payload;
  EXPECT_FALSE(depacketizer_->Parse(&payload, kPayload, sizeof(kPayload)));
}

TEST_F(RtpDepacketizerH264Test, TestTruncatedSingleStapANalu) {
  const uint8_t kPayload[] = {0xd8, 0x27};
  RtpDepacketizer::ParsedPayload payload;
  EXPECT_FALSE(depacketizer_->Parse(&payload, kPayload, sizeof(kPayload)));
}

TEST_F(RtpDepacketizerH264Test, TestStapAPacketWithTruncatedNalUnits) {
  const uint8_t kPayload[] = { 0x58, 0xCB, 0xED, 0xDF};
  RtpDepacketizer::ParsedPayload payload;
  EXPECT_FALSE(depacketizer_->Parse(&payload, kPayload, sizeof(kPayload)));
}

TEST_F(RtpDepacketizerH264Test, TestTruncationJustAfterSingleStapANalu) {
  const uint8_t kPayload[] = {0x38, 0x27, 0x27};
  RtpDepacketizer::ParsedPayload payload;
  EXPECT_FALSE(depacketizer_->Parse(&payload, kPayload, sizeof(kPayload)));
}

TEST_F(RtpDepacketizerH264Test, TestShortSpsPacket) {
  const uint8_t kPayload[] = {0x27, 0x80, 0x00};
  RtpDepacketizer::ParsedPayload payload;
  EXPECT_TRUE(depacketizer_->Parse(&payload, kPayload, sizeof(kPayload)));
}

TEST_F(RtpDepacketizerH264Test, TestSeiPacket) {
  const uint8_t kPayload[] = {
      kSei,  // F=0, NRI=0, Type=6.
      0x03, 0x03, 0x03, 0x03  // Payload.
  };
  RtpDepacketizer::ParsedPayload payload;
  ASSERT_TRUE(depacketizer_->Parse(&payload, kPayload, sizeof(kPayload)));
  const RTPVideoHeaderH264& h264 = payload.type.Video.codecHeader.H264;
  EXPECT_EQ(kVideoFrameDelta, payload.frame_type);
  EXPECT_EQ(kH264SingleNalu, h264.packetization_type);
  EXPECT_EQ(kSei, h264.nalu_type);
  ASSERT_EQ(1u, h264.nalus_length);
  EXPECT_EQ(static_cast<H264::NaluType>(kSei), h264.nalus[0].type);
  EXPECT_EQ(-1, h264.nalus[0].sps_id);
  EXPECT_EQ(-1, h264.nalus[0].pps_id);
}

}  // namespace webrtc
