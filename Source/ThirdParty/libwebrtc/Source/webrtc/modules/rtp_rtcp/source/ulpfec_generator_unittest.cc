/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/fec_test_helper.h"
#include "modules/rtp_rtcp/source/forward_error_correction.h"
#include "modules/rtp_rtcp/source/ulpfec_generator.h"
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
                  RedPacket* packet,
                  bool marker_bit) {
  EXPECT_GT(packet->length(), kRtpHeaderSize);
  EXPECT_TRUE(packet->data() != NULL);
  uint8_t* data = packet->data();
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
  UlpfecGeneratorTest() : packet_generator_(kMediaSsrc) {}

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
  ulpfec_generator_.SetFecParameters(params);
  uint8_t packet[28] = {0};
  for (Packet p : protected_packets) {
    if (p.marker_bit) {
      packet[1] |= 0x80;
    } else {
      packet[1] &= ~0x80;
    }
    ByteWriter<uint16_t>::WriteBigEndian(&packet[2], p.seq_num);
    ulpfec_generator_.AddRtpPacketAndGenerateFec(packet, p.payload_size,
                                                 p.header_size);
    size_t num_fec_packets = ulpfec_generator_.NumAvailableFecPackets();
    if (num_fec_packets > 0) {
      std::vector<std::unique_ptr<RedPacket>> fec_packets =
          ulpfec_generator_.GetUlpfecPacketsAsRed(kRedPayloadType,
                                                  kFecPayloadType, 100);
      EXPECT_EQ(num_fec_packets, fec_packets.size());
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
  ulpfec_generator_.SetFecParameters(params);  // Expecting one FEC packet.
  uint32_t last_timestamp = 0;
  for (size_t i = 0; i < kNumPackets; ++i) {
    std::unique_ptr<AugmentedPacket> packet =
        packet_generator_.NextPacket(i, 10);
    EXPECT_EQ(0, ulpfec_generator_.AddRtpPacketAndGenerateFec(
                     packet->data, packet->length, kRtpHeaderSize));
    last_timestamp = packet->header.header.timestamp;
  }
  EXPECT_TRUE(ulpfec_generator_.FecAvailable());
  const uint16_t seq_num = packet_generator_.NextPacketSeqNum();
  std::vector<std::unique_ptr<RedPacket>> red_packets =
      ulpfec_generator_.GetUlpfecPacketsAsRed(kRedPayloadType, kFecPayloadType,
                                              seq_num);
  EXPECT_FALSE(ulpfec_generator_.FecAvailable());
  ASSERT_EQ(1u, red_packets.size());
  VerifyHeader(seq_num, last_timestamp, kRedPayloadType, kFecPayloadType,
               red_packets.front().get(), false);
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
  ulpfec_generator_.SetFecParameters(params);  // Expecting one FEC packet.
  uint32_t last_timestamp = 0;
  for (size_t i = 0; i < kNumFrames; ++i) {
    packet_generator_.NewFrame(kNumPackets);
    for (size_t j = 0; j < kNumPackets; ++j) {
      std::unique_ptr<AugmentedPacket> packet =
          packet_generator_.NextPacket(i * kNumPackets + j, 10);
      EXPECT_EQ(0, ulpfec_generator_.AddRtpPacketAndGenerateFec(
                       packet->data, packet->length, kRtpHeaderSize));
      last_timestamp = packet->header.header.timestamp;
    }
  }
  EXPECT_TRUE(ulpfec_generator_.FecAvailable());
  const uint16_t seq_num = packet_generator_.NextPacketSeqNum();
  std::vector<std::unique_ptr<RedPacket>> red_packets =
      ulpfec_generator_.GetUlpfecPacketsAsRed(kRedPayloadType, kFecPayloadType,
                                              seq_num);
  EXPECT_FALSE(ulpfec_generator_.FecAvailable());
  ASSERT_EQ(1u, red_packets.size());
  VerifyHeader(seq_num, last_timestamp, kRedPayloadType, kFecPayloadType,
               red_packets.front().get(), false);
}

TEST_F(UlpfecGeneratorTest, MixedMediaRtpHeaderLengths) {
  constexpr size_t kShortRtpHeaderLength = 12;
  constexpr size_t kLongRtpHeaderLength = 16;

  // Only one frame required to generate FEC.
  FecProtectionParams params = {127, 1, kFecMaskRandom};
  ulpfec_generator_.SetFecParameters(params);

  // Fill up internal buffer with media packets with short RTP header length.
  packet_generator_.NewFrame(kUlpfecMaxMediaPackets + 1);
  for (size_t i = 0; i < kUlpfecMaxMediaPackets; ++i) {
    std::unique_ptr<AugmentedPacket> packet =
        packet_generator_.NextPacket(i, 10);
    EXPECT_EQ(0, ulpfec_generator_.AddRtpPacketAndGenerateFec(
                     packet->data, packet->length, kShortRtpHeaderLength));
    EXPECT_FALSE(ulpfec_generator_.FecAvailable());
  }

  // Kick off FEC generation with media packet with long RTP header length.
  // Since the internal buffer is full, this packet will not be protected.
  std::unique_ptr<AugmentedPacket> packet =
      packet_generator_.NextPacket(kUlpfecMaxMediaPackets, 10);
  EXPECT_EQ(0, ulpfec_generator_.AddRtpPacketAndGenerateFec(
                   packet->data, packet->length, kLongRtpHeaderLength));
  EXPECT_TRUE(ulpfec_generator_.FecAvailable());

  // Ensure that the RED header is placed correctly, i.e. the correct
  // RTP header length was used in the RED packet creation.
  const uint16_t seq_num = packet_generator_.NextPacketSeqNum();
  std::vector<std::unique_ptr<RedPacket>> red_packets =
      ulpfec_generator_.GetUlpfecPacketsAsRed(kRedPayloadType, kFecPayloadType,
                                              seq_num);
  for (const auto& red_packet : red_packets) {
    EXPECT_EQ(kFecPayloadType, red_packet->data()[kShortRtpHeaderLength]);
  }
}

}  // namespace webrtc
