/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_video_header.h"

#include "api/video/video_frame_metadata.h"
#include "api/video/video_frame_type.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

TEST(RTPVideoHeaderTest, FrameType_GetAsMetadata) {
  RTPVideoHeader video_header;
  video_header.frame_type = VideoFrameType::kVideoFrameKey;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_EQ(metadata.GetFrameType(), VideoFrameType::kVideoFrameKey);
}

TEST(RTPVideoHeaderTest, FrameType_FromMetadata) {
  VideoFrameMetadata metadata;
  metadata.SetFrameType(VideoFrameType::kVideoFrameKey);
  RTPVideoHeader video_header = RTPVideoHeader::FromMetadata(metadata);
  EXPECT_EQ(video_header.frame_type, VideoFrameType::kVideoFrameKey);
}

TEST(RTPVideoHeaderTest, Width_GetAsMetadata) {
  RTPVideoHeader video_header;
  video_header.width = 1280u;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_EQ(metadata.GetWidth(), 1280u);
}

TEST(RTPVideoHeaderTest, Width_FromMetadata) {
  VideoFrameMetadata metadata;
  metadata.SetWidth(1280u);
  RTPVideoHeader video_header = RTPVideoHeader::FromMetadata(metadata);
  EXPECT_EQ(video_header.width, 1280u);
}

TEST(RTPVideoHeaderTest, Height_GetAsMetadata) {
  RTPVideoHeader video_header;
  video_header.height = 720u;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_EQ(metadata.GetHeight(), 720u);
}

TEST(RTPVideoHeaderTest, Height_FromMetadata) {
  VideoFrameMetadata metadata;
  metadata.SetHeight(720u);
  RTPVideoHeader video_header = RTPVideoHeader::FromMetadata(metadata);
  EXPECT_EQ(video_header.height, 720u);
}

TEST(RTPVideoHeaderTest, Rotation_GetAsMetadata) {
  RTPVideoHeader video_header;
  video_header.rotation = VideoRotation::kVideoRotation_90;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_EQ(metadata.GetRotation(), VideoRotation::kVideoRotation_90);
}

TEST(RTPVideoHeaderTest, Rotation_FromMetadata) {
  VideoFrameMetadata metadata;
  metadata.SetRotation(VideoRotation::kVideoRotation_90);
  RTPVideoHeader video_header = RTPVideoHeader::FromMetadata(metadata);
  EXPECT_EQ(video_header.rotation, VideoRotation::kVideoRotation_90);
}

TEST(RTPVideoHeaderTest, ContentType_GetAsMetadata) {
  RTPVideoHeader video_header;
  video_header.content_type = VideoContentType::SCREENSHARE;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_EQ(metadata.GetContentType(), VideoContentType::SCREENSHARE);
}

TEST(RTPVideoHeaderTest, ContentType_FromMetadata) {
  VideoFrameMetadata metadata;
  metadata.SetContentType(VideoContentType::SCREENSHARE);
  RTPVideoHeader video_header = RTPVideoHeader::FromMetadata(metadata);
  EXPECT_EQ(video_header.content_type, VideoContentType::SCREENSHARE);
}

TEST(RTPVideoHeaderTest, FrameId_GetAsMetadata) {
  RTPVideoHeader video_header;
  RTPVideoHeader::GenericDescriptorInfo& generic =
      video_header.generic.emplace();
  generic.frame_id = 10;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_EQ(metadata.GetFrameId().value(), 10);
}

TEST(RTPVideoHeaderTest, FrameId_GetAsMetadataWhenGenericIsMissing) {
  RTPVideoHeader video_header;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  ASSERT_FALSE(video_header.generic);
  EXPECT_FALSE(metadata.GetFrameId().has_value());
}

TEST(RTPVideoHeaderTest, FrameId_FromMetadata) {
  VideoFrameMetadata metadata;
  metadata.SetFrameId(10);
  RTPVideoHeader video_header = RTPVideoHeader::FromMetadata(metadata);
  EXPECT_TRUE(video_header.generic.has_value());
  EXPECT_EQ(video_header.generic->frame_id, 10);
}

TEST(RTPVideoHeaderTest, FrameId_FromMetadataWhenFrameIdIsMissing) {
  VideoFrameMetadata metadata;
  metadata.SetFrameId(std::nullopt);
  RTPVideoHeader video_header = RTPVideoHeader::FromMetadata(metadata);
  EXPECT_FALSE(video_header.generic.has_value());
}

TEST(RTPVideoHeaderTest, SpatialIndex_GetAsMetadata) {
  RTPVideoHeader video_header;
  RTPVideoHeader::GenericDescriptorInfo& generic =
      video_header.generic.emplace();
  generic.spatial_index = 2;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_EQ(metadata.GetSpatialIndex(), 2);
}

TEST(RTPVideoHeaderTest, SpatialIndex_GetAsMetadataWhenGenericIsMissing) {
  RTPVideoHeader video_header;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  ASSERT_FALSE(video_header.generic);
  EXPECT_EQ(metadata.GetSpatialIndex(), 0);
}

