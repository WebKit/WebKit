/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/ulpfec_generator.h"

#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/fec_test_helper.h"
#include "modules/rtp_rtcp/source/forward_error_correction.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
using test::fec::AugmentedPacket;
using test::fec::AugmentedPacketGenerator;

constexpr int kFecPayloadType = 96;
constexpr int kRedPayloadType = 97;
constexpr uint32_t kMediaSsrc = 835424;
}  // namespace

void VerifyHeader(uint16_t seq_num,
                  uint32_t timestamp,
                  int red_payload_type,
                  int fec_payload_type,
                  bool marker_bit,
                  const rtc::CopyOnWriteBuffer& data) {
  // Marker bit not set.
  EXPECT_EQ(marker_bit ? 0x80 : 0, data[1] & 0x80);
  EXPECT_EQ(red_payload_type, data[1] & 0x7F);
  EXPECT_EQ(seq_num, (data[2] << 8) + data[3]);
  uint32_t parsed_timestamp =
      (data[4] << 24) + (data[5] << 16) + (data[6] << 8) + data[7];
  EXPECT_EQ(timestamp, parsed_timestamp);
  EXPECT_EQ(static_cast<uint8_t>(fec_payload_type), data[kRtpHeaderSize]);
}

class UlpfecGeneratorTest : public ::testing::Test {
 protected:
  UlpfecGeneratorTest()
      : fake_clock_(1),
        ulpfec_generator_(kRedPayloadType, kFecPayloadType, &fake_clock_),
        packet_generator_(kMediaSsrc) {}

  SimulatedClock fake_clock_;
  UlpfecGenerator ulpfec_generator_;
  AugmentedPacketGenerator packet_generator_;
};

// Verifies bug found via fuzzing, where a gap in the packet sequence caused us
// to move past the end of the current FEC packet mask byte without moving to
// the next byte. That likely caused us to repeatedly read from the same byte,
// and if that byte didn't protect packets we would generate empty FEC.
TEST_F(UlpfecGeneratorTest, NoEmptyFecWithSeqNumGaps) {
  struct Packet {
    size_t header_size;
    size_t payload_size;
    uint16_t seq_num;
    bool marker_bit;
  };
  std::vector<Packet> protected_packets;
  protected_packets.push_back({15, 3, 41, 0});
  protected_packets.push_back({14, 1, 43, 0});
  protected_packets.push_back({19, 0, 48, 0});
  protected_packets.push_back({19, 0, 50, 0});
  protected_packets.push_back({14, 3, 51, 0});
  protected_packets.push_back({13, 8, 52, 0});
  protected_packets.push_back({19, 2, 53, 0});
  protected_packets.push_back({12, 3, 54, 0});
  protected_packets.push_back({21, 0, 55, 0});
  protected_packets.push_back({13, 3, 57, 1});
  FecProtectionParams params = {117, 3, kFecMaskBursty};
  ulpfec_generator_.SetProtectionParameters(params, params);
  for (Packet p : protected_packets) {
    RtpPacketToSend packet(nullptr);
    packet.SetMarker(p.marker_bit);
    packet.AllocateExtension(RTPExtensionType::kRtpExtensionMid,
                             p.header_size - packet.headers_size());
    packet.SetSequenceNumber(p.seq_num);
    packet.AllocatePayload(p.payload_size);
    ulpfec_generator_.AddPacketAndGenerateFec(packet);

    std::vector<std::unique_ptr<RtpPacketToSend>> fec_packets =
        ulpfec_generator_.GetFecPackets();
    if (!p.marker_bit) {
      EXPECT_TRUE(fec_packets.empty());
    } else {
      EXPECT_FALSE(fec_packets.empty());
    }
  }
}

TEST_F(UlpfecGeneratorTest, OneFrameFec) {
  // The number of media packets (|kNumPackets|), number of frames (one for
  // this test), and the protection factor (|params->fec_rate|) are set to make
  // sure the conditions for generating FEC are satisfied. This means:
  // (1) protection factor is high enough so that actual overhead over 1 frame
  // of packets is within |kMaxExcessOverhead|, and (2) the total number of
  // media packets for 1 frame is at least |minimum_media_packets_fec_|.
  constexpr size_t kNumPackets = 4;
  FecProtectionParams params = {15, 3, kFecMaskRandom};
  packet_generator_.NewFrame(kNumPackets);
  // Expecting one FEC packet.
  ulpfec_generator_.SetProtectionParameters(params, params);
  uint32_t last_timestamp = 0;
  for (size_t i = 0; i < kNumPackets; ++i) {
    std::unique_ptr<AugmentedPacket> packet =
        packet_generator_.NextPacket(i, 10);
    RtpPacketToSend rtp_packet(nullptr);
    EXPECT_TRUE(rtp_packet.Parse(packet->data.data(), packet->data.size()));
    ulpfec_generator_.AddPacketAndGenerateFec(rtp_packet);
    last_timestamp = packet->header.timestamp;
  }
  std::vector<std::unique_ptr<RtpPacketToSend>> fec_packets =
      ulpfec_generator_.GetFecPackets();
  EXPECT_EQ(fec_packets.size(), 1u);
  uint16_t seq_num = packet_generator_.NextPacketSeqNum();
  fec_packets[0]->SetSequenceNumber(seq_num);
  EXPECT_TRUE(ulpfec_generator_.GetFecPackets().empty());

  EXPECT_EQ(fec_packets[0]->headers_size(), kRtpHeaderSize);

  VerifyHeader(seq_num, last_timestamp, kRedPayloadType, kFecPayloadType, false,
               fec_packets[0]->Buffer());
}

