/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/video_rtp_depacketizer_h265.h"

#include <cstdint>
#include <optional>

#include "api/array_view.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame_type.h"
#include "common_video/h265/h265_common.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer.h"
#include "rtc_base/buffer.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAreArray;

TEST(VideoRtpDepacketizerH265Test, SingleNalu) {
  uint8_t packet[3] = {0x26, 0x02,
                       0xFF};  // F=0, Type=19 (Idr), LayerId=0, TID=2.
  uint8_t expected_packet[] = {0x00, 0x00, 0x00, 0x01, 0x26, 0x02, 0xff};
  CopyOnWriteBuffer rtp_payload(packet);

  VideoRtpDepacketizerH265 depacketizer;
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(rtp_payload);
  ASSERT_TRUE(parsed);

  EXPECT_THAT(MakeArrayView(parsed->video_payload.cdata(),
                            parsed->video_payload.size()),
              ElementsAreArray(expected_packet));
  EXPECT_EQ(parsed->video_header.frame_type, VideoFrameType::kVideoFrameKey);
  EXPECT_EQ(parsed->video_header.codec, kVideoCodecH265);
  EXPECT_TRUE(parsed->video_header.is_first_packet_in_frame);
}

TEST(VideoRtpDepacketizerH265Test, SingleNaluSpsWithResolution) {
  // SPS for a 1280x720 camera capture from ffmpeg on linux. Contains
  // emulation bytes but no cropping. This buffer is generated
  // with following command:
  // 1) ffmpeg -i /dev/video0 -r 30 -c:v libx265 -s 1280x720 camera.h265
  //
  // 2) Open camera.h265 and find the SPS, generally everything between the
  // second and third start codes (0 0 0 1 or 0 0 1). The first two bytes
  // 0x42 and 0x02 shows the nal header of SPS.
  uint8_t packet[] = {0x42, 0x02, 0x01, 0x04, 0x08, 0x00, 0x00, 0x03,
                      0x00, 0x9d, 0x08, 0x00, 0x00, 0x03, 0x00, 0x00,
                      0x5d, 0xb0, 0x02, 0x80, 0x80, 0x2d, 0x16, 0x59,
                      0x59, 0xa4, 0x93, 0x2b, 0x80, 0x40, 0x00, 0x00,
                      0x03, 0x00, 0x40, 0x00, 0x00, 0x07, 0x82};
  uint8_t expected_packet[] = {
      0x00, 0x00, 0x00, 0x01, 0x42, 0x02, 0x01, 0x04, 0x08, 0x00, 0x00,
      0x03, 0x00, 0x9d, 0x08, 0x00, 0x00, 0x03, 0x00, 0x00, 0x5d, 0xb0,
      0x02, 0x80, 0x80, 0x2d, 0x16, 0x59, 0x59, 0xa4, 0x93, 0x2b, 0x80,
      0x40, 0x00, 0x00, 0x03, 0x00, 0x40, 0x00, 0x00, 0x07, 0x82};
  CopyOnWriteBuffer rtp_payload(packet);

  VideoRtpDepacketizerH265 depacketizer;
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(rtp_payload);
  ASSERT_TRUE(parsed);

  EXPECT_THAT(MakeArrayView(parsed->video_payload.cdata(),
                            parsed->video_payload.size()),
              ElementsAreArray(expected_packet));
  EXPECT_EQ(parsed->video_header.codec, kVideoCodecH265);
  EXPECT_TRUE(parsed->video_header.is_first_packet_in_frame);
  EXPECT_EQ(parsed->video_header.width, 1280u);
  EXPECT_EQ(parsed->video_header.height, 720u);
}

TEST(VideoRtpDepacketizerH265Test, PaciPackets) {
  uint8_t packet[2] = {0x64, 0x02};  // F=0, Type=50 (PACI), LayerId=0, TID=2.
  CopyOnWriteBuffer rtp_payload(packet);

  VideoRtpDepacketizerH265 depacketizer;
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(rtp_payload);
  ASSERT_FALSE(parsed);
}

