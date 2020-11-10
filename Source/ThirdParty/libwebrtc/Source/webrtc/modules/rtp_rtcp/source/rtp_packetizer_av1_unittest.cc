/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_packetizer_av1.h"

#include <stddef.h>
#include <stdint.h>

#include <initializer_list>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "api/scoped_refptr.h"
#include "api/video/encoded_image.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer_av1.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Le;
using ::testing::SizeIs;

constexpr uint8_t kNewCodedVideoSequenceBit = 0b00'00'1000;
// All obu types offset by 3 to take correct position in the obu_header.
constexpr uint8_t kObuTypeSequenceHeader = 1 << 3;
constexpr uint8_t kObuTypeTemporalDelimiter = 2 << 3;
constexpr uint8_t kObuTypeFrameHeader = 3 << 3;
constexpr uint8_t kObuTypeTileGroup = 4 << 3;
constexpr uint8_t kObuTypeMetadata = 5 << 3;
constexpr uint8_t kObuTypeFrame = 6 << 3;
constexpr uint8_t kObuTypeTileList = 8 << 3;
constexpr uint8_t kObuExtensionPresentBit = 0b0'0000'100;
constexpr uint8_t kObuSizePresentBit = 0b0'0000'010;
constexpr uint8_t kObuExtensionS1T1 = 0b001'01'000;

// Wrapper around rtp_packet to make it look like container of payload bytes.
struct RtpPayload {
  using value_type = rtc::ArrayView<const uint8_t>::value_type;
  using const_iterator = rtc::ArrayView<const uint8_t>::const_iterator;

  RtpPayload() : rtp_packet(/*extensions=*/nullptr) {}
  RtpPayload& operator=(RtpPayload&&) = default;
  RtpPayload(RtpPayload&&) = default;

  const_iterator begin() const { return rtp_packet.payload().begin(); }
  const_iterator end() const { return rtp_packet.payload().end(); }
  const uint8_t* data() const { return rtp_packet.payload().data(); }
  size_t size() const { return rtp_packet.payload().size(); }

  uint8_t aggregation_header() const { return rtp_packet.payload()[0]; }

  RtpPacketToSend rtp_packet;
};

// Wrapper around frame pointer to make it look like container of bytes with
// nullptr frame look like empty container.
class Av1Frame {
 public:
  using value_type = uint8_t;
  using const_iterator = const uint8_t*;

  explicit Av1Frame(rtc::scoped_refptr<EncodedImageBuffer> frame)
      : frame_(std::move(frame)) {}

  const_iterator begin() const { return frame_ ? frame_->data() : nullptr; }
  const_iterator end() const {
    return frame_ ? (frame_->data() + frame_->size()) : nullptr;
  }

 private:
  rtc::scoped_refptr<EncodedImageBuffer> frame_;
};

std::vector<RtpPayload> Packetize(
    rtc::ArrayView<const uint8_t> payload,
    RtpPacketizer::PayloadSizeLimits limits,
    VideoFrameType frame_type = VideoFrameType::kVideoFrameDelta) {
  // Run code under test.
  RtpPacketizerAv1 packetizer(payload, limits, frame_type);
  // Convert result into structure that is easier to run expectation against.
  std::vector<RtpPayload> result(packetizer.NumPackets());
  for (RtpPayload& rtp_payload : result) {
    EXPECT_TRUE(packetizer.NextPacket(&rtp_payload.rtp_packet));
  }
  return result;
}

Av1Frame ReassembleFrame(rtc::ArrayView<const RtpPayload> rtp_payloads) {
  std::vector<rtc::ArrayView<const uint8_t>> payloads(rtp_payloads.size());
  for (size_t i = 0; i < rtp_payloads.size(); ++i) {
    payloads[i] = rtp_payloads[i];
  }
  return Av1Frame(VideoRtpDepacketizerAv1().AssembleFrame(payloads));
}

class Obu {
 public:
  explicit Obu(uint8_t obu_type) : header_(obu_type | kObuSizePresentBit) {
    EXPECT_EQ(obu_type & 0b0'1111'000, obu_type);
  }