TEST_F(UlpfecGeneratorTest, TwoFrameFec) {
  // The number of media packets/frame (|kNumPackets|), the number of frames
  // (|kNumFrames|), and the protection factor (|params->fec_rate|) are set to
  // make sure the conditions for generating FEC are satisfied. This means:
  // (1) protection factor is high enough so that actual overhead over
  // |kNumFrames| is within |kMaxExcessOverhead|, and (2) the total number of
  // media packets for |kNumFrames| frames is at least
  // |minimum_media_packets_fec_|.
  constexpr size_t kNumPackets = 2;
  constexpr size_t kNumFrames = 2;

  FecProtectionParams params = {15, 3, kFecMaskRandom};
  // Expecting one FEC packet.
  ulpfec_generator_.SetProtectionParameters(params, params);
  uint32_t last_timestamp = 0;
  for (size_t i = 0; i < kNumFrames; ++i) {
    packet_generator_.NewFrame(kNumPackets);
    for (size_t j = 0; j < kNumPackets; ++j) {
      std::unique_ptr<AugmentedPacket> packet =
          packet_generator_.NextPacket(i * kNumPackets + j, 10);
      RtpPacketToSend rtp_packet(nullptr);
      EXPECT_TRUE(rtp_packet.Parse(packet->data.data(), packet->data.size()));
      ulpfec_generator_.AddPacketAndGenerateFec(rtp_packet);
      last_timestamp = packet->header.timestamp;
    }
  }
  std::vector<std::unique_ptr<RtpPacketToSend>> fec_packets =
      ulpfec_generator_.GetFecPackets();
  EXPECT_EQ(fec_packets.size(), 1u);
  const uint16_t seq_num = packet_generator_.NextPacketSeqNum();
  fec_packets[0]->SetSequenceNumber(seq_num);
  VerifyHeader(seq_num, last_timestamp, kRedPayloadType, kFecPayloadType, false,
               fec_packets[0]->Buffer());
}

TEST_F(UlpfecGeneratorTest, MixedMediaRtpHeaderLengths) {
  constexpr size_t kShortRtpHeaderLength = 12;
  constexpr size_t kLongRtpHeaderLength = 16;

  // Only one frame required to generate FEC.
  FecProtectionParams params = {127, 1, kFecMaskRandom};
  ulpfec_generator_.SetProtectionParameters(params, params);

  // Fill up internal buffer with media packets with short RTP header length.
  packet_generator_.NewFrame(kUlpfecMaxMediaPackets + 1);
  for (size_t i = 0; i < kUlpfecMaxMediaPackets; ++i) {
    std::unique_ptr<AugmentedPacket> packet =
        packet_generator_.NextPacket(i, 10);
    RtpPacketToSend rtp_packet(nullptr);
    EXPECT_TRUE(rtp_packet.Parse(packet->data.data(), packet->data.size()));
    EXPECT_EQ(rtp_packet.headers_size(), kShortRtpHeaderLength);
    ulpfec_generator_.AddPacketAndGenerateFec(rtp_packet);
    EXPECT_TRUE(ulpfec_generator_.GetFecPackets().empty());
  }

  // Kick off FEC generation with media packet with long RTP header length.
  // Since the internal buffer is full, this packet will not be protected.
  std::unique_ptr<AugmentedPacket> packet =
      packet_generator_.NextPacket(kUlpfecMaxMediaPackets, 10);
  RtpPacketToSend rtp_packet(nullptr);
  EXPECT_TRUE(rtp_packet.Parse(packet->data.data(), packet->data.size()));
  EXPECT_TRUE(rtp_packet.SetPayloadSize(0) != nullptr);
  const uint32_t csrcs[]{1};
  rtp_packet.SetCsrcs(csrcs);

  EXPECT_EQ(rtp_packet.headers_size(), kLongRtpHeaderLength);

  ulpfec_generator_.AddPacketAndGenerateFec(rtp_packet);
  std::vector<std::unique_ptr<RtpPacketToSend>> fec_packets =
      ulpfec_generator_.GetFecPackets();
  EXPECT_FALSE(fec_packets.empty());

  // Ensure that the RED header is placed correctly, i.e. the correct
  // RTP header length was used in the RED packet creation.
  uint16_t seq_num = packet_generator_.NextPacketSeqNum();
  for (const auto& fec_packet : fec_packets) {
    fec_packet->SetSequenceNumber(seq_num++);
    EXPECT_EQ(kFecPayloadType, fec_packet->data()[kShortRtpHeaderLength]);
  }
}

}  // namespace webrtc