TEST(RTPVideoHeaderTest, SpatialIndex_FromMetadata) {
  VideoFrameMetadata metadata;
  metadata.SetFrameId(123);  // Must have a frame ID for related properties.
  metadata.SetSpatialIndex(2);
  RTPVideoHeader video_header = RTPVideoHeader::FromMetadata(metadata);
  EXPECT_TRUE(video_header.generic.has_value());
  EXPECT_EQ(video_header.generic->spatial_index, 2);
}

TEST(RTPVideoHeaderTest, TemporalIndex_GetAsMetadata) {
  RTPVideoHeader video_header;
  RTPVideoHeader::GenericDescriptorInfo& generic =
      video_header.generic.emplace();
  generic.temporal_index = 3;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_EQ(metadata.GetTemporalIndex(), 3);
}

TEST(RTPVideoHeaderTest, TemporalIndex_GetAsMetadataWhenGenericIsMissing) {
  RTPVideoHeader video_header;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  ASSERT_FALSE(video_header.generic);
  EXPECT_EQ(metadata.GetTemporalIndex(), 0);
}

TEST(RTPVideoHeaderTest, TemporalIndex_FromMetadata) {
  VideoFrameMetadata metadata;
  metadata.SetFrameId(123);  // Must have a frame ID for related properties.
  metadata.SetTemporalIndex(3);
  RTPVideoHeader video_header = RTPVideoHeader::FromMetadata(metadata);
  EXPECT_TRUE(video_header.generic.has_value());
  EXPECT_EQ(video_header.generic->temporal_index, 3);
}

TEST(RTPVideoHeaderTest, FrameDependencies_GetAsMetadata) {
  RTPVideoHeader video_header;
  RTPVideoHeader::GenericDescriptorInfo& generic =
      video_header.generic.emplace();
  generic.dependencies = {5, 6, 7};
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_THAT(metadata.GetFrameDependencies(), ElementsAre(5, 6, 7));
}

TEST(RTPVideoHeaderTest, FrameDependency_GetAsMetadataWhenGenericIsMissing) {
  RTPVideoHeader video_header;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  ASSERT_FALSE(video_header.generic);
  EXPECT_THAT(metadata.GetFrameDependencies(), IsEmpty());
}

TEST(RTPVideoHeaderTest, FrameDependencies_FromMetadata) {
  VideoFrameMetadata metadata;
  absl::InlinedVector<int64_t, 5> dependencies = {5, 6, 7};
  metadata.SetFrameId(123);  // Must have a frame ID for related properties.
  metadata.SetFrameDependencies(dependencies);
  RTPVideoHeader video_header = RTPVideoHeader::FromMetadata(metadata);
  EXPECT_TRUE(video_header.generic.has_value());
  EXPECT_THAT(video_header.generic->dependencies, ElementsAre(5, 6, 7));
}

TEST(RTPVideoHeaderTest, DecodeTargetIndications_GetAsMetadata) {
  RTPVideoHeader video_header;
  RTPVideoHeader::GenericDescriptorInfo& generic =
      video_header.generic.emplace();
  generic.decode_target_indications = {DecodeTargetIndication::kSwitch};
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_THAT(metadata.GetDecodeTargetIndications(),
              ElementsAre(DecodeTargetIndication::kSwitch));
}

TEST(RTPVideoHeaderTest,
     DecodeTargetIndications_GetAsMetadataWhenGenericIsMissing) {
  RTPVideoHeader video_header;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  ASSERT_FALSE(video_header.generic);
  EXPECT_THAT(metadata.GetDecodeTargetIndications(), IsEmpty());
}

TEST(RTPVideoHeaderTest, DecodeTargetIndications_FromMetadata) {
  VideoFrameMetadata metadata;
  absl::InlinedVector<DecodeTargetIndication, 10> decode_target_indications = {
      DecodeTargetIndication::kSwitch};
  metadata.SetFrameId(123);  // Must have a frame ID for related properties.
  metadata.SetDecodeTargetIndications(decode_target_indications);
  RTPVideoHeader video_header = RTPVideoHeader::FromMetadata(metadata);
  EXPECT_TRUE(video_header.generic.has_value());
  EXPECT_THAT(video_header.generic->decode_target_indications,
              ElementsAre(DecodeTargetIndication::kSwitch));
}

TEST(RTPVideoHeaderTest, IsLastFrameInPicture_GetAsMetadata) {
  RTPVideoHeader video_header;
  video_header.is_last_frame_in_picture = false;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_FALSE(metadata.GetIsLastFrameInPicture());
}

TEST(RTPVideoHeaderTest, IsLastFrameInPicture_FromMetadata) {
  VideoFrameMetadata metadata;
  metadata.SetIsLastFrameInPicture(false);
  RTPVideoHeader video_header = RTPVideoHeader::FromMetadata(metadata);
  EXPECT_FALSE(video_header.is_last_frame_in_picture);
}