TEST(VideoRtpDepacketizerH265Test, ApKey) {
  uint8_t payload_header[] = {0x60, 0x02};
  uint8_t vps_nalu_size[] = {0, 0x17};
  uint8_t sps_nalu_size[] = {0, 0x27};
  uint8_t pps_nalu_size[] = {0, 0x32};
  uint8_t slice_nalu_size[] = {0, 0xa};
  uint8_t start_code[] = {0x00, 0x00, 0x00, 0x01};
  // VPS/SPS/PPS/IDR for a 1280x720 camera capture from ffmpeg on linux.
  // Contains emulation bytes but no cropping. This buffer is generated with
  // following command: 1) ffmpeg -i /dev/video0 -r 30 -c:v libx265 -s 1280x720
  // camera.h265
  //
  // 2) Open camera.h265 and find:
  // VPS - generally everything between the first and second start codes (0 0 0
  // 1 or 0 0 1). The first two bytes 0x40 and 0x02 shows the nal header of VPS.
  // SPS - generally everything between the
  // second and third start codes (0 0 0 1 or 0 0 1). The first two bytes
  // 0x42 and 0x02 shows the nal header of SPS.
  // PPS - generally everything between the third and fourth start codes (0 0 0
  // 1 or 0 0 1). The first two bytes 0x44 and 0x02 shows the nal header of PPS.
  // IDR - Part of the keyframe bitstream (no need to show all the bytes for
  // depacketizer testing). The first two bytes 0x26 and 0x02 shows the nal
  // header of IDR frame.
  uint8_t vps[] = {
      0x40, 0x02, 0x1c, 0x01, 0xff, 0xff, 0x04, 0x08, 0x00, 0x00, 0x03, 0x00,
      0x9d, 0x08, 0x00, 0x00, 0x03, 0x00, 0x00, 0x78, 0x95, 0x98, 0x09,
  };
  uint8_t sps[] = {0x42, 0x02, 0x01, 0x04, 0x08, 0x00, 0x00, 0x03, 0x00, 0x9d,
                   0x08, 0x00, 0x00, 0x03, 0x00, 0x00, 0x5d, 0xb0, 0x02, 0x80,
                   0x80, 0x2d, 0x16, 0x59, 0x59, 0xa4, 0x93, 0x2b, 0x80, 0x40,
                   0x00, 0x00, 0x03, 0x00, 0x40, 0x00, 0x00, 0x07, 0x82};
  uint8_t pps[] = {0x44, 0x02, 0xa4, 0x04, 0x55, 0xa2, 0x6d, 0xce, 0xc0, 0xc3,
                   0xed, 0x0b, 0xac, 0xbc, 0x00, 0xc4, 0x44, 0x2e, 0xf7, 0x55,
                   0xfd, 0x05, 0x86, 0x92, 0x19, 0xdf, 0x58, 0xec, 0x38, 0x36,
                   0xb7, 0x7c, 0x00, 0x15, 0x33, 0x78, 0x03, 0x67, 0x26, 0x0f,
                   0x7b, 0x30, 0x1c, 0xd7, 0xd4, 0x3a, 0xec, 0xad, 0xef, 0x73};
  uint8_t idr[] = {0x26, 0x02, 0xaf, 0x08, 0x4a, 0x31, 0x11, 0x15, 0xe5, 0xc0};

  Buffer packet;
  packet.AppendData(payload_header);
  packet.AppendData(vps_nalu_size);
  packet.AppendData(vps);
  packet.AppendData(sps_nalu_size);
  packet.AppendData(sps);
  packet.AppendData(pps_nalu_size);
  packet.AppendData(pps);
  packet.AppendData(slice_nalu_size);
  packet.AppendData(idr);

  Buffer expected_packet;
  expected_packet.AppendData(start_code);
  expected_packet.AppendData(vps);
  expected_packet.AppendData(start_code);
  expected_packet.AppendData(sps);
  expected_packet.AppendData(start_code);
  expected_packet.AppendData(pps);
  expected_packet.AppendData(start_code);
  expected_packet.AppendData(idr);

  // clang-format on
  CopyOnWriteBuffer rtp_payload(packet);

  VideoRtpDepacketizerH265 depacketizer;
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(rtp_payload);
  ASSERT_TRUE(parsed);

  EXPECT_THAT(MakeArrayView(parsed->video_payload.cdata(),
                            parsed->video_payload.size()),
              ElementsAreArray(expected_packet));
  EXPECT_EQ(parsed->video_header.frame_type, VideoFrameType::kVideoFrameKey);
  EXPECT_EQ(parsed->video_header.codec, kVideoCodecH265);
  EXPECT_TRUE(parsed->video_header.is_first_packet_in_frame);
}

