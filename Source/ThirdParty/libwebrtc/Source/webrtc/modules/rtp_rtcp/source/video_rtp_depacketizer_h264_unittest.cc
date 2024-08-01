/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/video_rtp_depacketizer_h264.h"

#include <cstdint>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "common_video/h264/h264_common.h"
#include "modules/rtp_rtcp/mocks/mock_rtp_rtcp.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtp_format_h264.h"
#include "rtc_base/copy_on_write_buffer.h"
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

enum Nalu {
  kSlice = 1,
  kIdr = 5,
  kSei = 6,
  kSps = 7,
  kPps = 8,
  kStapA = 24,
  kFuA = 28
};

constexpr uint8_t kOriginalSps[] = {kSps, 0x00, 0x00, 0x03, 0x03,
                                    0xF4, 0x05, 0x03, 0xC7, 0xC0};
constexpr uint8_t kRewrittenSps[] = {kSps, 0x00, 0x00, 0x03, 0x03,
                                     0xF4, 0x05, 0x03, 0xC7, 0xE0,
                                     0x1B, 0x41, 0x10, 0x8D, 0x00};
constexpr uint8_t kIdrOne[] = {kIdr, 0xFF, 0x00, 0x00, 0x04};
constexpr uint8_t kIdrTwo[] = {kIdr, 0xFF, 0x00, 0x11};

TEST(VideoRtpDepacketizerH264Test, SingleNalu) {
  uint8_t packet[2] = {0x05, 0xFF};  // F=0, NRI=0, Type=5 (IDR).
  rtc::CopyOnWriteBuffer rtp_payload(packet);

  VideoRtpDepacketizerH264 depacketizer;
  absl::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(rtp_payload);
  ASSERT_TRUE(parsed);

  EXPECT_EQ(parsed->video_payload, rtp_payload);
  EXPECT_EQ(parsed->video_header.frame_type, VideoFrameType::kVideoFrameKey);
  EXPECT_EQ(parsed->video_header.codec, kVideoCodecH264);
  EXPECT_TRUE(parsed->video_header.is_first_packet_in_frame);
  const RTPVideoHeaderH264& h264 =
      absl::get<RTPVideoHeaderH264>(parsed->video_header.video_type_header);
  EXPECT_EQ(h264.packetization_type, kH264SingleNalu);
  EXPECT_EQ(h264.nalu_type, kIdr);
}

TEST(VideoRtpDepacketizerH264Test, SingleNaluSpsWithResolution) {
  uint8_t packet[] = {kSps, 0x7A, 0x00, 0x1F, 0xBC, 0xD9, 0x40, 0x50,
                      0x05, 0xBA, 0x10, 0x00, 0x00, 0x03, 0x00, 0xC0,
                      0x00, 0x00, 0x03, 0x2A, 0xE0, 0xF1, 0x83, 0x25};
  rtc::CopyOnWriteBuffer rtp_payload(packet);

  VideoRtpDepacketizerH264 depacketizer;
  absl::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(rtp_payload);
  ASSERT_TRUE(parsed);

  EXPECT_EQ(parsed->video_payload, rtp_payload);
  EXPECT_EQ(parsed->video_header.frame_type, VideoFrameType::kVideoFrameKey);
  EXPECT_EQ(parsed->video_header.codec, kVideoCodecH264);
  EXPECT_TRUE(parsed->video_header.is_first_packet_in_frame);
  EXPECT_EQ(parsed->video_header.width, 1280u);
  EXPECT_EQ(parsed->video_header.height, 720u);
  const auto& h264 =
      absl::get<RTPVideoHeaderH264>(parsed->video_header.video_type_header);
  EXPECT_EQ(h264.packetization_type, kH264SingleNalu);
}