TEST(RTPVideoHeaderTest, SimulcastIdx_GetAsMetadata) {
  RTPVideoHeader video_header;
  video_header.simulcastIdx = 123;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_EQ(metadata.GetSimulcastIdx(), 123);
}

TEST(RTPVideoHeaderTest, SimulcastIdx_FromMetadata) {
  VideoFrameMetadata metadata;
  metadata.SetSimulcastIdx(123);
  RTPVideoHeader video_header = RTPVideoHeader::FromMetadata(metadata);
  EXPECT_EQ(video_header.simulcastIdx, 123);
}

TEST(RTPVideoHeaderTest, Codec_GetAsMetadata) {
  RTPVideoHeader video_header;
  video_header.codec = VideoCodecType::kVideoCodecVP9;
  video_header.video_type_header = RTPVideoHeaderVP9();
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_EQ(metadata.GetCodec(), VideoCodecType::kVideoCodecVP9);
}

TEST(RTPVideoHeaderTest, Codec_FromMetadata) {
  VideoFrameMetadata metadata;
  metadata.SetCodec(VideoCodecType::kVideoCodecVP9);
  metadata.SetRTPVideoHeaderCodecSpecifics(RTPVideoHeaderVP9());
  RTPVideoHeader video_header = RTPVideoHeader::FromMetadata(metadata);
  EXPECT_EQ(video_header.codec, VideoCodecType::kVideoCodecVP9);
}

TEST(RTPVideoHeaderTest, RTPVideoHeaderCodecSpecifics_GetAsMetadata) {
  RTPVideoHeader video_header;
  {
    video_header.codec = VideoCodecType::kVideoCodecVP8;
    RTPVideoHeaderVP8 vp8_specifics;
    vp8_specifics.InitRTPVideoHeaderVP8();
    vp8_specifics.pictureId = 42;
    video_header.video_type_header = vp8_specifics;
    VideoFrameMetadata metadata = video_header.GetAsMetadata();
    EXPECT_EQ(
        absl::get<RTPVideoHeaderVP8>(metadata.GetRTPVideoHeaderCodecSpecifics())
            .pictureId,
        vp8_specifics.pictureId);
  }
  {
    video_header.codec = VideoCodecType::kVideoCodecVP9;
    RTPVideoHeaderVP9 vp9_specifics;
    vp9_specifics.InitRTPVideoHeaderVP9();
    vp9_specifics.max_picture_id = 42;
    video_header.video_type_header = vp9_specifics;
    VideoFrameMetadata metadata = video_header.GetAsMetadata();
    EXPECT_EQ(
        absl::get<RTPVideoHeaderVP9>(metadata.GetRTPVideoHeaderCodecSpecifics())
            .max_picture_id,
        vp9_specifics.max_picture_id);
  }
  {
    video_header.codec = VideoCodecType::kVideoCodecH264;
    RTPVideoHeaderH264 h264_specifics;
    h264_specifics.nalu_type = 42;
    video_header.video_type_header = h264_specifics;
    VideoFrameMetadata metadata = video_header.GetAsMetadata();
    EXPECT_EQ(absl::get<RTPVideoHeaderH264>(
                  metadata.GetRTPVideoHeaderCodecSpecifics())
                  .nalu_type,
              h264_specifics.nalu_type);
  }
}

TEST(RTPVideoHeaderTest, RTPVideoHeaderCodecSpecifics_FromMetadata) {
  VideoFrameMetadata metadata;
  {
    metadata.SetCodec(VideoCodecType::kVideoCodecVP8);
    RTPVideoHeaderVP8 vp8_specifics;
    vp8_specifics.InitRTPVideoHeaderVP8();
    vp8_specifics.pictureId = 42;
    metadata.SetRTPVideoHeaderCodecSpecifics(vp8_specifics);
    RTPVideoHeader video_header = RTPVideoHeader::FromMetadata(metadata);
    EXPECT_EQ(
        absl::get<RTPVideoHeaderVP8>(video_header.video_type_header).pictureId,
        42);
  }
  {
    metadata.SetCodec(VideoCodecType::kVideoCodecVP9);
    RTPVideoHeaderVP9 vp9_specifics;
    vp9_specifics.InitRTPVideoHeaderVP9();
    vp9_specifics.max_picture_id = 42;
    metadata.SetRTPVideoHeaderCodecSpecifics(vp9_specifics);
    RTPVideoHeader video_header = RTPVideoHeader::FromMetadata(metadata);
    EXPECT_EQ(absl::get<RTPVideoHeaderVP9>(video_header.video_type_header)
                  .max_picture_id,
              42);
  }
  {
    metadata.SetCodec(VideoCodecType::kVideoCodecH264);
    RTPVideoHeaderH264 h264_specifics;
    h264_specifics.nalu_type = 42;
    metadata.SetRTPVideoHeaderCodecSpecifics(h264_specifics);
    RTPVideoHeader video_header = RTPVideoHeader::FromMetadata(metadata);
    EXPECT_EQ(
        absl::get<RTPVideoHeaderH264>(video_header.video_type_header).nalu_type,
        42);
  }
}

}  // namespace
}  // namespace webrtc