TEST(VideoRtpDepacketizerH265Test, ApNaluSpsWithResolution) {
  uint8_t payload_header[] = {0x60, 0x02};
  uint8_t vps_nalu_size[] = {0, 0x17};
  uint8_t sps_nalu_size[] = {0, 0x27};
  uint8_t pps_nalu_size[] = {0, 0x32};
  uint8_t slice_nalu_size[] = {0, 0xa};
  uint8_t start_code[] = {0x00, 0x00, 0x00, 0x01};
  // The VPS/SPS/PPS/IDR bytes are generated using the same way as above case.
  uint8_t vps[] = {
      0x40, 0x02, 0x1c, 0x01, 0xff, 0xff, 0x04, 0x08, 0x00, 0x00, 0x03, 0x00,
      0x9d, 0x08, 0x00, 0x00, 0x03, 0x00, 0x00, 0x78, 0x95, 0x98, 0x09,
  };
  uint8_t sps[] = {0x42, 0x02, 0x01, 0x04, 0x08, 0x00, 0x00, 0x03, 0x00, 0x9d,
                   0x08, 0x00, 0x00, 0x03, 0x00, 0x00, 0x5d, 0xb0, 0x02, 0x80,
                   0x80, 0x2d, 0x16, 0x59, 0x59, 0xa4, 0x93, 0x2b, 0x80, 0x40,
                   0x00, 0x00, 0x03, 0x00, 0x40, 0x00, 0x00, 0x07, 0x82};
  uint8_t pps[] = {0x44, 0x02, 0xa4, 0x04, 0x55, 0xa2, 0x6d, 0xce, 0xc0, 0xc3,
                   0xed, 0x0b, 0xac, 0xbc, 0x00, 0xc4, 0x44, 0x2e, 0xf7, 0x55,
                   0xfd, 0x05, 0x86, 0x92, 0x19, 0xdf, 0x58, 0xec, 0x38, 0x36,
                   0xb7, 0x7c, 0x00, 0x15, 0x33, 0x78, 0x03, 0x67, 0x26, 0x0f,
                   0x7b, 0x30, 0x1c, 0xd7, 0xd4, 0x3a, 0xec, 0xad, 0xef, 0x73};
  uint8_t idr[] = {0x26, 0x02, 0xaf, 0x08, 0x4a, 0x31, 0x11, 0x15, 0xe5, 0xc0};

  Buffer packet;
  packet.AppendData(payload_header);
  packet.AppendData(vps_nalu_size);
  packet.AppendData(vps);
  packet.AppendData(sps_nalu_size);
  packet.AppendData(sps);
  packet.AppendData(pps_nalu_size);
  packet.AppendData(pps);
  packet.AppendData(slice_nalu_size);
  packet.AppendData(idr);

  Buffer expected_packet;
  expected_packet.AppendData(start_code);
  expected_packet.AppendData(vps);
  expected_packet.AppendData(start_code);
  expected_packet.AppendData(sps);
  expected_packet.AppendData(start_code);
  expected_packet.AppendData(pps);
  expected_packet.AppendData(start_code);
  expected_packet.AppendData(idr);

  CopyOnWriteBuffer rtp_payload(packet);

  VideoRtpDepacketizerH265 depacketizer;
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(rtp_payload);
  ASSERT_TRUE(parsed);

  EXPECT_THAT(MakeArrayView(parsed->video_payload.cdata(),
                            parsed->video_payload.size()),
              ElementsAreArray(expected_packet));
  EXPECT_EQ(parsed->video_header.frame_type, VideoFrameType::kVideoFrameKey);
  EXPECT_EQ(parsed->video_header.codec, kVideoCodecH265);
  EXPECT_TRUE(parsed->video_header.is_first_packet_in_frame);
  EXPECT_EQ(parsed->video_header.width, 1280u);
  EXPECT_EQ(parsed->video_header.height, 720u);
}

TEST(VideoRtpDepacketizerH265Test, EmptyApRejected) {
  uint8_t lone_empty_packet[] = {0x60, 0x02,  // F=0, Type=48 (kH265Ap).
                                 0x00, 0x00};
  uint8_t leading_empty_packet[] = {0x60, 0x02,  // F=0, Type=48 (kH265Ap).
                                    0x00, 0x00, 0x00, 0x05, 0x26,
                                    0x02, 0xFF, 0x00, 0x11};  // kIdrWRadl
  uint8_t middle_empty_packet[] = {0x60, 0x02,  // F=0, Type=48 (kH265Ap).
                                   0x00, 0x04, 0x26, 0x02, 0xFF,
                                   0x00, 0x00, 0x00, 0x00, 0x05,
                                   0x26, 0x02, 0xFF, 0x00, 0x11};  // kIdrWRadl
  uint8_t trailing_empty_packet[] = {0x60, 0x02,  // F=0, Type=48 (kH265Ap).
                                     0x00, 0x04, 0x26,
                                     0x02, 0xFF, 0x00,  // kIdrWRadl
                                     0x00, 0x00};

  VideoRtpDepacketizerH265 depacketizer;
  EXPECT_FALSE(depacketizer.Parse(CopyOnWriteBuffer(lone_empty_packet)));
  EXPECT_FALSE(depacketizer.Parse(CopyOnWriteBuffer(leading_empty_packet)));
  EXPECT_FALSE(depacketizer.Parse(CopyOnWriteBuffer(middle_empty_packet)));
  EXPECT_FALSE(depacketizer.Parse(CopyOnWriteBuffer(trailing_empty_packet)));
}