TEST(VideoRtpDepacketizerH264Test, StapAKey) {
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
  rtc::CopyOnWriteBuffer rtp_payload(packet);

  VideoRtpDepacketizerH264 depacketizer;
  absl::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(rtp_payload);
  ASSERT_TRUE(parsed);

  EXPECT_EQ(parsed->video_payload, rtp_payload);
  EXPECT_EQ(parsed->video_header.frame_type, VideoFrameType::kVideoFrameKey);
  EXPECT_EQ(parsed->video_header.codec, kVideoCodecH264);
  EXPECT_TRUE(parsed->video_header.is_first_packet_in_frame);
  const auto& h264 =
      absl::get<RTPVideoHeaderH264>(parsed->video_header.video_type_header);
  EXPECT_EQ(h264.packetization_type, kH264StapA);
  // NALU type for aggregated packets is the type of the first packet only.
  EXPECT_EQ(h264.nalu_type, kSps);
  EXPECT_THAT(h264.nalus, ElementsAreArray(kExpectedNalus));
}

TEST(VideoRtpDepacketizerH264Test, StapANaluSpsWithResolution) {
  uint8_t packet[] = {kStapA,  // F=0, NRI=0, Type=24.
                               // Length (2 bytes), nal header, payload.
                      0x00, 0x19, kSps, 0x7A, 0x00, 0x1F, 0xBC, 0xD9, 0x40,
                      0x50, 0x05, 0xBA, 0x10, 0x00, 0x00, 0x03, 0x00, 0xC0,
                      0x00, 0x00, 0x03, 0x2A, 0xE0, 0xF1, 0x83, 0x25, 0x80,
                      0x00, 0x03, kIdr, 0xFF, 0x00, 0x00, 0x04, kIdr, 0xFF,
                      0x00, 0x11};
  rtc::CopyOnWriteBuffer rtp_payload(packet);

  VideoRtpDepacketizerH264 depacketizer;
  absl::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(rtp_payload);
  ASSERT_TRUE(parsed);

  EXPECT_EQ(parsed->video_payload, rtp_payload);
  EXPECT_EQ(parsed->video_header.frame_type, VideoFrameType::kVideoFrameKey);
  EXPECT_EQ(parsed->video_header.codec, kVideoCodecH264);
  EXPECT_TRUE(parsed->video_header.is_first_packet_in_frame);
  EXPECT_EQ(parsed->video_header.width, 1280u);
  EXPECT_EQ(parsed->video_header.height, 720u);
  const auto& h264 =
      absl::get<RTPVideoHeaderH264>(parsed->video_header.video_type_header);
  EXPECT_EQ(h264.packetization_type, kH264StapA);
}

TEST(VideoRtpDepacketizerH264Test, EmptyStapARejected) {
  uint8_t lone_empty_packet[] = {kStapA, 0x00, 0x00};
  uint8_t leading_empty_packet[] = {kStapA, 0x00, 0x00, 0x00, 0x04,
                                    kIdr,   0xFF, 0x00, 0x11};
  uint8_t middle_empty_packet[] = {kStapA, 0x00, 0x03, kIdr, 0xFF, 0x00, 0x00,
                                   0x00,   0x00, 0x04, kIdr, 0xFF, 0x00, 0x11};
  uint8_t trailing_empty_packet[] = {kStapA, 0x00, 0x03, kIdr,
                                     0xFF,   0x00, 0x00, 0x00};

  VideoRtpDepacketizerH264 depacketizer;
  EXPECT_FALSE(depacketizer.Parse(rtc::CopyOnWriteBuffer(lone_empty_packet)));
  EXPECT_FALSE(
      depacketizer.Parse(rtc::CopyOnWriteBuffer(leading_empty_packet)));
  EXPECT_FALSE(depacketizer.Parse(rtc::CopyOnWriteBuffer(middle_empty_packet)));
  EXPECT_FALSE(
      depacketizer.Parse(rtc::CopyOnWriteBuffer(trailing_empty_packet)));
}

