/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <vector>

#include "modules/rtp_rtcp/source/rtp_format_vp9.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {
namespace {
void VerifyHeader(const RTPVideoHeaderVP9& expected,
                  const RTPVideoHeaderVP9& actual) {
  EXPECT_EQ(expected.inter_layer_predicted, actual.inter_layer_predicted);
  EXPECT_EQ(expected.inter_pic_predicted, actual.inter_pic_predicted);
  EXPECT_EQ(expected.flexible_mode, actual.flexible_mode);
  EXPECT_EQ(expected.beginning_of_frame, actual.beginning_of_frame);
  EXPECT_EQ(expected.end_of_frame, actual.end_of_frame);
  EXPECT_EQ(expected.ss_data_available, actual.ss_data_available);
  EXPECT_EQ(expected.picture_id, actual.picture_id);
  EXPECT_EQ(expected.max_picture_id, actual.max_picture_id);
  EXPECT_EQ(expected.temporal_idx, actual.temporal_idx);
  EXPECT_EQ(expected.spatial_idx, actual.spatial_idx);
  EXPECT_EQ(expected.gof_idx, actual.gof_idx);
  EXPECT_EQ(expected.tl0_pic_idx, actual.tl0_pic_idx);
  EXPECT_EQ(expected.temporal_up_switch, actual.temporal_up_switch);

  EXPECT_EQ(expected.num_ref_pics, actual.num_ref_pics);
  for (uint8_t i = 0; i < expected.num_ref_pics; ++i) {
    EXPECT_EQ(expected.pid_diff[i], actual.pid_diff[i]);
    EXPECT_EQ(expected.ref_picture_id[i], actual.ref_picture_id[i]);
  }
  if (expected.ss_data_available) {
    EXPECT_EQ(expected.spatial_layer_resolution_present,
              actual.spatial_layer_resolution_present);
    EXPECT_EQ(expected.num_spatial_layers, actual.num_spatial_layers);
    if (expected.spatial_layer_resolution_present) {
      for (size_t i = 0; i < expected.num_spatial_layers; i++) {
        EXPECT_EQ(expected.width[i], actual.width[i]);
        EXPECT_EQ(expected.height[i], actual.height[i]);
      }
    }
    EXPECT_EQ(expected.gof.num_frames_in_gof, actual.gof.num_frames_in_gof);
    for (size_t i = 0; i < expected.gof.num_frames_in_gof; i++) {
      EXPECT_EQ(expected.gof.temporal_up_switch[i],
                actual.gof.temporal_up_switch[i]);
      EXPECT_EQ(expected.gof.temporal_idx[i], actual.gof.temporal_idx[i]);
      EXPECT_EQ(expected.gof.num_ref_pics[i], actual.gof.num_ref_pics[i]);
      for (uint8_t j = 0; j < expected.gof.num_ref_pics[i]; j++) {
        EXPECT_EQ(expected.gof.pid_diff[i][j], actual.gof.pid_diff[i][j]);
      }
    }
  }
}

void VerifyPayload(const RtpDepacketizer::ParsedPayload& parsed,
                   const uint8_t* payload,
                   size_t payload_length) {
  EXPECT_EQ(payload, parsed.payload);
  EXPECT_EQ(payload_length, parsed.payload_length);
  EXPECT_THAT(std::vector<uint8_t>(parsed.payload,
                                   parsed.payload + parsed.payload_length),
              ::testing::ElementsAreArray(payload, payload_length));
}

void ParseAndCheckPacket(const uint8_t* packet,
                         const RTPVideoHeaderVP9& expected,
                         size_t expected_hdr_length,
                         size_t expected_length) {
  std::unique_ptr<RtpDepacketizer> depacketizer(new RtpDepacketizerVp9());
  RtpDepacketizer::ParsedPayload parsed;
  ASSERT_TRUE(depacketizer->Parse(&parsed, packet, expected_length));
  EXPECT_EQ(kRtpVideoVp9, parsed.type.Video.codec);
  VerifyHeader(expected, parsed.type.Video.codecHeader.VP9);
  const size_t kExpectedPayloadLength = expected_length - expected_hdr_length;
  VerifyPayload(parsed, packet + expected_hdr_length, kExpectedPayloadLength);
}
}  // namespace

// Payload descriptor for flexible mode
//        0 1 2 3 4 5 6 7
//        +-+-+-+-+-+-+-+-+
//        |I|P|L|F|B|E|V|-| (REQUIRED)
//        +-+-+-+-+-+-+-+-+
//   I:   |M| PICTURE ID  | (RECOMMENDED)
//        +-+-+-+-+-+-+-+-+
//   M:   | EXTENDED PID  | (RECOMMENDED)
//        +-+-+-+-+-+-+-+-+
//   L:   |  T  |U|  S  |D| (CONDITIONALLY RECOMMENDED)
//        +-+-+-+-+-+-+-+-+                             -|
//   P,F: | P_DIFF      |N| (CONDITIONALLY RECOMMENDED)  . up to 3 times
//        +-+-+-+-+-+-+-+-+                             -|
//   V:   | SS            |
//        | ..            |
//        +-+-+-+-+-+-+-+-+
//
// Payload descriptor for non-flexible mode
//        0 1 2 3 4 5 6 7
//        +-+-+-+-+-+-+-+-+
//        |I|P|L|F|B|E|V|-| (REQUIRED)
//        +-+-+-+-+-+-+-+-+
//   I:   |M| PICTURE ID  | (RECOMMENDED)
//        +-+-+-+-+-+-+-+-+
//   M:   | EXTENDED PID  | (RECOMMENDED)
//        +-+-+-+-+-+-+-+-+
//   L:   |  T  |U|  S  |D| (CONDITIONALLY RECOMMENDED)
//        +-+-+-+-+-+-+-+-+
//        |   TL0PICIDX   | (CONDITIONALLY REQUIRED)
//        +-+-+-+-+-+-+-+-+
//   V:   | SS            |
//        | ..            |
//        +-+-+-+-+-+-+-+-+