TEST(VideoRtpDepacketizerH265Test, ApDelta) {
  uint8_t packet[20] = {0x60, 0x02,  // F=0, Type=48 (kH265Ap).
                                     // Length, nal header, payload.
                        0, 0x03, 0x02, 0x02, 0xFF,               // TrailR
                        0, 0x04, 0x02, 0x02, 0xFF, 0x00,         // TrailR
                        0, 0x05, 0x02, 0x02, 0xFF, 0x00, 0x11};  // TrailR
  uint8_t expected_packet[] = {
      0x00, 0x00, 0x00, 0x01, 0x02, 0x02, 0xFF,               // TrailR
      0x00, 0x00, 0x00, 0x01, 0x02, 0x02, 0xFF, 0x00,         // TrailR
      0x00, 0x00, 0x00, 0x01, 0x02, 0x02, 0xFF, 0x00, 0x11};  // TrailR
  CopyOnWriteBuffer rtp_payload(packet);

  VideoRtpDepacketizerH265 depacketizer;
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(rtp_payload);
  ASSERT_TRUE(parsed);

  EXPECT_THAT(MakeArrayView(parsed->video_payload.cdata(),
                            parsed->video_payload.size()),
              ElementsAreArray(expected_packet));

  EXPECT_EQ(parsed->video_header.frame_type, VideoFrameType::kVideoFrameDelta);
  EXPECT_EQ(parsed->video_header.codec, kVideoCodecH265);
  EXPECT_TRUE(parsed->video_header.is_first_packet_in_frame);
}

TEST(VideoRtpDepacketizerH265Test, Fu) {
  // clang-format off
  uint8_t packet1[] = {
      0x62, 0x02,  // F=0, Type=49 (kH265Fu).
      0x93,  // FU header kH265SBitMask | H265::kIdrWRadl.
      0xaf, 0x08, 0x4a, 0x31, 0x11, 0x15, 0xe5, 0xc0  // Payload.
  };
  // clang-format on
  // F=0, Type=19, (kIdrWRadl), tid=1, nalu header: 00100110 00000010, which is
  // 0x26, 0x02
  const uint8_t kExpected1[] = {0x00, 0x00, 0x00, 0x01, 0x26, 0x02, 0xaf,
                                0x08, 0x4a, 0x31, 0x11, 0x15, 0xe5, 0xc0};

  uint8_t packet2[] = {
      0x62, 0x02,     // F=0, Type=49 (kH265Fu).
      H265::kBlaWLp,  // FU header.
      0x02            // Payload.
  };
  const uint8_t kExpected2[] = {0x02};

  uint8_t packet3[] = {
      0x62, 0x02,  // F=0, Type=49 (kH265Fu).
      0x53,        // FU header kH265EBitMask | H265::kIdrWRadl.
      0x03         // Payload.
  };
  const uint8_t kExpected3[] = {0x03};

  VideoRtpDepacketizerH265 depacketizer;
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed1 =
      depacketizer.Parse(CopyOnWriteBuffer(packet1));
  ASSERT_TRUE(parsed1);
  // We expect that the first packet is one byte shorter since the FU header
  // has been replaced by the original nal header.
  EXPECT_THAT(MakeArrayView(parsed1->video_payload.cdata(),
                            parsed1->video_payload.size()),
              ElementsAreArray(kExpected1));
  EXPECT_EQ(parsed1->video_header.frame_type, VideoFrameType::kVideoFrameKey);
  EXPECT_EQ(parsed1->video_header.codec, kVideoCodecH265);
  EXPECT_TRUE(parsed1->video_header.is_first_packet_in_frame);

  // Following packets will be 2 bytes shorter since they will only be appended
  // onto the first packet.
  auto parsed2 = depacketizer.Parse(CopyOnWriteBuffer(packet2));
  EXPECT_THAT(MakeArrayView(parsed2->video_payload.cdata(),
                            parsed2->video_payload.size()),
              ElementsAreArray(kExpected2));
  EXPECT_FALSE(parsed2->video_header.is_first_packet_in_frame);
  EXPECT_EQ(parsed2->video_header.frame_type, VideoFrameType::kVideoFrameKey);
  EXPECT_EQ(parsed2->video_header.codec, kVideoCodecH265);

  auto parsed3 = depacketizer.Parse(CopyOnWriteBuffer(packet3));
  EXPECT_THAT(MakeArrayView(parsed3->video_payload.cdata(),
                            parsed3->video_payload.size()),
              ElementsAreArray(kExpected3));
  EXPECT_FALSE(parsed3->video_header.is_first_packet_in_frame);
  EXPECT_EQ(parsed3->video_header.frame_type, VideoFrameType::kVideoFrameKey);
  EXPECT_EQ(parsed3->video_header.codec, kVideoCodecH265);
}