  Obu& WithExtension(uint8_t extension) {
    extension_ = extension;
    header_ |= kObuExtensionPresentBit;
    return *this;
  }
  Obu& WithoutSize() {
    header_ &= ~kObuSizePresentBit;
    return *this;
  }
  Obu& WithPayload(std::vector<uint8_t> payload) {
    payload_ = std::move(payload);
    return *this;
  }

 private:
  friend std::vector<uint8_t> BuildAv1Frame(std::initializer_list<Obu> obus);
  uint8_t header_;
  uint8_t extension_ = 0;
  std::vector<uint8_t> payload_;
};

std::vector<uint8_t> BuildAv1Frame(std::initializer_list<Obu> obus) {
  std::vector<uint8_t> raw;
  for (const Obu& obu : obus) {
    raw.push_back(obu.header_);
    if (obu.header_ & kObuExtensionPresentBit) {
      raw.push_back(obu.extension_);
    }
    if (obu.header_ & kObuSizePresentBit) {
      // write size in leb128 format.
      size_t payload_size = obu.payload_.size();
      while (payload_size >= 0x80) {
        raw.push_back(0x80 | (payload_size & 0x7F));
        payload_size >>= 7;
      }
      raw.push_back(payload_size);
    }
    raw.insert(raw.end(), obu.payload_.begin(), obu.payload_.end());
  }
  return raw;
}

TEST(RtpPacketizerAv1Test, PacketizeOneObuWithoutSizeAndExtension) {
  auto kFrame = BuildAv1Frame(
      {Obu(kObuTypeFrame).WithoutSize().WithPayload({1, 2, 3, 4, 5, 6, 7})});
  EXPECT_THAT(Packetize(kFrame, {}),
              ElementsAre(ElementsAre(0b00'01'0000,  // aggregation header
                                      kObuTypeFrame, 1, 2, 3, 4, 5, 6, 7)));
}

TEST(RtpPacketizerAv1Test, PacketizeOneObuWithoutSizeWithExtension) {
  auto kFrame = BuildAv1Frame({Obu(kObuTypeFrame)
                                   .WithoutSize()
                                   .WithExtension(kObuExtensionS1T1)
                                   .WithPayload({2, 3, 4, 5, 6, 7})});
  EXPECT_THAT(Packetize(kFrame, {}),
              ElementsAre(ElementsAre(0b00'01'0000,  // aggregation header
                                      kObuTypeFrame | kObuExtensionPresentBit,
                                      kObuExtensionS1T1, 2, 3, 4, 5, 6, 7)));
}

TEST(RtpPacketizerAv1Test, RemovesObuSizeFieldWithoutExtension) {
  auto kFrame = BuildAv1Frame(
      {Obu(kObuTypeFrame).WithPayload({11, 12, 13, 14, 15, 16, 17})});
  EXPECT_THAT(
      Packetize(kFrame, {}),
      ElementsAre(ElementsAre(0b00'01'0000,  // aggregation header
                              kObuTypeFrame, 11, 12, 13, 14, 15, 16, 17)));
}

TEST(RtpPacketizerAv1Test, RemovesObuSizeFieldWithExtension) {
  auto kFrame = BuildAv1Frame({Obu(kObuTypeFrame)
                                   .WithExtension(kObuExtensionS1T1)
                                   .WithPayload({1, 2, 3, 4, 5, 6, 7})});
  EXPECT_THAT(Packetize(kFrame, {}),
              ElementsAre(ElementsAre(0b00'01'0000,  // aggregation header
                                      kObuTypeFrame | kObuExtensionPresentBit,
                                      kObuExtensionS1T1, 1, 2, 3, 4, 5, 6, 7)));
}

TEST(RtpPacketizerAv1Test, OmitsSizeForLastObuWhenThreeObusFitsIntoThePacket) {
  auto kFrame = BuildAv1Frame(
      {Obu(kObuTypeSequenceHeader).WithPayload({1, 2, 3, 4, 5, 6}),
       Obu(kObuTypeMetadata).WithPayload({11, 12, 13, 14}),
       Obu(kObuTypeFrame).WithPayload({21, 22, 23, 24, 25, 26})});
  EXPECT_THAT(
      Packetize(kFrame, {}),
      ElementsAre(ElementsAre(0b00'11'0000,  // aggregation header
                              7, kObuTypeSequenceHeader, 1, 2, 3, 4, 5, 6,  //
                              5, kObuTypeMetadata, 11, 12, 13, 14,          //
                              kObuTypeFrame, 21, 22, 23, 24, 25, 26)));
}

