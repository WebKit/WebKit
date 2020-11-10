/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/video_frame_metadata.h"

#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

TEST(VideoFrameMetadata, GetWidthReturnsCorrectValue) {
  RTPVideoHeader video_header;
  video_header.width = 1280u;
  VideoFrameMetadata metadata(video_header);
  EXPECT_EQ(metadata.GetWidth(), video_header.width);
}

TEST(VideoFrameMetadata, GetHeightReturnsCorrectValue) {
  RTPVideoHeader video_header;
  video_header.height = 720u;
  VideoFrameMetadata metadata(video_header);
  EXPECT_EQ(metadata.GetHeight(), video_header.height);
}

TEST(VideoFrameMetadata, GetFrameIdReturnsCorrectValue) {
  RTPVideoHeader video_header;
  RTPVideoHeader::GenericDescriptorInfo& generic =
      video_header.generic.emplace();
  generic.frame_id = 10;
  VideoFrameMetadata metadata(video_header);
  EXPECT_EQ(metadata.GetFrameId().value(), 10);
}

TEST(VideoFrameMetadata, HasNoFrameIdForHeaderWithoutGeneric) {
  RTPVideoHeader video_header;
  VideoFrameMetadata metadata(video_header);
  ASSERT_FALSE(video_header.generic);
  EXPECT_EQ(metadata.GetFrameId(), absl::nullopt);
}

TEST(VideoFrameMetadata, GetSpatialIndexReturnsCorrectValue) {
  RTPVideoHeader video_header;
  RTPVideoHeader::GenericDescriptorInfo& generic =
      video_header.generic.emplace();
  generic.spatial_index = 2;
  VideoFrameMetadata metadata(video_header);
  EXPECT_EQ(metadata.GetSpatialIndex(), 2);
}

TEST(VideoFrameMetadata, SpatialIndexIsZeroForHeaderWithoutGeneric) {
  RTPVideoHeader video_header;
  VideoFrameMetadata metadata(video_header);
  ASSERT_FALSE(video_header.generic);
  EXPECT_EQ(metadata.GetSpatialIndex(), 0);
}

TEST(VideoFrameMetadata, GetTemporalIndexReturnsCorrectValue) {
  RTPVideoHeader video_header;
  RTPVideoHeader::GenericDescriptorInfo& generic =
      video_header.generic.emplace();
  generic.temporal_index = 3;
  VideoFrameMetadata metadata(video_header);
  EXPECT_EQ(metadata.GetTemporalIndex(), 3);
}

TEST(VideoFrameMetadata, TemporalIndexIsZeroForHeaderWithoutGeneric) {
  RTPVideoHeader video_header;
  VideoFrameMetadata metadata(video_header);
  ASSERT_FALSE(video_header.generic);
  EXPECT_EQ(metadata.GetTemporalIndex(), 0);
}

TEST(VideoFrameMetadata, GetFrameDependenciesReturnsCorrectValue) {
  RTPVideoHeader video_header;
  RTPVideoHeader::GenericDescriptorInfo& generic =
      video_header.generic.emplace();
  generic.dependencies = {5, 6, 7};
  VideoFrameMetadata metadata(video_header);
  EXPECT_THAT(metadata.GetFrameDependencies(), ElementsAre(5, 6, 7));
}

TEST(VideoFrameMetadata, FrameDependencyVectorIsEmptyForHeaderWithoutGeneric) {
  RTPVideoHeader video_header;
  VideoFrameMetadata metadata(video_header);
  ASSERT_FALSE(video_header.generic);
  EXPECT_THAT(metadata.GetFrameDependencies(), IsEmpty());
}

TEST(VideoFrameMetadata, GetDecodeTargetIndicationsReturnsCorrectValue) {
  RTPVideoHeader video_header;
  RTPVideoHeader::GenericDescriptorInfo& generic =
      video_header.generic.emplace();
  generic.decode_target_indications = {DecodeTargetIndication::kSwitch};
  VideoFrameMetadata metadata(video_header);
  EXPECT_THAT(metadata.GetDecodeTargetIndications(),
              ElementsAre(DecodeTargetIndication::kSwitch));
}

TEST(VideoFrameMetadata,
     DecodeTargetIndicationsVectorIsEmptyForHeaderWithoutGeneric) {
  RTPVideoHeader video_header;
  VideoFrameMetadata metadata(video_header);
  ASSERT_FALSE(video_header.generic);
  EXPECT_THAT(metadata.GetDecodeTargetIndications(), IsEmpty());
}

}  // namespace
}  // namespace webrtc