TEST(VideoRtpDepacketizerH265Test, EmptyPayload) {
  CopyOnWriteBuffer empty;
  VideoRtpDepacketizerH265 depacketizer;
  EXPECT_FALSE(depacketizer.Parse(empty));
}

TEST(VideoRtpDepacketizerH265Test, TruncatedFuNalu) {
  const uint8_t kPayload[] = {0x62};
  VideoRtpDepacketizerH265 depacketizer;
  EXPECT_FALSE(depacketizer.Parse(CopyOnWriteBuffer(kPayload)));
}

TEST(VideoRtpDepacketizerH265Test, TruncatedSingleApNalu) {
  const uint8_t kPayload[] = {0xe0, 0x02, 0x40};
  VideoRtpDepacketizerH265 depacketizer;
  EXPECT_FALSE(depacketizer.Parse(CopyOnWriteBuffer(kPayload)));
}

TEST(VideoRtpDepacketizerH265Test, ApPacketWithTruncatedNalUnits) {
  const uint8_t kPayload[] = {0x60, 0x02, 0xED, 0xDF};
  VideoRtpDepacketizerH265 depacketizer;
  EXPECT_FALSE(depacketizer.Parse(CopyOnWriteBuffer(kPayload)));
}

TEST(VideoRtpDepacketizerH265Test, TruncationJustAfterSingleApNalu) {
  const uint8_t kPayload[] = {0x60, 0x02, 0x40, 0x40};
  VideoRtpDepacketizerH265 depacketizer;
  EXPECT_FALSE(depacketizer.Parse(CopyOnWriteBuffer(kPayload)));
}

TEST(VideoRtpDepacketizerH265Test, ShortSpsPacket) {
  const uint8_t kPayload[] = {0x40, 0x80, 0x00};
  VideoRtpDepacketizerH265 depacketizer;
  EXPECT_TRUE(depacketizer.Parse(CopyOnWriteBuffer(kPayload)));
}

TEST(VideoRtpDepacketizerH265Test, InvalidNaluSizeApNalu) {
  const uint8_t kPayload[] = {0x60, 0x02,  // F=0, Type=48 (kH265Ap).
                                           // Length, nal header, payload.
                              0, 0xff, 0x02, 0x02, 0xFF,  // TrailR
                              0, 0x05, 0x02, 0x02, 0xFF, 0x00,
                              0x11};  // TrailR;
  VideoRtpDepacketizerH265 depacketizer;
  EXPECT_FALSE(depacketizer.Parse(CopyOnWriteBuffer(kPayload)));
}

TEST(VideoRtpDepacketizerH265Test, PrefixSeiSetsFirstPacketInFrame) {
  const uint8_t kPayload[] = {
      0x4e, 0x02,             // F=0, Type=39 (H265::kPrefixSei).
      0x03, 0x03, 0x03, 0x03  // Payload.
  };
  VideoRtpDepacketizerH265 depacketizer;
  auto parsed = depacketizer.Parse(CopyOnWriteBuffer(kPayload));
  ASSERT_TRUE(parsed.has_value());
  EXPECT_TRUE(parsed->video_header.is_first_packet_in_frame);
}