class RtpPacketizerVp9Test : public ::testing::Test {
 protected:
  static constexpr RtpPacketToSend::ExtensionManager* kNoExtensions = nullptr;
  static constexpr size_t kMaxPacketSize = 1200;

  RtpPacketizerVp9Test() : packet_(kNoExtensions, kMaxPacketSize) {}
  virtual void SetUp() {
    expected_.InitRTPVideoHeaderVP9();
  }

  RtpPacketToSend packet_;
  std::unique_ptr<uint8_t[]> payload_;
  size_t payload_size_;
  size_t payload_pos_;
  RTPVideoHeaderVP9 expected_;
  std::unique_ptr<RtpPacketizerVp9> packetizer_;
  size_t num_packets_;

  void Init(size_t payload_size, size_t packet_size) {
    payload_.reset(new uint8_t[payload_size]);
    memset(payload_.get(), 7, payload_size);
    payload_size_ = payload_size;
    payload_pos_ = 0;
    packetizer_.reset(new RtpPacketizerVp9(expected_, packet_size,
                                           /*last_packet_reduction_len=*/0));
    num_packets_ =
        packetizer_->SetPayloadData(payload_.get(), payload_size_, nullptr);
  }

  void CheckPayload(const uint8_t* packet,
                    size_t start_pos,
                    size_t end_pos,
                    bool last) {
    for (size_t i = start_pos; i < end_pos; ++i) {
      EXPECT_EQ(packet[i], payload_[payload_pos_++]);
    }
    EXPECT_EQ(last, payload_pos_ == payload_size_);
  }

  void CreateParseAndCheckPackets(const size_t* expected_hdr_sizes,
                                  const size_t* expected_sizes,
                                  size_t expected_num_packets) {
    ASSERT_TRUE(packetizer_.get() != NULL);
    if (expected_num_packets == 0) {
      EXPECT_FALSE(packetizer_->NextPacket(&packet_));
      return;
    }
    EXPECT_EQ(expected_num_packets, num_packets_);
    for (size_t i = 0; i < expected_num_packets; ++i) {
      EXPECT_TRUE(packetizer_->NextPacket(&packet_));
      auto rtp_payload = packet_.payload();
      EXPECT_EQ(expected_sizes[i], rtp_payload.size());
      RTPVideoHeaderVP9 hdr = expected_;
      hdr.beginning_of_frame = (i == 0);
      hdr.end_of_frame = (i + 1) == expected_num_packets;
      ParseAndCheckPacket(rtp_payload.data(), hdr, expected_hdr_sizes[i],
                          rtp_payload.size());
      CheckPayload(rtp_payload.data(), expected_hdr_sizes[i],
                   rtp_payload.size(), (i + 1) == expected_num_packets);
      expected_.ss_data_available = false;
    }
  }
};

TEST_F(RtpPacketizerVp9Test, TestEqualSizedMode_OnePacket) {
  const size_t kFrameSize = 25;
  const size_t kPacketSize = 26;
  Init(kFrameSize, kPacketSize);

  // One packet:
  // I:0, P:0, L:0, F:0, B:1, E:1, V:0  (1hdr + 25 payload)
  const size_t kExpectedHdrSizes[] = {1};
  const size_t kExpectedSizes[] = {26};
  const size_t kExpectedNum = GTEST_ARRAY_SIZE_(kExpectedSizes);
  CreateParseAndCheckPackets(kExpectedHdrSizes, kExpectedSizes, kExpectedNum);
}

TEST_F(RtpPacketizerVp9Test, TestEqualSizedMode_TwoPackets) {
  const size_t kFrameSize = 27;
  const size_t kPacketSize = 27;
  Init(kFrameSize, kPacketSize);

  // Two packets:
  // I:0, P:0, L:0, F:0, B:1, E:0, V:0  (1hdr + 14 payload)
  // I:0, P:0, L:0, F:0, B:0, E:1, V:0  (1hdr + 13 payload)
  const size_t kExpectedHdrSizes[] = {1, 1};
  const size_t kExpectedSizes[] = {14, 15};
  const size_t kExpectedNum = GTEST_ARRAY_SIZE_(kExpectedSizes);
  CreateParseAndCheckPackets(kExpectedHdrSizes, kExpectedSizes, kExpectedNum);
}

TEST_F(RtpPacketizerVp9Test, TestTooShortBufferToFitPayload) {
  const size_t kFrameSize = 1;
  const size_t kPacketSize = 1;
  Init(kFrameSize, kPacketSize);  // 1hdr + 1 payload

  const size_t kExpectedNum = 0;
  CreateParseAndCheckPackets(NULL, NULL, kExpectedNum);
}

