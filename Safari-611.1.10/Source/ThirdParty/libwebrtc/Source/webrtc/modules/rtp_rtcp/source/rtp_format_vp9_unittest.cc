/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_format_vp9.h"

#include <memory>
#include <vector>

#include "api/array_view.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer_vp9.h"
#include "test/gmock.h"
#include "test/gtest.h"

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
  EXPECT_EQ(expected.non_ref_for_inter_layer_pred,
            actual.non_ref_for_inter_layer_pred);
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

void ParseAndCheckPacket(const uint8_t* packet,
                         const RTPVideoHeaderVP9& expected,
                         int expected_hdr_length,
                         size_t expected_length) {
  RTPVideoHeader video_header;
  EXPECT_EQ(VideoRtpDepacketizerVp9::ParseRtpPayload(
                rtc::MakeArrayView(packet, expected_length), &video_header),
            expected_hdr_length);
  EXPECT_EQ(kVideoCodecVP9, video_header.codec);
  auto& vp9_header =
      absl::get<RTPVideoHeaderVP9>(video_header.video_type_header);
  VerifyHeader(expected, vp9_header);
}

// Payload descriptor for flexible mode
//        0 1 2 3 4 5 6 7
//        +-+-+-+-+-+-+-+-+
//        |I|P|L|F|B|E|V|Z| (REQUIRED)
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
//        |I|P|L|F|B|E|V|Z| (REQUIRED)
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
  void SetUp() override { expected_.InitRTPVideoHeaderVP9(); }

  RtpPacketToSend packet_;
  std::vector<uint8_t> payload_;
  size_t payload_pos_;
  RTPVideoHeaderVP9 expected_;
  std::unique_ptr<RtpPacketizerVp9> packetizer_;
  size_t num_packets_;

  void Init(size_t payload_size, size_t packet_size) {
    payload_.assign(payload_size, 7);
    payload_pos_ = 0;
    RtpPacketizer::PayloadSizeLimits limits;
    limits.max_payload_len = packet_size;
    packetizer_.reset(new RtpPacketizerVp9(payload_, limits, expected_));
    num_packets_ = packetizer_->NumPackets();
  }

  void CheckPayload(const uint8_t* packet,
                    size_t start_pos,
                    size_t end_pos,
                    bool last) {
    for (size_t i = start_pos; i < end_pos; ++i) {
      EXPECT_EQ(packet[i], payload_[payload_pos_++]);
    }
    EXPECT_EQ(last, payload_pos_ == payload_.size());
  }

  void CreateParseAndCheckPackets(
      rtc::ArrayView<const size_t> expected_hdr_sizes,
      rtc::ArrayView<const size_t> expected_sizes) {
    ASSERT_EQ(expected_hdr_sizes.size(), expected_sizes.size());
    ASSERT_TRUE(packetizer_ != nullptr);
    EXPECT_EQ(expected_sizes.size(), num_packets_);
    for (size_t i = 0; i < expected_sizes.size(); ++i) {
      EXPECT_TRUE(packetizer_->NextPacket(&packet_));
      auto rtp_payload = packet_.payload();
      EXPECT_EQ(expected_sizes[i], rtp_payload.size());
      RTPVideoHeaderVP9 hdr = expected_;
      hdr.beginning_of_frame = (i == 0);
      hdr.end_of_frame = (i + 1) == expected_sizes.size();
      ParseAndCheckPacket(rtp_payload.data(), hdr, expected_hdr_sizes[i],
                          rtp_payload.size());
      CheckPayload(rtp_payload.data(), expected_hdr_sizes[i],
                   rtp_payload.size(), (i + 1) == expected_sizes.size());
      expected_.ss_data_available = false;
    }
  }

  void CreateParseAndCheckPacketsLayers(size_t num_spatial_layers,
                                        size_t expected_layer) {
    ASSERT_TRUE(packetizer_ != nullptr);
    for (size_t i = 0; i < num_packets_; ++i) {
      EXPECT_TRUE(packetizer_->NextPacket(&packet_));
      RTPVideoHeader video_header;
      VideoRtpDepacketizerVp9::ParseRtpPayload(packet_.payload(),
                                               &video_header);
      const auto& vp9_header =
          absl::get<RTPVideoHeaderVP9>(video_header.video_type_header);
      EXPECT_EQ(vp9_header.spatial_idx, expected_layer);
      EXPECT_EQ(vp9_header.num_spatial_layers, num_spatial_layers);
    }
  }
};