TEST(VideoRtpDepacketizerH265Test, ApVpsSpsPpsMultiIdrSlices) {
  uint8_t payload_header[] = {0x60, 0x02};
  uint8_t vps_nalu_size[] = {0, 0x17};
  uint8_t sps_nalu_size[] = {0, 0x27};
  uint8_t pps_nalu_size[] = {0, 0x32};
  uint8_t slice_nalu_size[] = {0, 0xa};
  uint8_t start_code[] = {0x00, 0x00, 0x00, 0x01};
  // The VPS/SPS/PPS/IDR bytes are generated using the same way as above case.
  // Slices are truncated to contain enough data for test.
  uint8_t vps[] = {0x40, 0x02, 0x1c, 0x01, 0xff, 0xff, 0x04, 0x08,
                   0x00, 0x00, 0x03, 0x00, 0x9d, 0x08, 0x00, 0x00,
                   0x03, 0x00, 0x00, 0x78, 0x95, 0x98, 0x09};
  uint8_t sps[] = {0x42, 0x02, 0x01, 0x04, 0x08, 0x00, 0x00, 0x03, 0x00, 0x9d,
                   0x08, 0x00, 0x00, 0x03, 0x00, 0x00, 0x5d, 0xb0, 0x02, 0x80,
                   0x80, 0x2d, 0x16, 0x59, 0x59, 0xa4, 0x93, 0x2b, 0x80, 0x40,
                   0x00, 0x00, 0x03, 0x00, 0x40, 0x00, 0x00, 0x07, 0x82};
  uint8_t pps[] = {0x44, 0x02, 0xa4, 0x04, 0x55, 0xa2, 0x6d, 0xce, 0xc0, 0xc3,
                   0xed, 0x0b, 0xac, 0xbc, 0x00, 0xc4, 0x44, 0x2e, 0xf7, 0x55,
                   0xfd, 0x05, 0x86, 0x92, 0x19, 0xdf, 0x58, 0xec, 0x38, 0x36,
                   0xb7, 0x7c, 0x00, 0x15, 0x33, 0x78, 0x03, 0x67, 0x26, 0x0f,
                   0x7b, 0x30, 0x1c, 0xd7, 0xd4, 0x3a, 0xec, 0xad, 0xef, 0x73};
  uint8_t idr_slice1[] = {0x28, 0x01, 0xac, 0x6d, 0xa0,
                          0x7b, 0x4c, 0xe2, 0x09, 0xef};
  uint8_t idr_slice2[] = {0x28, 0x01, 0x27, 0xf8, 0x63,
                          0x6d, 0x7b, 0x6f, 0xcf, 0xff};

  CopyOnWriteBuffer rtp_payload;
  rtp_payload.AppendData(payload_header);
  rtp_payload.AppendData(vps_nalu_size);
  rtp_payload.AppendData(vps);
  rtp_payload.AppendData(sps_nalu_size);
  rtp_payload.AppendData(sps);
  rtp_payload.AppendData(pps_nalu_size);
  rtp_payload.AppendData(pps);
  rtp_payload.AppendData(slice_nalu_size);
  rtp_payload.AppendData(idr_slice1);
  rtp_payload.AppendData(slice_nalu_size);
  rtp_payload.AppendData(idr_slice2);

  Buffer expected_packet;
  expected_packet.AppendData(start_code);
  expected_packet.AppendData(vps);
  expected_packet.AppendData(start_code);
  expected_packet.AppendData(sps);
  expected_packet.AppendData(start_code);
  expected_packet.AppendData(pps);
  expected_packet.AppendData(start_code);
  expected_packet.AppendData(idr_slice1);
  expected_packet.AppendData(start_code);
  expected_packet.AppendData(idr_slice2);

  VideoRtpDepacketizerH265 depacketizer;
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(rtp_payload);
  ASSERT_TRUE(parsed.has_value());

  EXPECT_THAT(MakeArrayView(parsed->video_payload.cdata(),
                            parsed->video_payload.size()),
              ElementsAreArray(expected_packet));
  EXPECT_EQ(parsed->video_header.frame_type, VideoFrameType::kVideoFrameKey);
  EXPECT_TRUE(parsed->video_header.is_first_packet_in_frame);
}