TEST_F(RtpPacketizerVp9Test, TestOneBytePictureId) {
  const size_t kFrameSize = 30;
  const size_t kPacketSize = 12;

  expected_.picture_id = kMaxOneBytePictureId;   // 2 byte payload descriptor
  expected_.max_picture_id = kMaxOneBytePictureId;
  Init(kFrameSize, kPacketSize);

  // Three packets:
  // I:1, P:0, L:0, F:0, B:1, E:0, V:0 (2hdr + 10 payload)
  // I:1, P:0, L:0, F:0, B:0, E:0, V:0 (2hdr + 10 payload)
  // I:1, P:0, L:0, F:0, B:0, E:1, V:0 (2hdr + 10 payload)
  const size_t kExpectedHdrSizes[] = {2, 2, 2};
  const size_t kExpectedSizes[] = {12, 12, 12};
  const size_t kExpectedNum = GTEST_ARRAY_SIZE_(kExpectedSizes);
  CreateParseAndCheckPackets(kExpectedHdrSizes, kExpectedSizes, kExpectedNum);
}

TEST_F(RtpPacketizerVp9Test, TestTwoBytePictureId) {
  const size_t kFrameSize = 31;
  const size_t kPacketSize = 13;

  expected_.picture_id = kMaxTwoBytePictureId;  // 3 byte payload descriptor
  Init(kFrameSize, kPacketSize);

  // Four packets:
  // I:1, P:0, L:0, F:0, B:1, E:0, V:0 (3hdr + 8 payload)
  // I:1, P:0, L:0, F:0, B:0, E:0, V:0 (3hdr + 8 payload)
  // I:1, P:0, L:0, F:0, B:0, E:0, V:0 (3hdr + 8 payload)
  // I:1, P:0, L:0, F:0, B:0, E:1, V:0 (3hdr + 7 payload)
  const size_t kExpectedHdrSizes[] = {3, 3, 3, 3};
  const size_t kExpectedSizes[] = {10, 11, 11, 11};
  const size_t kExpectedNum = GTEST_ARRAY_SIZE_(kExpectedSizes);
  CreateParseAndCheckPackets(kExpectedHdrSizes, kExpectedSizes, kExpectedNum);
}

TEST_F(RtpPacketizerVp9Test, TestLayerInfoWithNonFlexibleMode) {
  const size_t kFrameSize = 30;
  const size_t kPacketSize = 25;

  expected_.temporal_idx = 3;
  expected_.temporal_up_switch = true;  // U
  expected_.num_spatial_layers = 3;
  expected_.spatial_idx = 2;
  expected_.inter_layer_predicted = true;  // D
  expected_.tl0_pic_idx = 117;
  Init(kFrameSize, kPacketSize);

  // Two packets:
  //    | I:0, P:0, L:1, F:0, B:1, E:0, V:0 | (3hdr + 15 payload)
  // L: | T:3, U:1, S:2, D:1 | TL0PICIDX:117 |
  //    | I:0, P:0, L:1, F:0, B:0, E:1, V:0 | (3hdr + 15 payload)
  // L: | T:3, U:1, S:2, D:1 | TL0PICIDX:117 |
  const size_t kExpectedHdrSizes[] = {3, 3};
  const size_t kExpectedSizes[] = {18, 18};
  const size_t kExpectedNum = GTEST_ARRAY_SIZE_(kExpectedSizes);
  CreateParseAndCheckPackets(kExpectedHdrSizes, kExpectedSizes, kExpectedNum);
}

TEST_F(RtpPacketizerVp9Test, TestLayerInfoWithFlexibleMode) {
  const size_t kFrameSize = 21;
  const size_t kPacketSize = 23;

  expected_.flexible_mode = true;
  expected_.temporal_idx = 3;
  expected_.temporal_up_switch = true;  // U
  expected_.num_spatial_layers = 3;
  expected_.spatial_idx = 2;
  expected_.inter_layer_predicted = false;  // D
  Init(kFrameSize, kPacketSize);

  // One packet:
  // I:0, P:0, L:1, F:1, B:1, E:1, V:0 (2hdr + 21 payload)
  // L:   T:3, U:1, S:2, D:0
  const size_t kExpectedHdrSizes[] = {2};
  const size_t kExpectedSizes[] = {23};
  const size_t kExpectedNum = GTEST_ARRAY_SIZE_(kExpectedSizes);
  CreateParseAndCheckPackets(kExpectedHdrSizes, kExpectedSizes, kExpectedNum);
}

