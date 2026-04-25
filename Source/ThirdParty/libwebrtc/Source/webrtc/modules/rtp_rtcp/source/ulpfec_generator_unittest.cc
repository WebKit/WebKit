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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "modules/include/module_fec_types.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/fec_test_helper.h"
#include "modules/rtp_rtcp/source/forward_error_correction_internal.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "system_wrappers/include/clock.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
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
                  const CopyOnWriteBuffer& data) {
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
      : env_(CreateEnvironment(std::make_unique<SimulatedClock>(1))),
        ulpfec_generator_(env_, kRedPayloadType, kFecPayloadType),
        packet_generator_(kMediaSsrc) {}

  const Environment env_;
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
  protected_packets.push_back(
      {.header_size = 15, .payload_size = 3, .seq_num = 41, .marker_bit = 0});
  protected_packets.push_back(
      {.header_size = 14, .payload_size = 1, .seq_num = 43, .marker_bit = 0});
  protected_packets.push_back(
      {.header_size = 19, .payload_size = 0, .seq_num = 48, .marker_bit = 0});
  protected_packets.push_back(
      {.header_size = 19, .payload_size = 0, .seq_num = 50, .marker_bit = 0});
  protected_packets.push_back(
      {.header_size = 14, .payload_size = 3, .seq_num = 51, .marker_bit = 0});
  protected_packets.push_back(
      {.header_size = 13, .payload_size = 8, .seq_num = 52, .marker_bit = 0});
  protected_packets.push_back(
      {.header_size = 19, .payload_size = 2, .seq_num = 53, .marker_bit = 0});
  protected_packets.push_back(
      {.header_size = 12, .payload_size = 3, .seq_num = 54, .marker_bit = 0});
  protected_packets.push_back(
      {.header_size = 21, .payload_size = 0, .seq_num = 55, .marker_bit = 0});
  protected_packets.push_back(
      {.header_size = 13, .payload_size = 3, .seq_num = 57, .marker_bit = 1});
  FecProtectionParams params = {
      .fec_rate = 117, .max_fec_frames = 3, .fec_mask_type = kFecMaskBursty};
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
  // The number of media packets (`kNumPackets`), number of frames (one for
  // this test), and the protection factor (|params->fec_rate|) are set to make
  // sure the conditions for generating FEC are satisfied. This means:
  // (1) protection factor is high enough so that actual overhead over 1 frame
  // of packets is within `kMaxExcessOverhead`, and (2) the total number of
  // media packets for 1 frame is at least `minimum_media_packets_fec_`.
  constexpr size_t kNumPackets = 4;
  FecProtectionParams params = {
      .fec_rate = 15, .max_fec_frames = 3, .fec_mask_type = kFecMaskRandom};
  packet_generator_.NewFrame(kNumPackets);
  // Expecting one FEC packet.
  ulpfec_generator_.SetProtectionParameters(params, params);
  uint32_t last_timestamp = 0;
  for (size_t i = 0; i < kNumPackets; ++i) {
    RtpPacketToSend rtp_packet =
        packet_generator_.NextPacket<RtpPacketToSend>(i, 10);
    ulpfec_generator_.AddPacketAndGenerateFec(rtp_packet);
    last_timestamp = rtp_packet.Timestamp();
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
  // The number of media packets/frame (`kNumPackets`), the number of frames
  // (`kNumFrames`), and the protection factor (|params->fec_rate|) are set to
  // make sure the conditions for generating FEC are satisfied. This means:
  // (1) protection factor is high enough so that actual overhead over
  // `kNumFrames` is within `kMaxExcessOverhead`, and (2) the total number of
  // media packets for `kNumFrames` frames is at least
  // `minimum_media_packets_fec_`.
  constexpr size_t kNumPackets = 2;
  constexpr size_t kNumFrames = 2;

  FecProtectionParams params = {
      .fec_rate = 15, .max_fec_frames = 3, .fec_mask_type = kFecMaskRandom};
  // Expecting one FEC packet.
  ulpfec_generator_.SetProtectionParameters(params, params);
  uint32_t last_timestamp = 0;
  for (size_t i = 0; i < kNumFrames; ++i) {
    packet_generator_.NewFrame(kNumPackets);
    for (size_t j = 0; j < kNumPackets; ++j) {
      RtpPacketToSend rtp_packet =
          packet_generator_.NextPacket<RtpPacketToSend>(i * kNumPackets + j,
                                                        10);
      ulpfec_generator_.AddPacketAndGenerateFec(rtp_packet);
      last_timestamp = rtp_packet.Timestamp();
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
  FecProtectionParams params = {
      .fec_rate = 127, .max_fec_frames = 1, .fec_mask_type = kFecMaskRandom};
  ulpfec_generator_.SetProtectionParameters(params, params);

  // Fill up internal buffer with media packets with short RTP header length.
  packet_generator_.NewFrame(kUlpfecMaxMediaPackets + 1);
  for (size_t i = 0; i < kUlpfecMaxMediaPackets; ++i) {
    RtpPacketToSend rtp_packet =
        packet_generator_.NextPacket<RtpPacketToSend>(i, 10);
    EXPECT_EQ(rtp_packet.headers_size(), kShortRtpHeaderLength);
    ulpfec_generator_.AddPacketAndGenerateFec(rtp_packet);
    EXPECT_TRUE(ulpfec_generator_.GetFecPackets().empty());
  }

  // Kick off FEC generation with media packet with long RTP header length.
  // Since the internal buffer is full, this packet will not be protected.
  RtpPacketToSend rtp_packet =
      packet_generator_.NextPacket<RtpPacketToSend>(kUlpfecMaxMediaPackets, 10);
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

TEST_F(UlpfecGeneratorTest, UpdatesProtectionParameters) {
  const FecProtectionParams kKeyFrameParams = {
      .fec_rate = 25, .max_fec_frames = 2, .fec_mask_type = kFecMaskRandom};
  const FecProtectionParams kDeltaFrameParams = {
      .fec_rate = 25, .max_fec_frames = 5, .fec_mask_type = kFecMaskRandom};

  ulpfec_generator_.SetProtectionParameters(kDeltaFrameParams, kKeyFrameParams);

  // No params applied yet.
  EXPECT_EQ(ulpfec_generator_.CurrentParams().max_fec_frames, 0);

  // Helper function to add a single-packet frame market as either key-frame
  // or delta-frame.
  auto add_frame = [&](bool is_keyframe) {
    packet_generator_.NewFrame(1);
    RtpPacketToSend rtp_packet =
        packet_generator_.NextPacket<RtpPacketToSend>(0, 10);
    rtp_packet.set_is_key_frame(is_keyframe);
    ulpfec_generator_.AddPacketAndGenerateFec(rtp_packet);
  };

  // Add key-frame, keyframe params should apply, no FEC generated yet.
  add_frame(true);
  EXPECT_EQ(ulpfec_generator_.CurrentParams().max_fec_frames, 2);
  EXPECT_TRUE(ulpfec_generator_.GetFecPackets().empty());

  // Add delta-frame, generated FEC packet. Params will not be updated until
  // next added packet though.
  add_frame(false);
  EXPECT_EQ(ulpfec_generator_.CurrentParams().max_fec_frames, 2);
  EXPECT_FALSE(ulpfec_generator_.GetFecPackets().empty());

  // Add delta-frame, now params get updated.
  add_frame(false);
  EXPECT_EQ(ulpfec_generator_.CurrentParams().max_fec_frames, 5);
  EXPECT_TRUE(ulpfec_generator_.GetFecPackets().empty());

  // Add yet another delta-frame.
  add_frame(false);
  EXPECT_EQ(ulpfec_generator_.CurrentParams().max_fec_frames, 5);
  EXPECT_TRUE(ulpfec_generator_.GetFecPackets().empty());

  // Add key-frame, params immediately switch to key-frame ones. The two
  // buffered frames plus the key-frame is protected and fec emitted,
  // even though the frame count is technically over the keyframe frame count
  // threshold.
  add_frame(true);
  EXPECT_EQ(ulpfec_generator_.CurrentParams().max_fec_frames, 2);
  EXPECT_FALSE(ulpfec_generator_.GetFecPackets().empty());
}

}  // namespace webrtc