TEST_F(RtpPacketizerVp9Test, TestEqualSizedMode_OnePacket) {
  const size_t kFrameSize = 25;
  const size_t kPacketSize = 26;
  Init(kFrameSize, kPacketSize);

  // One packet:
  // I:0, P:0, L:0, F:0, B:1, E:1, V:0, Z:0  (1hdr + 25 payload)
  const size_t kExpectedHdrSizes[] = {1};
  const size_t kExpectedSizes[] = {26};
  CreateParseAndCheckPackets(kExpectedHdrSizes, kExpectedSizes);
}

TEST_F(RtpPacketizerVp9Test, TestEqualSizedMode_TwoPackets) {
  const size_t kFrameSize = 27;
  const size_t kPacketSize = 27;
  Init(kFrameSize, kPacketSize);

  // Two packets:
  // I:0, P:0, L:0, F:0, B:1, E:0, V:0, Z:0  (1hdr + 14 payload)
  // I:0, P:0, L:0, F:0, B:0, E:1, V:0, Z:0  (1hdr + 13 payload)
  const size_t kExpectedHdrSizes[] = {1, 1};
  const size_t kExpectedSizes[] = {14, 15};
  CreateParseAndCheckPackets(kExpectedHdrSizes, kExpectedSizes);
}

TEST_F(RtpPacketizerVp9Test, TestTooShortBufferToFitPayload) {
  const size_t kFrameSize = 1;
  const size_t kPacketSize = 1;
  Init(kFrameSize, kPacketSize);  // 1hdr + 1 payload

  EXPECT_FALSE(packetizer_->NextPacket(&packet_));
}

TEST_F(RtpPacketizerVp9Test, TestOneBytePictureId) {
  const size_t kFrameSize = 30;
  const size_t kPacketSize = 12;

  expected_.picture_id = kMaxOneBytePictureId;  // 2 byte payload descriptor
  expected_.max_picture_id = kMaxOneBytePictureId;
  Init(kFrameSize, kPacketSize);

  // Three packets:
  // I:1, P:0, L:0, F:0, B:1, E:0, V:0, Z:0 (2hdr + 10 payload)
  // I:1, P:0, L:0, F:0, B:0, E:0, V:0, Z:0 (2hdr + 10 payload)
  // I:1, P:0, L:0, F:0, B:0, E:1, V:0, Z:0 (2hdr + 10 payload)
  const size_t kExpectedHdrSizes[] = {2, 2, 2};
  const size_t kExpectedSizes[] = {12, 12, 12};
  CreateParseAndCheckPackets(kExpectedHdrSizes, kExpectedSizes);
}

TEST_F(RtpPacketizerVp9Test, TestTwoBytePictureId) {
  const size_t kFrameSize = 31;
  const size_t kPacketSize = 13;

  expected_.picture_id = kMaxTwoBytePictureId;  // 3 byte payload descriptor
  Init(kFrameSize, kPacketSize);

  // Four packets:
  // I:1, P:0, L:0, F:0, B:1, E:0, V:0, Z:0 (3hdr + 8 payload)
  // I:1, P:0, L:0, F:0, B:0, E:0, V:0, Z:0 (3hdr + 8 payload)
  // I:1, P:0, L:0, F:0, B:0, E:0, V:0, Z:0 (3hdr + 8 payload)
  // I:1, P:0, L:0, F:0, B:0, E:1, V:0, Z:0 (3hdr + 7 payload)
  const size_t kExpectedHdrSizes[] = {3, 3, 3, 3};
  const size_t kExpectedSizes[] = {10, 11, 11, 11};
  CreateParseAndCheckPackets(kExpectedHdrSizes, kExpectedSizes);
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
  //    | I:0, P:0, L:1, F:0, B:1, E:0, V:0 Z:0 | (3hdr + 15 payload)
  // L: | T:3, U:1, S:2, D:1 | TL0PICIDX:117 |
  //    | I:0, P:0, L:1, F:0, B:0, E:1, V:0 Z:0 | (3hdr + 15 payload)
  // L: | T:3, U:1, S:2, D:1 | TL0PICIDX:117 |
  const size_t kExpectedHdrSizes[] = {3, 3};
  const size_t kExpectedSizes[] = {18, 18};
  CreateParseAndCheckPackets(kExpectedHdrSizes, kExpectedSizes);
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
  CreateParseAndCheckPackets(kExpectedHdrSizes, kExpectedSizes);
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
  // I:1, P:1, L:0, F:1, B:1, E:1, V:0, Z:0 (5hdr + 16 payload)
  // I:   2
  // P,F: P_DIFF:1,   N:1
  //      P_DIFF:3,   N:1
  //      P_DIFF:127, N:0
  const size_t kExpectedHdrSizes[] = {5};
  const size_t kExpectedSizes[] = {21};
  CreateParseAndCheckPackets(kExpectedHdrSizes, kExpectedSizes);
}