TEST_F(RtpPacketizerVp9Test, TestRefIdx) {
  const size_t kFrameSize = 16;
  const size_t kPacketSize = 21;

  expected_.inter_pic_predicted = true;  // P
  expected_.flexible_mode = true;        // F
  expected_.picture_id = 2;
  expected_.max_picture_id = kMaxOneBytePictureId;

  expected_.num_ref_pics = 3;
  expected_.pid_diff[0] = 1;
  expected_.pid_diff[1] = 3;
  expected_.pid_diff[2] = 127;
  expected_.ref_picture_id[0] = 1;    // 2 - 1 = 1
  expected_.ref_picture_id[1] = 127;  // (kMaxPictureId + 1) + 2 - 3 = 127
  expected_.ref_picture_id[2] = 3;    // (kMaxPictureId + 1) + 2 - 127 = 3
  Init(kFrameSize, kPacketSize);

  // Two packets:
  // I:1, P:1, L:0, F:1, B:1, E:1, V:0 (5hdr + 16 payload)
  // I:   2
  // P,F: P_DIFF:1,   N:1
  //      P_DIFF:3,   N:1
  //      P_DIFF:127, N:0
  const size_t kExpectedHdrSizes[] = {5};
  const size_t kExpectedSizes[] = {21};
  const size_t kExpectedNum = GTEST_ARRAY_SIZE_(kExpectedSizes);
  CreateParseAndCheckPackets(kExpectedHdrSizes, kExpectedSizes, kExpectedNum);
}

TEST_F(RtpPacketizerVp9Test, TestRefIdxFailsWithoutPictureId) {
  const size_t kFrameSize = 16;
  const size_t kPacketSize = 21;

  expected_.inter_pic_predicted = true;
  expected_.flexible_mode = true;
  expected_.num_ref_pics = 1;
  expected_.pid_diff[0] = 3;
  Init(kFrameSize, kPacketSize);

  const size_t kExpectedNum = 0;
  CreateParseAndCheckPackets(NULL, NULL, kExpectedNum);
}

TEST_F(RtpPacketizerVp9Test, TestSsDataWithoutSpatialResolutionPresent) {
  const size_t kFrameSize = 21;
  const size_t kPacketSize = 26;

  expected_.ss_data_available = true;
  expected_.num_spatial_layers = 1;
  expected_.spatial_layer_resolution_present = false;
  expected_.gof.num_frames_in_gof = 1;
  expected_.gof.temporal_idx[0] = 0;
  expected_.gof.temporal_up_switch[0] = true;
  expected_.gof.num_ref_pics[0] = 1;
  expected_.gof.pid_diff[0][0] = 4;
  Init(kFrameSize, kPacketSize);

  // One packet:
  // I:0, P:0, L:0, F:0, B:1, E:1, V:1 (5hdr + 21 payload)
  // N_S:0, Y:0, G:1
  // N_G:1
  // T:0, U:1, R:1 | P_DIFF[0][0]:4
  const size_t kExpectedHdrSizes[] = {5};
  const size_t kExpectedSizes[] = {26};
  const size_t kExpectedNum = GTEST_ARRAY_SIZE_(kExpectedSizes);
  CreateParseAndCheckPackets(kExpectedHdrSizes, kExpectedSizes, kExpectedNum);
}

TEST_F(RtpPacketizerVp9Test, TestSsDataWithoutGbitPresent) {
  const size_t kFrameSize = 21;
  const size_t kPacketSize = 23;

  expected_.ss_data_available = true;
  expected_.num_spatial_layers = 1;
  expected_.spatial_layer_resolution_present = false;
  expected_.gof.num_frames_in_gof = 0;
  Init(kFrameSize, kPacketSize);

  // One packet:
  // I:0, P:0, L:0, F:0, B:1, E:1, V:1 (2hdr + 21 payload)
  // N_S:0, Y:0, G:0
  const size_t kExpectedHdrSizes[] = {2};
  const size_t kExpectedSizes[] = {23};
  const size_t kExpectedNum = GTEST_ARRAY_SIZE_(kExpectedSizes);
  CreateParseAndCheckPackets(kExpectedHdrSizes, kExpectedSizes, kExpectedNum);
}

TEST_F(RtpPacketizerVp9Test, TestSsData) {
  const size_t kFrameSize = 21;
  const size_t kPacketSize = 40;

  expected_.ss_data_available = true;
  expected_.num_spatial_layers = 2;
  expected_.spatial_layer_resolution_present = true;
  expected_.width[0] = 640;
  expected_.width[1] = 1280;
  expected_.height[0] = 360;
  expected_.height[1] = 720;
  expected_.gof.num_frames_in_gof = 3;
  expected_.gof.temporal_idx[0] = 0;
  expected_.gof.temporal_idx[1] = 1;
  expected_.gof.temporal_idx[2] = 2;
  expected_.gof.temporal_up_switch[0] = true;
  expected_.gof.temporal_up_switch[1] = true;
  expected_.gof.temporal_up_switch[2] = false;
  expected_.gof.num_ref_pics[0] = 0;
  expected_.gof.num_ref_pics[1] = 3;
  expected_.gof.num_ref_pics[2] = 2;
  expected_.gof.pid_diff[1][0] = 5;
  expected_.gof.pid_diff[1][1] = 6;
  expected_.gof.pid_diff[1][2] = 7;
  expected_.gof.pid_diff[2][0] = 8;
  expected_.gof.pid_diff[2][1] = 9;
  Init(kFrameSize, kPacketSize);

  // One packet:
  // I:0, P:0, L:0, F:0, B:1, E:1, V:1 (19hdr + 21 payload)
  // N_S:1, Y:1, G:1
  // WIDTH:640   // 2 bytes
  // HEIGHT:360  // 2 bytes
  // WIDTH:1280  // 2 bytes
  // HEIGHT:720  // 2 bytes
  // N_G:3
  // T:0, U:1, R:0
  // T:1, U:1, R:3 | P_DIFF[1][0]:5 | P_DIFF[1][1]:6 | P_DIFF[1][2]:7
  // T:2, U:0, R:2 | P_DIFF[2][0]:8 | P_DIFF[2][0]:9
  const size_t kExpectedHdrSizes[] = {19};
  const size_t kExpectedSizes[] = {40};
  const size_t kExpectedNum = GTEST_ARRAY_SIZE_(kExpectedSizes);
  CreateParseAndCheckPackets(kExpectedHdrSizes, kExpectedSizes, kExpectedNum);
}