TEST(VideoRtpDepacketizerH265Test, ApMultiNonFirstSlicesFromSingleNonIdrFrame) {
  uint8_t payload_header[] = {0x60, 0x02};
  uint8_t slice_nalu_size[] = {0, 0xa};
  uint8_t start_code[] = {0x00, 0x00, 0x00, 0x01};
  // First few bytes of two non-IDR slices from the same frame, both with the
  // first_slice_segment_in_pic_flag set to 0.
  uint8_t non_idr_slice1[] = {0x02, 0x01, 0x23, 0xfc, 0x20,
                              0x42, 0xad, 0x1b, 0x68, 0xdf};
  uint8_t non_idr_slice2[] = {0x02, 0x01, 0x27, 0xf8, 0x20,
                              0x42, 0xad, 0x1b, 0x68, 0xe0};

  CopyOnWriteBuffer rtp_payload;
  rtp_payload.AppendData(payload_header);
  rtp_payload.AppendData(slice_nalu_size);
  rtp_payload.AppendData(non_idr_slice1);
  rtp_payload.AppendData(slice_nalu_size);
  rtp_payload.AppendData(non_idr_slice2);

  Buffer expected_packet;
  expected_packet.AppendData(start_code);
  expected_packet.AppendData(non_idr_slice1);
  expected_packet.AppendData(start_code);
  expected_packet.AppendData(non_idr_slice2);

  VideoRtpDepacketizerH265 depacketizer;
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(rtp_payload);
  ASSERT_TRUE(parsed.has_value());

  EXPECT_THAT(MakeArrayView(parsed->video_payload.cdata(),
                            parsed->video_payload.size()),
              ElementsAreArray(expected_packet));
  EXPECT_EQ(parsed->video_header.frame_type, VideoFrameType::kVideoFrameDelta);
  EXPECT_FALSE(parsed->video_header.is_first_packet_in_frame);
}

TEST(VideoRtpDepacketizerH265Test, ApFirstTwoSlicesFromSingleNonIdrFrame) {
  uint8_t payload_header[] = {0x60, 0x02};
  uint8_t slice_nalu_size[] = {0, 0xa};
  uint8_t start_code[] = {0x00, 0x00, 0x00, 0x01};
  // First few bytes of two non-IDR slices from the same frame, with the first
  // slice's first_slice_segment_in_pic_flag set to 1, and second set to 0.
  uint8_t non_idr_slice1[] = {0x02, 0x01, 0xa4, 0x08, 0x55,
                              0xa3, 0x6d, 0xcc, 0xcf, 0x26};
  uint8_t non_idr_slice2[] = {0x02, 0x01, 0x23, 0xfc, 0x20,
                              0x42, 0xad, 0x1b, 0x68, 0xdf};

  CopyOnWriteBuffer rtp_payload;
  rtp_payload.AppendData(payload_header);
  rtp_payload.AppendData(slice_nalu_size);
  rtp_payload.AppendData(non_idr_slice1);
  rtp_payload.AppendData(slice_nalu_size);
  rtp_payload.AppendData(non_idr_slice2);

  Buffer expected_packet;
  expected_packet.AppendData(start_code);
  expected_packet.AppendData(non_idr_slice1);
  expected_packet.AppendData(start_code);
  expected_packet.AppendData(non_idr_slice2);

  VideoRtpDepacketizerH265 depacketizer;
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(rtp_payload);
  ASSERT_TRUE(parsed.has_value());

  EXPECT_THAT(MakeArrayView(parsed->video_payload.cdata(),
                            parsed->video_payload.size()),
              ElementsAreArray(expected_packet));
  EXPECT_EQ(parsed->video_header.frame_type, VideoFrameType::kVideoFrameDelta);
  EXPECT_TRUE(parsed->video_header.is_first_packet_in_frame);
}

TEST(VideoRtpDepacketizerH265Test, SingleNaluFromIdrSecondSlice) {
  // First few bytes of the second slice of an IDR_N_LP nalu with
  // first_slice_segment_in_pic_flag set to 0.
  const uint8_t kPayload[] = {0x28, 0x01, 0x27, 0xf8, 0x63, 0x6d, 0x7b, 0x6f,
                              0xcf, 0xff, 0x0d, 0xf5, 0xc7, 0xfe, 0x57, 0x77,
                              0xdc, 0x29, 0x24, 0x89, 0x89, 0xea, 0xd1, 0x88};

  VideoRtpDepacketizerH265 depacketizer;
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(CopyOnWriteBuffer(kPayload));
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->video_header.frame_type, VideoFrameType::kVideoFrameKey);
  EXPECT_FALSE(parsed->video_header.is_first_packet_in_frame);
}

TEST(VideoRtpDepacketizerH265Test, SingleNaluFromNonIdrSecondSlice) {
  // First few bytes of the second slice of an TRAIL_R nalu with
  // first_slice_segment_in_pic_flag set to 0.
  const uint8_t kPayload[] = {0x02, 0x01, 0x23, 0xfc, 0x20, 0x22, 0xad, 0x13,
                              0x68, 0xce, 0xc3, 0x5a, 0x00, 0xdc, 0xeb, 0x86,
                              0x4b, 0x0b, 0xa7, 0x6a, 0xe1, 0x9c, 0x5c, 0xea};

  VideoRtpDepacketizerH265 depacketizer;
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(CopyOnWriteBuffer(kPayload));
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->video_header.frame_type, VideoFrameType::kVideoFrameDelta);
  EXPECT_FALSE(parsed->video_header.is_first_packet_in_frame);
}