TEST(RtpPacketizerAv1Test, UseSizeForAllObusWhenFourObusFitsIntoThePacket) {
  auto kFrame = BuildAv1Frame(
      {Obu(kObuTypeSequenceHeader).WithPayload({1, 2, 3, 4, 5, 6}),
       Obu(kObuTypeMetadata).WithPayload({11, 12, 13, 14}),
       Obu(kObuTypeFrameHeader).WithPayload({21, 22, 23}),
       Obu(kObuTypeTileGroup).WithPayload({31, 32, 33, 34, 35, 36})});
  EXPECT_THAT(
      Packetize(kFrame, {}),
      ElementsAre(ElementsAre(0b00'00'0000,  // aggregation header
                              7, kObuTypeSequenceHeader, 1, 2, 3, 4, 5, 6,  //
                              5, kObuTypeMetadata, 11, 12, 13, 14,          //
                              4, kObuTypeFrameHeader, 21, 22, 23,           //
                              7, kObuTypeTileGroup, 31, 32, 33, 34, 35, 36)));
}

TEST(RtpPacketizerAv1Test, DiscardsTemporalDelimiterAndTileListObu) {
  auto kFrame = BuildAv1Frame(
      {Obu(kObuTypeTemporalDelimiter), Obu(kObuTypeMetadata),
       Obu(kObuTypeTileList).WithPayload({1, 2, 3, 4, 5, 6}),
       Obu(kObuTypeFrameHeader).WithPayload({21, 22, 23}),
       Obu(kObuTypeTileGroup).WithPayload({31, 32, 33, 34, 35, 36})});

  EXPECT_THAT(
      Packetize(kFrame, {}),
      ElementsAre(ElementsAre(0b00'11'0000,  // aggregation header
                              1,
                              kObuTypeMetadata,  //
                              4, kObuTypeFrameHeader, 21, 22,
                              23,  //
                              kObuTypeTileGroup, 31, 32, 33, 34, 35, 36)));
}

TEST(RtpPacketizerAv1Test, SplitTwoObusIntoTwoPacketForceSplitObuHeader) {
  // Craft expected payloads so that there is only one way to split original
  // frame into two packets.
  const uint8_t kExpectPayload1[6] = {
      0b01'10'0000,  // aggregation_header
      3,
      kObuTypeFrameHeader | kObuExtensionPresentBit,
      kObuExtensionS1T1,
      21,  //
      kObuTypeTileGroup | kObuExtensionPresentBit};
  const uint8_t kExpectPayload2[6] = {0b10'01'0000,  // aggregation_header
                                      kObuExtensionS1T1, 11, 12, 13, 14};
  auto kFrame = BuildAv1Frame({Obu(kObuTypeFrameHeader)
                                   .WithExtension(kObuExtensionS1T1)
                                   .WithPayload({21}),
                               Obu(kObuTypeTileGroup)
                                   .WithExtension(kObuExtensionS1T1)
                                   .WithPayload({11, 12, 13, 14})});

  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 6;
  auto payloads = Packetize(kFrame, limits);
  EXPECT_THAT(payloads, ElementsAre(ElementsAreArray(kExpectPayload1),
                                    ElementsAreArray(kExpectPayload2)));
}

TEST(RtpPacketizerAv1Test,
     SetsNbitAtTheFirstPacketOfAKeyFrameWithSequenceHeader) {
  auto kFrame = BuildAv1Frame(
      {Obu(kObuTypeSequenceHeader).WithPayload({1, 2, 3, 4, 5, 6, 7})});
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 6;
  auto packets = Packetize(kFrame, limits, VideoFrameType::kVideoFrameKey);
  ASSERT_THAT(packets, SizeIs(2));
  EXPECT_TRUE(packets[0].aggregation_header() & kNewCodedVideoSequenceBit);
  EXPECT_FALSE(packets[1].aggregation_header() & kNewCodedVideoSequenceBit);
}