TEST_F(RtpPacketizerVp9Test, TestSsDataDoesNotFitInAveragePacket) {
  const size_t kFrameSize = 24;
  const size_t kPacketSize = 20;

  expected_.ss_data_available = true;
  expected_.num_spatial_layers = 2;
  expected_.spatial_layer_resolution_present = true;
  expected_.width[0] = 640;
  expected_.width[1] = 1280;
  expected_.height[0] = 360;
  expected_.height[1] = 720;
  expected_.gof.num_frames_in_gof = 3;
  expected_.gof.temporal_idx[0] = 0;
  expected_.gof.temporal_idx[1] = 1;
  expected_.gof.temporal_idx[2] = 2;
  expected_.gof.temporal_up_switch[0] = true;
  expected_.gof.temporal_up_switch[1] = true;
  expected_.gof.temporal_up_switch[2] = false;
  expected_.gof.num_ref_pics[0] = 0;
  expected_.gof.num_ref_pics[1] = 3;
  expected_.gof.num_ref_pics[2] = 2;
  expected_.gof.pid_diff[1][0] = 5;
  expected_.gof.pid_diff[1][1] = 6;
  expected_.gof.pid_diff[1][2] = 7;
  expected_.gof.pid_diff[2][0] = 8;
  expected_.gof.pid_diff[2][1] = 9;
  Init(kFrameSize, kPacketSize);

  // Three packets:
  // I:0, P:0, L:0, F:0, B:1, E:1, V:1 (19hdr + 1 payload)
  // N_S:1, Y:1, G:1
  // WIDTH:640   // 2 bytes
  // HEIGHT:360  // 2 bytes
  // WIDTH:1280  // 2 bytes
  // HEIGHT:720  // 2 bytes
  // N_G:3
  // T:0, U:1, R:0
  // T:1, U:1, R:3 | P_DIFF[1][0]:5 | P_DIFF[1][1]:6 | P_DIFF[1][2]:7
  // T:2, U:0, R:2 | P_DIFF[2][0]:8 | P_DIFF[2][0]:9
  // Last two packets 1 bytes vp9 hdrs and the rest of payload 14 and 9 bytes.
  const size_t kExpectedHdrSizes[] = {19, 1, 1};
  const size_t kExpectedSizes[] = {20, 15, 10};
  const size_t kExpectedNum = GTEST_ARRAY_SIZE_(kExpectedSizes);
  CreateParseAndCheckPackets(kExpectedHdrSizes, kExpectedSizes, kExpectedNum);
}

TEST_F(RtpPacketizerVp9Test, TestOnlyHighestSpatialLayerSetMarker) {
  const size_t kFrameSize = 10;
  const size_t kPacketSize = 8;
  const size_t kLastPacketReductionLen = 0;
  const uint8_t kFrame[kFrameSize] = {7};
  const RTPFragmentationHeader* kNoFragmentation = nullptr;

  RTPVideoHeaderVP9 vp9_header;
  vp9_header.InitRTPVideoHeaderVP9();
  vp9_header.flexible_mode = true;
  vp9_header.num_spatial_layers = 3;

  RtpPacketToSend packet(kNoExtensions);

  vp9_header.spatial_idx = 0;
  RtpPacketizerVp9 packetizer0(vp9_header, kPacketSize,
                               kLastPacketReductionLen);
  packetizer0.SetPayloadData(kFrame, sizeof(kFrame), kNoFragmentation);
  ASSERT_TRUE(packetizer0.NextPacket(&packet));
  EXPECT_FALSE(packet.Marker());
  ASSERT_TRUE(packetizer0.NextPacket(&packet));
  EXPECT_FALSE(packet.Marker());

  vp9_header.spatial_idx = 1;
  RtpPacketizerVp9 packetizer1(vp9_header, kPacketSize,
                               kLastPacketReductionLen);
  packetizer1.SetPayloadData(kFrame, sizeof(kFrame), kNoFragmentation);
  ASSERT_TRUE(packetizer1.NextPacket(&packet));
  EXPECT_FALSE(packet.Marker());
  ASSERT_TRUE(packetizer1.NextPacket(&packet));
  EXPECT_FALSE(packet.Marker());

  vp9_header.spatial_idx = 2;
  RtpPacketizerVp9 packetizer2(vp9_header, kPacketSize,
                               kLastPacketReductionLen);
  packetizer2.SetPayloadData(kFrame, sizeof(kFrame), kNoFragmentation);
  ASSERT_TRUE(packetizer2.NextPacket(&packet));
  EXPECT_FALSE(packet.Marker());
  ASSERT_TRUE(packetizer2.NextPacket(&packet));
  EXPECT_TRUE(packet.Marker());
}