TEST_F(RtpPacketizerVp9Test, TestRefIdxFailsWithoutPictureId) {
  const size_t kFrameSize = 16;
  const size_t kPacketSize = 21;

  expected_.inter_pic_predicted = true;
  expected_.flexible_mode = true;
  expected_.num_ref_pics = 1;
  expected_.pid_diff[0] = 3;
  Init(kFrameSize, kPacketSize);

  EXPECT_FALSE(packetizer_->NextPacket(&packet_));
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
  // I:0, P:0, L:0, F:0, B:1, E:1, V:1, Z:0 (5hdr + 21 payload)
  // N_S:0, Y:0, G:1
  // N_G:1
  // T:0, U:1, R:1 | P_DIFF[0][0]:4
  const size_t kExpectedHdrSizes[] = {5};
  const size_t kExpectedSizes[] = {26};
  CreateParseAndCheckPackets(kExpectedHdrSizes, kExpectedSizes);
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
  // I:0, P:0, L:0, F:0, B:1, E:1, V:1, Z:0 (2hdr + 21 payload)
  // N_S:0, Y:0, G:0
  const size_t kExpectedHdrSizes[] = {2};
  const size_t kExpectedSizes[] = {23};
  CreateParseAndCheckPackets(kExpectedHdrSizes, kExpectedSizes);
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
  // I:0, P:0, L:0, F:0, B:1, E:1, V:1, Z:0 (19hdr + 21 payload)
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
  CreateParseAndCheckPackets(kExpectedHdrSizes, kExpectedSizes);
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
  // I:0, P:0, L:0, F:0, B:1, E:1, V:1, Z:0 (19hdr + 1 payload)
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
  CreateParseAndCheckPackets(kExpectedHdrSizes, kExpectedSizes);
}

TEST_F(RtpPacketizerVp9Test, EndOfPictureSetsSetMarker) {
  const size_t kFrameSize = 10;
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 8;
  const uint8_t kFrame[kFrameSize] = {7};

  RTPVideoHeaderVP9 vp9_header;
  vp9_header.InitRTPVideoHeaderVP9();
  vp9_header.flexible_mode = true;
  vp9_header.num_spatial_layers = 3;

  RtpPacketToSend packet(kNoExtensions);

  // Drop top layer and ensure that marker bit is set on last encoded layer.
  for (size_t spatial_idx = 0; spatial_idx < vp9_header.num_spatial_layers - 1;
       ++spatial_idx) {
    const bool end_of_picture =
        spatial_idx + 1 == vp9_header.num_spatial_layers - 1;
    vp9_header.spatial_idx = spatial_idx;
    vp9_header.end_of_picture = end_of_picture;
    RtpPacketizerVp9 packetizer(kFrame, limits, vp9_header);
    ASSERT_TRUE(packetizer.NextPacket(&packet));
    EXPECT_FALSE(packet.Marker());
    ASSERT_TRUE(packetizer.NextPacket(&packet));
    EXPECT_EQ(packet.Marker(), end_of_picture);
  }
}