TEST(RtpPacketizerAv1Test,
     DoesntSetNbitAtThePacketsOfAKeyFrameWithoutSequenceHeader) {
  auto kFrame =
      BuildAv1Frame({Obu(kObuTypeFrame).WithPayload({1, 2, 3, 4, 5, 6, 7})});
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 6;
  auto packets = Packetize(kFrame, limits, VideoFrameType::kVideoFrameKey);
  ASSERT_THAT(packets, SizeIs(2));
  EXPECT_FALSE(packets[0].aggregation_header() & kNewCodedVideoSequenceBit);
  EXPECT_FALSE(packets[1].aggregation_header() & kNewCodedVideoSequenceBit);
}

TEST(RtpPacketizerAv1Test, DoesntSetNbitAtThePacketsOfADeltaFrame) {
  // Even when that delta frame starts with a (redundant) sequence header.
  auto kFrame = BuildAv1Frame(
      {Obu(kObuTypeSequenceHeader).WithPayload({1, 2, 3, 4, 5, 6, 7})});
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 6;
  auto packets = Packetize(kFrame, limits, VideoFrameType::kVideoFrameDelta);
  ASSERT_THAT(packets, SizeIs(2));
  EXPECT_FALSE(packets[0].aggregation_header() & kNewCodedVideoSequenceBit);
  EXPECT_FALSE(packets[1].aggregation_header() & kNewCodedVideoSequenceBit);
}

// There are multiple valid reasonable ways to split payload into multiple
// packets, do not validate current choice, instead use RtpDepacketizer
// to validate frame is reconstracted to the same one. Note: since
// RtpDepacketizer always inserts obu_size fields in the output, use frame where
// each obu has obu_size fields for more streight forward validation.
TEST(RtpPacketizerAv1Test, SplitSingleObuIntoTwoPackets) {
  auto kFrame = BuildAv1Frame(
      {Obu(kObuTypeFrame).WithPayload({11, 12, 13, 14, 15, 16, 17, 18, 19})});

  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 8;
  auto payloads = Packetize(kFrame, limits);
  EXPECT_THAT(payloads, ElementsAre(SizeIs(Le(8u)), SizeIs(Le(8u))));

  // Use RtpDepacketizer to validate the split.
  EXPECT_THAT(ReassembleFrame(payloads), ElementsAreArray(kFrame));
}

TEST(RtpPacketizerAv1Test, SplitSingleObuIntoManyPackets) {
  auto kFrame = BuildAv1Frame(
      {Obu(kObuTypeFrame).WithPayload(std::vector<uint8_t>(1200, 27))});

  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 100;
  auto payloads = Packetize(kFrame, limits);
  EXPECT_THAT(payloads, SizeIs(13u));
  EXPECT_THAT(payloads, Each(SizeIs(Le(100u))));

  // Use RtpDepacketizer to validate the split.
  EXPECT_THAT(ReassembleFrame(payloads), ElementsAreArray(kFrame));
}

TEST(RtpPacketizerAv1Test, SplitTwoObusIntoTwoPackets) {
  // 2nd OBU is too large to fit into one packet, so its head would be in the
  // same packet as the 1st OBU.
  auto kFrame = BuildAv1Frame(
      {Obu(kObuTypeSequenceHeader).WithPayload({11, 12}),
       Obu(kObuTypeFrame).WithPayload({1, 2, 3, 4, 5, 6, 7, 8, 9})});

  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 8;
  auto payloads = Packetize(kFrame, limits);
  EXPECT_THAT(payloads, ElementsAre(SizeIs(Le(8u)), SizeIs(Le(8u))));

  // Use RtpDepacketizer to validate the split.
  EXPECT_THAT(ReassembleFrame(payloads), ElementsAreArray(kFrame));
}

TEST(RtpPacketizerAv1Test,
     SplitSingleObuIntoTwoPacketsBecauseOfSinglePacketLimit) {
  auto kFrame = BuildAv1Frame(
      {Obu(kObuTypeFrame).WithPayload({11, 12, 13, 14, 15, 16, 17, 18, 19})});
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 10;
  limits.single_packet_reduction_len = 8;
  auto payloads = Packetize(kFrame, limits);
  EXPECT_THAT(payloads, ElementsAre(SizeIs(Le(10u)), SizeIs(Le(10u))));

  EXPECT_THAT(ReassembleFrame(payloads), ElementsAreArray(kFrame));
}

}  // namespace
}  // namespace webrtc