TEST_F(RtpPacketizerVp9Test, TestGeneratesMinimumNumberOfPackets) {
  const size_t kFrameSize = 10;
  const size_t kPacketSize = 8;
  const size_t kLastPacketReductionLen = 0;
  // Calculated by hand. One packet can contain
  // |kPacketSize| - |kVp9MinDiscriptorSize| = 6 bytes of the frame payload,
  // thus to fit 10 bytes two packets are required.
  const size_t kMinNumberOfPackets = 2;
  const uint8_t kFrame[kFrameSize] = {7};
  const RTPFragmentationHeader* kNoFragmentation = nullptr;

  RTPVideoHeaderVP9 vp9_header;
  vp9_header.InitRTPVideoHeaderVP9();

  RtpPacketToSend packet(kNoExtensions);

  RtpPacketizerVp9 packetizer(vp9_header, kPacketSize, kLastPacketReductionLen);
  EXPECT_EQ(kMinNumberOfPackets, packetizer.SetPayloadData(
                                     kFrame, sizeof(kFrame), kNoFragmentation));
  ASSERT_TRUE(packetizer.NextPacket(&packet));
  EXPECT_FALSE(packet.Marker());
  ASSERT_TRUE(packetizer.NextPacket(&packet));
  EXPECT_TRUE(packet.Marker());
}

TEST_F(RtpPacketizerVp9Test, TestRespectsLastPacketReductionLen) {
  const size_t kFrameSize = 10;
  const size_t kPacketSize = 8;
  const size_t kLastPacketReductionLen = 5;
  // Calculated by hand. VP9 payload descriptor is 2 bytes. Like in the test
  // above, 1 packet is not enough. 2 packets can contain
  // 2*(|kPacketSize| - |kVp9MinDiscriptorSize|) - |kLastPacketReductionLen| = 7
  // But three packets are enough, since they have capacity of 3*(8-2)-5=13
  // bytes.
  const size_t kMinNumberOfPackets = 3;
  const uint8_t kFrame[kFrameSize] = {7};
  const RTPFragmentationHeader* kNoFragmentation = nullptr;

  RTPVideoHeaderVP9 vp9_header;
  vp9_header.InitRTPVideoHeaderVP9();
  vp9_header.flexible_mode = true;

  RtpPacketToSend packet(kNoExtensions);

  RtpPacketizerVp9 packetizer0(vp9_header, kPacketSize,
                               kLastPacketReductionLen);
  EXPECT_EQ(
      packetizer0.SetPayloadData(kFrame, sizeof(kFrame), kNoFragmentation),
      kMinNumberOfPackets);
  ASSERT_TRUE(packetizer0.NextPacket(&packet));
  EXPECT_FALSE(packet.Marker());
  ASSERT_TRUE(packetizer0.NextPacket(&packet));
  EXPECT_FALSE(packet.Marker());
  ASSERT_TRUE(packetizer0.NextPacket(&packet));
  EXPECT_TRUE(packet.Marker());
}

class RtpDepacketizerVp9Test : public ::testing::Test {
 protected:
  RtpDepacketizerVp9Test()
      : depacketizer_(new RtpDepacketizerVp9()) {}

  virtual void SetUp() {
    expected_.InitRTPVideoHeaderVP9();
  }

  RTPVideoHeaderVP9 expected_;
  std::unique_ptr<RtpDepacketizer> depacketizer_;
};

TEST_F(RtpDepacketizerVp9Test, ParseBasicHeader) {
  const uint8_t kHeaderLength = 1;
  uint8_t packet[4] = {0};
  packet[0] = 0x0C;  // I:0 P:0 L:0 F:0 B:1 E:1 V:0 R:0
  expected_.beginning_of_frame = true;
  expected_.end_of_frame = true;
  ParseAndCheckPacket(packet, expected_, kHeaderLength, sizeof(packet));
}

TEST_F(RtpDepacketizerVp9Test, ParseOneBytePictureId) {
  const uint8_t kHeaderLength = 2;
  uint8_t packet[10] = {0};
  packet[0] = 0x80;  // I:1 P:0 L:0 F:0 B:0 E:0 V:0 R:0
  packet[1] = kMaxOneBytePictureId;

  expected_.picture_id = kMaxOneBytePictureId;
  expected_.max_picture_id = kMaxOneBytePictureId;
  ParseAndCheckPacket(packet, expected_, kHeaderLength, sizeof(packet));
}

TEST_F(RtpDepacketizerVp9Test, ParseTwoBytePictureId) {
  const uint8_t kHeaderLength = 3;
  uint8_t packet[10] = {0};
  packet[0] = 0x80;  // I:1 P:0 L:0 F:0 B:0 E:0 V:0 R:0
  packet[1] = 0x80 | ((kMaxTwoBytePictureId >> 8) & 0x7F);
  packet[2] = kMaxTwoBytePictureId & 0xFF;

  expected_.picture_id = kMaxTwoBytePictureId;
  expected_.max_picture_id = kMaxTwoBytePictureId;
  ParseAndCheckPacket(packet, expected_, kHeaderLength, sizeof(packet));
}