TEST(VideoRtpDepacketizerH264Test, DepacketizeWithRewriting) {
  rtc::CopyOnWriteBuffer in_buffer;
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

  VideoRtpDepacketizerH264 depacketizer;
  auto parsed = depacketizer.Parse(in_buffer);
  ASSERT_TRUE(parsed);
  EXPECT_THAT(rtc::MakeArrayView(parsed->video_payload.cdata(),
                                 parsed->video_payload.size()),
              ElementsAreArray(out_buffer));
}

TEST(VideoRtpDepacketizerH264Test, DepacketizeWithDoubleRewriting) {
  rtc::CopyOnWriteBuffer in_buffer;
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

  VideoRtpDepacketizerH264 depacketizer;
  auto parsed = depacketizer.Parse(in_buffer);
  ASSERT_TRUE(parsed);
  std::vector<uint8_t> expected_packet_payload(
      out_buffer.data(), &out_buffer.data()[out_buffer.size()]);
  EXPECT_THAT(rtc::MakeArrayView(parsed->video_payload.cdata(),
                                 parsed->video_payload.size()),
              ElementsAreArray(out_buffer));
}

TEST(VideoRtpDepacketizerH264Test, StapADelta) {
  uint8_t packet[16] = {kStapA,  // F=0, NRI=0, Type=24.
                                 // Length, nal header, payload.
                        0, 0x02, kSlice, 0xFF, 0, 0x03, kSlice, 0xFF, 0x00, 0,
                        0x04, kSlice, 0xFF, 0x00, 0x11};
  rtc::CopyOnWriteBuffer rtp_payload(packet);

  VideoRtpDepacketizerH264 depacketizer;
  absl::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(rtp_payload);
  ASSERT_TRUE(parsed);

  EXPECT_EQ(parsed->video_payload.size(), rtp_payload.size());
  EXPECT_EQ(parsed->video_payload.cdata(), rtp_payload.cdata());

  EXPECT_EQ(parsed->video_header.frame_type, VideoFrameType::kVideoFrameDelta);
  EXPECT_EQ(parsed->video_header.codec, kVideoCodecH264);
  EXPECT_TRUE(parsed->video_header.is_first_packet_in_frame);
  const RTPVideoHeaderH264& h264 =
      absl::get<RTPVideoHeaderH264>(parsed->video_header.video_type_header);
  EXPECT_EQ(h264.packetization_type, kH264StapA);
  // NALU type for aggregated packets is the type of the first packet only.
  EXPECT_EQ(h264.nalu_type, kSlice);
}