TEST_F(RtpPacketizerVp9Test, TestGeneratesMinimumNumberOfPackets) {
  const size_t kFrameSize = 10;
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 8;
  // Calculated by hand. One packet can contain
  // |kPacketSize| - |kVp9MinDiscriptorSize| = 6 bytes of the frame payload,
  // thus to fit 10 bytes two packets are required.
  const size_t kMinNumberOfPackets = 2;
  const uint8_t kFrame[kFrameSize] = {7};

  RTPVideoHeaderVP9 vp9_header;
  vp9_header.InitRTPVideoHeaderVP9();

  RtpPacketToSend packet(kNoExtensions);

  RtpPacketizerVp9 packetizer(kFrame, limits, vp9_header);
  EXPECT_EQ(packetizer.NumPackets(), kMinNumberOfPackets);
  ASSERT_TRUE(packetizer.NextPacket(&packet));
  EXPECT_FALSE(packet.Marker());
  ASSERT_TRUE(packetizer.NextPacket(&packet));
  EXPECT_TRUE(packet.Marker());
}

TEST_F(RtpPacketizerVp9Test, TestRespectsLastPacketReductionLen) {
  const size_t kFrameSize = 10;
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 8;
  limits.last_packet_reduction_len = 5;
  // Calculated by hand. VP9 payload descriptor is 2 bytes. Like in the test
  // above, 1 packet is not enough. 2 packets can contain
  // 2*(|kPacketSize| - |kVp9MinDiscriptorSize|) - |kLastPacketReductionLen| = 7
  // But three packets are enough, since they have capacity of 3*(8-2)-5=13
  // bytes.
  const size_t kMinNumberOfPackets = 3;
  const uint8_t kFrame[kFrameSize] = {7};

  RTPVideoHeaderVP9 vp9_header;
  vp9_header.InitRTPVideoHeaderVP9();
  vp9_header.flexible_mode = true;

  RtpPacketToSend packet(kNoExtensions);

  RtpPacketizerVp9 packetizer0(kFrame, limits, vp9_header);
  EXPECT_EQ(packetizer0.NumPackets(), kMinNumberOfPackets);
  ASSERT_TRUE(packetizer0.NextPacket(&packet));
  EXPECT_FALSE(packet.Marker());
  ASSERT_TRUE(packetizer0.NextPacket(&packet));
  EXPECT_FALSE(packet.Marker());
  ASSERT_TRUE(packetizer0.NextPacket(&packet));
  EXPECT_TRUE(packet.Marker());
}

TEST_F(RtpPacketizerVp9Test, TestNonRefForInterLayerPred) {
  const size_t kFrameSize = 25;
  const size_t kPacketSize = 26;

  expected_.non_ref_for_inter_layer_pred = true;
  Init(kFrameSize, kPacketSize);

  // I:0, P:0, L:0, F:0, B:1, E:1, V:0, Z:1  (1hdr + 25 payload)
  const size_t kExpectedHdrSizes[] = {1};
  const size_t kExpectedSizes[] = {26};
  CreateParseAndCheckPackets(kExpectedHdrSizes, kExpectedSizes);
}

TEST_F(RtpPacketizerVp9Test,
       ShiftsSpatialLayersTowardZeroWhenFirstLayersAreDisabled) {
  const size_t kFrameSize = 25;
  const size_t kPacketSize = 1024;

  expected_.width[0] = 0;
  expected_.height[0] = 0;
  expected_.width[1] = 640;
  expected_.height[1] = 360;
  expected_.width[2] = 1280;
  expected_.height[2] = 720;
  expected_.num_spatial_layers = 3;
  expected_.first_active_layer = 1;
  expected_.ss_data_available = true;
  expected_.spatial_layer_resolution_present = true;
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

  expected_.spatial_idx = 1;
  Init(kFrameSize, kPacketSize);
  CreateParseAndCheckPacketsLayers(/*num_spatial_layers=*/2,
                                   /*expected_layer=*/0);

  // Now check for SL 2;
  expected_.spatial_idx = 2;
  Init(kFrameSize, kPacketSize);
  CreateParseAndCheckPacketsLayers(/*num_spatial_layers=*/2,
                                   /*expected_layer=*/1);
}

}  // namespace
}  // namespace webrtc