TEST_F(RtpDepacketizerVp9Test, ParseLayerInfoWithNonFlexibleMode) {
  const uint8_t kHeaderLength = 3;
  const uint8_t kTemporalIdx = 2;
  const uint8_t kUbit = 1;
  const uint8_t kSpatialIdx = 1;
  const uint8_t kDbit = 1;
  const uint8_t kTl0PicIdx = 17;
  uint8_t packet[13] = {0};
  packet[0] = 0x20;  // I:0 P:0 L:1 F:0 B:0 E:0 V:0 R:0
  packet[1] = (kTemporalIdx << 5) | (kUbit << 4) | (kSpatialIdx << 1) | kDbit;
  packet[2] = kTl0PicIdx;

  // T:2 U:1 S:1 D:1
  // TL0PICIDX:17
  expected_.temporal_idx = kTemporalIdx;
  expected_.temporal_up_switch = kUbit ? true : false;
  expected_.spatial_idx = kSpatialIdx;
  expected_.inter_layer_predicted = kDbit ? true : false;
  expected_.tl0_pic_idx = kTl0PicIdx;
  ParseAndCheckPacket(packet, expected_, kHeaderLength, sizeof(packet));
}

TEST_F(RtpDepacketizerVp9Test, ParseLayerInfoWithFlexibleMode) {
  const uint8_t kHeaderLength = 2;
  const uint8_t kTemporalIdx = 2;
  const uint8_t kUbit = 1;
  const uint8_t kSpatialIdx = 0;
  const uint8_t kDbit = 0;
  uint8_t packet[13] = {0};
  packet[0] = 0x38;  // I:0 P:0 L:1 F:1 B:1 E:0 V:0 R:0
  packet[1] = (kTemporalIdx << 5) | (kUbit << 4) | (kSpatialIdx << 1) | kDbit;

  // I:0 P:0 L:1 F:1 B:1 E:0 V:0
  // L:   T:2 U:1 S:0 D:0
  expected_.beginning_of_frame = true;
  expected_.flexible_mode = true;
  expected_.temporal_idx = kTemporalIdx;
  expected_.temporal_up_switch = kUbit ? true : false;
  expected_.spatial_idx = kSpatialIdx;
  expected_.inter_layer_predicted = kDbit ? true : false;
  ParseAndCheckPacket(packet, expected_, kHeaderLength, sizeof(packet));
}

TEST_F(RtpDepacketizerVp9Test, ParseRefIdx) {
  const uint8_t kHeaderLength = 6;
  const int16_t kPictureId = 17;
  const uint8_t kPdiff1 = 17;
  const uint8_t kPdiff2 = 18;
  const uint8_t kPdiff3 = 127;
  uint8_t packet[13] = {0};
  packet[0] = 0xD8;  // I:1 P:1 L:0 F:1 B:1 E:0 V:0 R:0
  packet[1] = 0x80 | ((kPictureId >> 8) & 0x7F);  // Two byte pictureID.
  packet[2] = kPictureId;
  packet[3] = (kPdiff1 << 1) | 1;  // P_DIFF N:1
  packet[4] = (kPdiff2 << 1) | 1;  // P_DIFF N:1
  packet[5] = (kPdiff3 << 1) | 0;  // P_DIFF N:0

  // I:1 P:1 L:0 F:1 B:1 E:0 V:0
  // I:    PICTURE ID:17
  // I:
  // P,F:  P_DIFF:17  N:1 => refPicId = 17 - 17 = 0
  // P,F:  P_DIFF:18  N:1 => refPicId = (kMaxPictureId + 1) + 17 - 18 = 0x7FFF
  // P,F:  P_DIFF:127 N:0 => refPicId = (kMaxPictureId + 1) + 17 - 127 = 32658
  expected_.beginning_of_frame = true;
  expected_.inter_pic_predicted = true;
  expected_.flexible_mode = true;
  expected_.picture_id = kPictureId;
  expected_.num_ref_pics = 3;
  expected_.pid_diff[0] = kPdiff1;
  expected_.pid_diff[1] = kPdiff2;
  expected_.pid_diff[2] = kPdiff3;
  expected_.ref_picture_id[0] = 0;
  expected_.ref_picture_id[1] = 0x7FFF;
  expected_.ref_picture_id[2] = 32658;
  ParseAndCheckPacket(packet, expected_, kHeaderLength, sizeof(packet));
}

TEST_F(RtpDepacketizerVp9Test, ParseRefIdxFailsWithNoPictureId) {
  const uint8_t kPdiff = 3;
  uint8_t packet[13] = {0};
  packet[0] = 0x58;            // I:0 P:1 L:0 F:1 B:1 E:0 V:0 R:0
  packet[1] = (kPdiff << 1);   // P,F:  P_DIFF:3 N:0

  RtpDepacketizer::ParsedPayload parsed;
  EXPECT_FALSE(depacketizer_->Parse(&parsed, packet, sizeof(packet)));
}