TEST(VideoRtpDepacketizerH264Test, FuA) {
  // clang-format off
  uint8_t packet1[] = {
      kFuA,              // F=0, NRI=0, Type=28.
      kH264SBit | kIdr,  // FU header.
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
      kFuA,              // F=0, NRI=0, Type=28.
      kH264EBit | kIdr,  // FU header.
      0x03               // Payload.
  };
  const uint8_t kExpected3[] = {0x03};

  VideoRtpDepacketizerH264 depacketizer;
  absl::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed1 =
      depacketizer.Parse(rtc::CopyOnWriteBuffer(packet1));
  ASSERT_TRUE(parsed1);
  // We expect that the first packet is one byte shorter since the FU-A header
  // has been replaced by the original nal header.
  EXPECT_THAT(rtc::MakeArrayView(parsed1->video_payload.cdata(),
                                 parsed1->video_payload.size()),
              ElementsAreArray(kExpected1));
  EXPECT_EQ(parsed1->video_header.frame_type, VideoFrameType::kVideoFrameKey);
  EXPECT_EQ(parsed1->video_header.codec, kVideoCodecH264);
  EXPECT_TRUE(parsed1->video_header.is_first_packet_in_frame);
  {
    const RTPVideoHeaderH264& h264 =
        absl::get<RTPVideoHeaderH264>(parsed1->video_header.video_type_header);
    EXPECT_EQ(h264.packetization_type, kH264FuA);
    EXPECT_EQ(h264.nalu_type, kIdr);
    ASSERT_THAT(h264.nalus, SizeIs(1));
    EXPECT_EQ(h264.nalus[0].type, static_cast<H264::NaluType>(kIdr));
    EXPECT_EQ(h264.nalus[0].sps_id, -1);
    EXPECT_EQ(h264.nalus[0].pps_id, 0);
  }

  // Following packets will be 2 bytes shorter since they will only be appended
  // onto the first packet.
  auto parsed2 = depacketizer.Parse(rtc::CopyOnWriteBuffer(packet2));
  EXPECT_THAT(rtc::MakeArrayView(parsed2->video_payload.cdata(),
                                 parsed2->video_payload.size()),
              ElementsAreArray(kExpected2));
  EXPECT_FALSE(parsed2->video_header.is_first_packet_in_frame);
  EXPECT_EQ(parsed2->video_header.codec, kVideoCodecH264);
  {
    const RTPVideoHeaderH264& h264 =
        absl::get<RTPVideoHeaderH264>(parsed2->video_header.video_type_header);
    EXPECT_EQ(h264.packetization_type, kH264FuA);
    EXPECT_EQ(h264.nalu_type, kIdr);
    // NALU info is only expected for the first FU-A packet.
    EXPECT_THAT(h264.nalus, IsEmpty());
  }

  auto parsed3 = depacketizer.Parse(rtc::CopyOnWriteBuffer(packet3));
  EXPECT_THAT(rtc::MakeArrayView(parsed3->video_payload.cdata(),
                                 parsed3->video_payload.size()),
              ElementsAreArray(kExpected3));
  EXPECT_FALSE(parsed3->video_header.is_first_packet_in_frame);
  EXPECT_EQ(parsed3->video_header.codec, kVideoCodecH264);
  {
    const RTPVideoHeaderH264& h264 =
        absl::get<RTPVideoHeaderH264>(parsed3->video_header.video_type_header);
    EXPECT_EQ(h264.packetization_type, kH264FuA);
    EXPECT_EQ(h264.nalu_type, kIdr);
    // NALU info is only expected for the first FU-A packet.
    EXPECT_THAT(h264.nalus, IsEmpty());
  }
}

TEST(VideoRtpDepacketizerH264Test, EmptyPayload) {
  rtc::CopyOnWriteBuffer empty;
  VideoRtpDepacketizerH264 depacketizer;
  EXPECT_FALSE(depacketizer.Parse(empty));
}

TEST(VideoRtpDepacketizerH264Test, TruncatedFuaNalu) {
  const uint8_t kPayload[] = {0x9c};
  VideoRtpDepacketizerH264 depacketizer;
  EXPECT_FALSE(depacketizer.Parse(rtc::CopyOnWriteBuffer(kPayload)));
}

TEST(VideoRtpDepacketizerH264Test, TruncatedSingleStapANalu) {
  const uint8_t kPayload[] = {0xd8, 0x27};
  VideoRtpDepacketizerH264 depacketizer;
  EXPECT_FALSE(depacketizer.Parse(rtc::CopyOnWriteBuffer(kPayload)));
}

TEST(VideoRtpDepacketizerH264Test, StapAPacketWithTruncatedNalUnits) {
  const uint8_t kPayload[] = {0x58, 0xCB, 0xED, 0xDF};
  VideoRtpDepacketizerH264 depacketizer;
  EXPECT_FALSE(depacketizer.Parse(rtc::CopyOnWriteBuffer(kPayload)));
}

TEST(VideoRtpDepacketizerH264Test, TruncationJustAfterSingleStapANalu) {
  const uint8_t kPayload[] = {0x38, 0x27, 0x27};
  VideoRtpDepacketizerH264 depacketizer;
  EXPECT_FALSE(depacketizer.Parse(rtc::CopyOnWriteBuffer(kPayload)));
}