TEST(VideoRtpDepacketizerH265Test, FuFromIdrFrameSecondSlice) {
  // First few bytes of the second slice of an IDR_N_LP nalu with
  // first_slice_segment_in_pic_flag set to 0.
  const uint8_t kPayload[] = {
      0x62, 0x02,  // F=0, Type=49 (H265::kFu).
      0x93,        // FU header kH265SBitMask | H265::kIdrWRadl.
      0x23, 0xfc, 0x20, 0x22, 0xad, 0x13, 0x68, 0xce, 0xc3, 0x5a, 0x00, 0xdc};

  VideoRtpDepacketizerH265 depacketizer;
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(CopyOnWriteBuffer(kPayload));
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->video_header.frame_type, VideoFrameType::kVideoFrameKey);
  EXPECT_FALSE(parsed->video_header.is_first_packet_in_frame);
}

TEST(VideoRtpDepacketizerH265Test, FuFromNonIdrFrameSecondSlice) {
  // First few bytes of the second slice of an TRAIL_R nalu with
  // first_slice_segment_in_pic_flag set to 0.
  const uint8_t kPayload[] = {0x62, 0x02,  // F=0, Type=49 (H265::kFu).
                              0x80,  // FU header kH265SBitMask | H265::kTrailR.
                              0x23, 0xfc, 0x20, 0x22, 0xad, 0x13,
                              0x68, 0xce, 0xc3, 0x5a, 0x00, 0xdc};

  VideoRtpDepacketizerH265 depacketizer;
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(CopyOnWriteBuffer(kPayload));
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->video_header.frame_type, VideoFrameType::kVideoFrameDelta);
  EXPECT_FALSE(parsed->video_header.is_first_packet_in_frame);
}

TEST(VideoRtpDepacketizerH265Test, AudSetsFirstPacketInFrame) {
  const uint8_t kPayload[] = {0x46, 0x01, 0x10};

  VideoRtpDepacketizerH265 depacketizer;
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(CopyOnWriteBuffer(kPayload));
  ASSERT_TRUE(parsed.has_value());
  EXPECT_TRUE(parsed->video_header.is_first_packet_in_frame);
}

TEST(VideoRtpDepacketizerH265Test, PpsSetsFirstPacketInFrame) {
  const uint8_t kPayload[] = {
      0x44, 0x02, 0xa4, 0x04, 0x55, 0xa2, 0x6d, 0xce, 0xc0, 0xc3,
      0xed, 0x0b, 0xac, 0xbc, 0x00, 0xc4, 0x44, 0x2e, 0xf7, 0x55,
      0xfd, 0x05, 0x86, 0x92, 0x19, 0xdf, 0x58, 0xec, 0x38, 0x36,
      0xb7, 0x7c, 0x00, 0x15, 0x33, 0x78, 0x03, 0x67, 0x26, 0x0f,
      0x7b, 0x30, 0x1c, 0xd7, 0xd4, 0x3a, 0xec, 0xad, 0xef, 0x73};

  VideoRtpDepacketizerH265 depacketizer;
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(CopyOnWriteBuffer(kPayload));
  ASSERT_TRUE(parsed.has_value());
  EXPECT_TRUE(parsed->video_header.is_first_packet_in_frame);
}

TEST(VideoRtpDepacketizerH265Test, SuffixSeiNotSetFirstPacketInFrame) {
  const uint8_t kPayload[] = {0x50, 0x01, 0x81, 0x01, 0x03, 0x80};

  VideoRtpDepacketizerH265 depacketizer;
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(CopyOnWriteBuffer(kPayload));
  ASSERT_TRUE(parsed.has_value());
  EXPECT_FALSE(parsed->video_header.is_first_packet_in_frame);
}

TEST(VideoRtpDepacketizerH265Test, EmptyNaluPayload) {
  const uint8_t kPayload[] = {0x48, 0x00};  // F=0, Type=36 (H265::kEos).
  VideoRtpDepacketizerH265 depacketizer;
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(CopyOnWriteBuffer(kPayload));
  ASSERT_TRUE(parsed.has_value());
}

}  // namespace
}  // namespace webrtc