TEST_F(RtpDepacketizerVp9Test, ParseRefIdxFailsWithTooManyRefPics) {
  const uint8_t kPdiff = 3;
  uint8_t packet[13] = {0};
  packet[0] = 0xD8;                  // I:1 P:1 L:0 F:1 B:1 E:0 V:0 R:0
  packet[1] = kMaxOneBytePictureId;  // I:    PICTURE ID:127
  packet[2] = (kPdiff << 1) | 1;     // P,F:  P_DIFF:3 N:1
  packet[3] = (kPdiff << 1) | 1;     // P,F:  P_DIFF:3 N:1
  packet[4] = (kPdiff << 1) | 1;     // P,F:  P_DIFF:3 N:1
  packet[5] = (kPdiff << 1) | 0;     // P,F:  P_DIFF:3 N:0

  RtpDepacketizer::ParsedPayload parsed;
  EXPECT_FALSE(depacketizer_->Parse(&parsed, packet, sizeof(packet)));
}

TEST_F(RtpDepacketizerVp9Test, ParseSsData) {
  const uint8_t kHeaderLength = 6;
  const uint8_t kYbit = 0;
  const size_t kNs = 2;
  const size_t kNg = 2;
  uint8_t packet[23] = {0};
  packet[0] = 0x0A;  // I:0 P:0 L:0 F:0 B:1 E:0 V:1 R:0
  packet[1] = ((kNs - 1) << 5) | (kYbit << 4) | (1 << 3);  // N_S Y G:1 -
  packet[2] = kNg;                                         // N_G
  packet[3] = (0 << 5) | (1 << 4) | (0 << 2) | 0;          // T:0 U:1 R:0 -
  packet[4] = (2 << 5) | (0 << 4) | (1 << 2) | 0;          // T:2 U:0 R:1 -
  packet[5] = 33;

  expected_.beginning_of_frame = true;
  expected_.ss_data_available = true;
  expected_.num_spatial_layers = kNs;
  expected_.spatial_layer_resolution_present = kYbit ? true : false;
  expected_.gof.num_frames_in_gof = kNg;
  expected_.gof.temporal_idx[0] = 0;
  expected_.gof.temporal_idx[1] = 2;
  expected_.gof.temporal_up_switch[0] = true;
  expected_.gof.temporal_up_switch[1] = false;
  expected_.gof.num_ref_pics[0] = 0;
  expected_.gof.num_ref_pics[1] = 1;
  expected_.gof.pid_diff[1][0] = 33;
  ParseAndCheckPacket(packet, expected_, kHeaderLength, sizeof(packet));
}

TEST_F(RtpDepacketizerVp9Test, ParseFirstPacketInKeyFrame) {
  uint8_t packet[2] = {0};
  packet[0] = 0x08;  // I:0 P:0 L:0 F:0 B:1 E:0 V:0 R:0

  RtpDepacketizer::ParsedPayload parsed;
  ASSERT_TRUE(depacketizer_->Parse(&parsed, packet, sizeof(packet)));
  EXPECT_EQ(kVideoFrameKey, parsed.frame_type);
  EXPECT_TRUE(parsed.type.Video.is_first_packet_in_frame);
}

TEST_F(RtpDepacketizerVp9Test, ParseLastPacketInDeltaFrame) {
  uint8_t packet[2] = {0};
  packet[0] = 0x44;  // I:0 P:1 L:0 F:0 B:0 E:1 V:0 R:0

  RtpDepacketizer::ParsedPayload parsed;
  ASSERT_TRUE(depacketizer_->Parse(&parsed, packet, sizeof(packet)));
  EXPECT_EQ(kVideoFrameDelta, parsed.frame_type);
  EXPECT_FALSE(parsed.type.Video.is_first_packet_in_frame);
}

TEST_F(RtpDepacketizerVp9Test, ParseResolution) {
  const uint16_t kWidth[2] = {640, 1280};
  const uint16_t kHeight[2] = {360, 720};
  uint8_t packet[20] = {0};
  packet[0] = 0x0A;  // I:0 P:0 L:0 F:0 B:1 E:0 V:1 R:0
  packet[1] = (1 << 5) | (1 << 4) | 0;  // N_S:1 Y:1 G:0
  packet[2] = kWidth[0] >> 8;
  packet[3] = kWidth[0] & 0xFF;
  packet[4] = kHeight[0] >> 8;
  packet[5] = kHeight[0] & 0xFF;
  packet[6] = kWidth[1] >> 8;
  packet[7] = kWidth[1] & 0xFF;
  packet[8] = kHeight[1] >> 8;
  packet[9] = kHeight[1] & 0xFF;

  RtpDepacketizer::ParsedPayload parsed;
  ASSERT_TRUE(depacketizer_->Parse(&parsed, packet, sizeof(packet)));
  EXPECT_EQ(kWidth[0], parsed.type.Video.width);
  EXPECT_EQ(kHeight[0], parsed.type.Video.height);
}

TEST_F(RtpDepacketizerVp9Test, ParseFailsForNoPayloadLength) {
  uint8_t packet[1] = {0};
  RtpDepacketizer::ParsedPayload parsed;
  EXPECT_FALSE(depacketizer_->Parse(&parsed, packet, 0));
}

TEST_F(RtpDepacketizerVp9Test, ParseFailsForTooShortBufferToFitPayload) {
  const uint8_t kHeaderLength = 1;
  uint8_t packet[kHeaderLength] = {0};
  RtpDepacketizer::ParsedPayload parsed;
  EXPECT_FALSE(depacketizer_->Parse(&parsed, packet, sizeof(packet)));
}

}  // namespace webrtc