TEST(VideoRtpDepacketizerH264Test, SeiPacket) {
  const uint8_t kPayload[] = {
      kSei,                   // F=0, NRI=0, Type=6.
      0x03, 0x03, 0x03, 0x03  // Payload.
  };
  VideoRtpDepacketizerH264 depacketizer;
  auto parsed = depacketizer.Parse(rtc::CopyOnWriteBuffer(kPayload));
  ASSERT_TRUE(parsed);
  const RTPVideoHeaderH264& h264 =
      absl::get<RTPVideoHeaderH264>(parsed->video_header.video_type_header);
  EXPECT_EQ(parsed->video_header.frame_type, VideoFrameType::kVideoFrameDelta);
  EXPECT_EQ(h264.packetization_type, kH264SingleNalu);
  EXPECT_EQ(h264.nalu_type, kSei);
  ASSERT_THAT(h264.nalus, SizeIs(1));
  EXPECT_EQ(h264.nalus[0].type, static_cast<H264::NaluType>(kSei));
  EXPECT_EQ(h264.nalus[0].sps_id, -1);
  EXPECT_EQ(h264.nalus[0].pps_id, -1);
}

TEST(VideoRtpDepacketizerH264Test, ShortSpsPacket) {
  const uint8_t kPayload[] = {0x27, 0x80, 0x00};
  VideoRtpDepacketizerH264 depacketizer;
  EXPECT_FALSE(depacketizer.Parse(rtc::CopyOnWriteBuffer(kPayload)));
}

TEST(VideoRtpDepacketizerH264Test, BadSps) {
  const uint8_t kPayload[] = {
      kSps, 0x42, 0x41, 0x2a, 0xd3, 0x93, 0xd3, 0x3b  // Payload.
  };
  VideoRtpDepacketizerH264 depacketizer;
  EXPECT_FALSE(depacketizer.Parse(rtc::CopyOnWriteBuffer(kPayload)));
}

TEST(VideoRtpDepacketizerH264Test, BadPps) {
  const uint8_t kPayload[] = {
      kPps,
      0x00  // Payload.
  };
  VideoRtpDepacketizerH264 depacketizer;
  EXPECT_FALSE(depacketizer.Parse(rtc::CopyOnWriteBuffer(kPayload)));
}

TEST(VideoRtpDepacketizerH264Test, BadSlice) {
  const uint8_t kPayload[] = {
      kIdr,
      0xc0  // Payload.
  };
  VideoRtpDepacketizerH264 depacketizer;
  EXPECT_FALSE(depacketizer.Parse(rtc::CopyOnWriteBuffer(kPayload)));
}

TEST(VideoRtpDepacketizerH264Test, StapASpsPpsMultiSlice) {
  // A STAP-A containing a black 320x192 key frame with multiple slices.
  const uint8_t kPayload[] = {
      // clang-format off
      0x67, 0x42, 0xc0, 0x15, 0x8c, 0x68, 0x14, 0x19,  // SPS.
      0x79, 0xe0, 0x1e, 0x11, 0x08, 0xd4, 0x00, 0x04, 0x68, 0xce, 0x3c, 0x80,
      0x00, 0x2e,  // PPS.
      // Slices.
      0x65, 0xb8, 0x00, 0x04, 0x08, 0x79, 0x31, 0x40, 0x00, 0x42, 0xae, 0x4d,
      0xc9, 0xc9, 0xc9, 0xc9, 0xc9, 0xc9, 0xc9, 0xc9, 0xc9, 0xc9, 0xc9, 0xc9,
      0xc9, 0xc9, 0xc9, 0xc9, 0xc9, 0xc9, 0xd6, 0xeb, 0xae, 0xba, 0xeb, 0xae,
      0xba, 0xeb, 0xae, 0xba, 0xeb, 0xae, 0xba, 0xeb, 0xae, 0xbc, 0x00, 0x2f,
      0x65, 0x05, 0x2e, 0x00, 0x01, 0x02, 0x1e, 0x4c, 0x50, 0x00, 0x10, 0xab,
      0x93, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72,
      0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x75, 0xba, 0xeb, 0xae, 0xba,
      0xeb, 0xae, 0xba, 0xeb, 0xae, 0xba, 0xeb, 0xae, 0xba, 0xeb, 0xaf, 0x00,
      0x30, 0x65, 0x02, 0x8b, 0x80, 0x00, 0x40, 0x87, 0x93, 0x14, 0x00, 0x04,
      0x2a, 0xe4, 0xdc, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c,
      0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9d, 0x6e, 0xba, 0xeb,
      0xae, 0xba, 0xeb, 0xae, 0xba, 0xeb, 0xae, 0xba, 0xeb, 0xae, 0xba, 0xeb,
      0xc0, 0x00, 0x30, 0x65, 0x03, 0xcb, 0x80, 0x00, 0x40, 0x87, 0x93, 0x14,
      0x00, 0x04, 0x2a, 0xe4, 0xdc, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c,
      0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9d, 0x6e,
      0xba, 0xeb, 0xae, 0xba, 0xeb, 0xae, 0xba, 0xeb, 0xae, 0xba, 0xeb, 0xae,
      0xba, 0xeb, 0xc0, 0x00, 0x30, 0x65, 0x01, 0x42, 0xe0, 0x00, 0x10, 0x21,
      0xe4, 0xc5, 0x00, 0x01, 0x0a, 0xb9, 0x37, 0x27, 0x27, 0x27, 0x27, 0x27,
      0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27,
      0x27, 0x5b, 0xae, 0xba, 0xeb, 0xae, 0xba, 0xeb, 0xae, 0xba, 0xeb, 0xae,
      0xba, 0xeb, 0xae, 0xba, 0xf0, 0x00, 0x30, 0x65, 0x01, 0x92, 0xe0, 0x00,
      0x10, 0x21, 0xe4, 0xc5, 0x00, 0x01, 0x0a, 0xb9, 0x37, 0x27, 0x27, 0x27,
      0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27,
      0x27, 0x27, 0x27, 0x5b, 0xae, 0xba, 0xeb, 0xae, 0xba, 0xeb, 0xae, 0xba,
      0xeb, 0xae, 0xba, 0xeb, 0xae, 0xba, 0xf0
      // clang-format on
  };

  VideoRtpDepacketizerH264 depacketizer;
  absl::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(rtc::CopyOnWriteBuffer(kPayload));
  ASSERT_TRUE(parsed);
  EXPECT_TRUE(parsed->video_header.is_first_packet_in_frame);
}

TEST(VideoRtpDepacketizerH264Test, SecondSliceIdrNalu) {
  // First few bytes of a second slice of an IDR nalu with
  // first_mb_in_slice = 480.
  const uint8_t kPayload[] = {
      // clang-format off
      0x65, 0x00, 0xf0, 0x88, 0x82, 0x01, 0x3b, 0xff, 0xdf, 0xfe, 0x0b, 0xbb,
      0xfc, 0xb4, 0x30, 0xd1, 0x00, 0xef, 0xfd, 0xef, 0x0e, 0x79, 0x8b, 0x74,
      0x9b, 0x44, 0xf3, 0xb8, 0x65, 0x8f, 0xa1, 0x92, 0x30, 0xf9, 0x40, 0x06,
      0xb0, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00,
      0x03, 0x00, 0x18, 0x87, 0x4f, 0x6a, 0xfe, 0x60, 0x03, 0x9f, 0xfe, 0xd8,
      0x8b, 0xa6, 0x67, 0x31
      // clang-format on
  };

  VideoRtpDepacketizerH264 depacketizer;
  absl::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(rtc::CopyOnWriteBuffer(kPayload));
  ASSERT_TRUE(parsed);
  EXPECT_FALSE(parsed->video_header.is_first_packet_in_frame);
}

}  // namespace
}  // namespace webrtc
