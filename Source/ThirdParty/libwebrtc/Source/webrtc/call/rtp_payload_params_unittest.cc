/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/rtp_payload_params.h"

#include <string.h>

#include <map>
#include <set>

#include "absl/container/inlined_vector.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "api/transport/field_trial_based_config.h"
#include "api/video/video_content_type.h"
#include "api/video/video_rotation.h"
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "modules/video_coding/codecs/interface/common_constants.h"
#include "modules/video_coding/codecs/vp8/include/vp8_globals.h"
#include "modules/video_coding/codecs/vp9/include/vp9_globals.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "test/explicit_key_value_config.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::SizeIs;

namespace webrtc {
namespace {
const uint32_t kSsrc1 = 12345;
const uint32_t kSsrc2 = 23456;
const int16_t kPictureId = 123;
const int16_t kTl0PicIdx = 20;
const uint8_t kTemporalIdx = 1;
const int16_t kInitialPictureId1 = 222;
const int16_t kInitialTl0PicIdx1 = 99;
const int64_t kDontCare = 0;
}  // namespace

TEST(RtpPayloadParamsTest, InfoMappedToRtpVideoHeader_Vp8) {
  RtpPayloadState state2;
  state2.picture_id = kPictureId;
  state2.tl0_pic_idx = kTl0PicIdx;
  std::map<uint32_t, RtpPayloadState> states = {{kSsrc2, state2}};

  RtpPayloadParams params(kSsrc2, &state2, FieldTrialBasedConfig());
  EncodedImage encoded_image;
  encoded_image.rotation_ = kVideoRotation_90;
  encoded_image.content_type_ = VideoContentType::SCREENSHARE;
  encoded_image.SetSpatialIndex(1);

  CodecSpecificInfo codec_info;
  codec_info.codecType = kVideoCodecVP8;
  codec_info.codecSpecific.VP8.temporalIdx = 0;
  codec_info.codecSpecific.VP8.keyIdx = kNoKeyIdx;
  codec_info.codecSpecific.VP8.layerSync = false;
  codec_info.codecSpecific.VP8.nonReference = true;

  RTPVideoHeader header =
      params.GetRtpVideoHeader(encoded_image, &codec_info, kDontCare);

  codec_info.codecType = kVideoCodecVP8;
  codec_info.codecSpecific.VP8.temporalIdx = 1;
  codec_info.codecSpecific.VP8.layerSync = true;

  header = params.GetRtpVideoHeader(encoded_image, &codec_info, 1);

  EXPECT_EQ(kVideoRotation_90, header.rotation);
  EXPECT_EQ(VideoContentType::SCREENSHARE, header.content_type);
  EXPECT_EQ(1, header.simulcastIdx);
  EXPECT_EQ(kVideoCodecVP8, header.codec);
  const auto& vp8_header =
      absl::get<RTPVideoHeaderVP8>(header.video_type_header);
  EXPECT_EQ(kPictureId + 2, vp8_header.pictureId);
  EXPECT_EQ(kTemporalIdx, vp8_header.temporalIdx);
  EXPECT_EQ(kTl0PicIdx + 1, vp8_header.tl0PicIdx);
  EXPECT_EQ(kNoKeyIdx, vp8_header.keyIdx);
  EXPECT_TRUE(vp8_header.layerSync);
  EXPECT_TRUE(vp8_header.nonReference);
}

TEST(RtpPayloadParamsTest, InfoMappedToRtpVideoHeader_Vp9) {
  RtpPayloadState state;
  state.picture_id = kPictureId;
  state.tl0_pic_idx = kTl0PicIdx;
  RtpPayloadParams params(kSsrc1, &state, FieldTrialBasedConfig());

  EncodedImage encoded_image;
  encoded_image.rotation_ = kVideoRotation_90;
  encoded_image.content_type_ = VideoContentType::SCREENSHARE;
  encoded_image.SetSpatialIndex(0);
  CodecSpecificInfo codec_info;
  codec_info.codecType = kVideoCodecVP9;
  codec_info.codecSpecific.VP9.num_spatial_layers = 3;
  codec_info.codecSpecific.VP9.first_frame_in_picture = true;
  codec_info.codecSpecific.VP9.temporal_idx = 2;
  codec_info.end_of_picture = false;

  RTPVideoHeader header =
      params.GetRtpVideoHeader(encoded_image, &codec_info, kDontCare);

  EXPECT_EQ(kVideoRotation_90, header.rotation);
  EXPECT_EQ(VideoContentType::SCREENSHARE, header.content_type);
  EXPECT_EQ(kVideoCodecVP9, header.codec);
  EXPECT_FALSE(header.color_space);
  const auto& vp9_header =
      absl::get<RTPVideoHeaderVP9>(header.video_type_header);
  EXPECT_EQ(kPictureId + 1, vp9_header.picture_id);
  EXPECT_EQ(kTl0PicIdx, vp9_header.tl0_pic_idx);
  EXPECT_EQ(vp9_header.temporal_idx, codec_info.codecSpecific.VP9.temporal_idx);
  EXPECT_EQ(vp9_header.spatial_idx, encoded_image.SpatialIndex());
  EXPECT_EQ(vp9_header.num_spatial_layers,
            codec_info.codecSpecific.VP9.num_spatial_layers);
  EXPECT_EQ(vp9_header.end_of_picture, codec_info.end_of_picture);

  // Next spatial layer.
  codec_info.codecSpecific.VP9.first_frame_in_picture = false;
  codec_info.end_of_picture = true;

  encoded_image.SetSpatialIndex(1);
  ColorSpace color_space(
      ColorSpace::PrimaryID::kSMPTE170M, ColorSpace::TransferID::kSMPTE170M,
      ColorSpace::MatrixID::kSMPTE170M, ColorSpace::RangeID::kFull);
  encoded_image.SetColorSpace(color_space);
  header = params.GetRtpVideoHeader(encoded_image, &codec_info, kDontCare);

  EXPECT_EQ(kVideoRotation_90, header.rotation);
  EXPECT_EQ(VideoContentType::SCREENSHARE, header.content_type);
  EXPECT_EQ(kVideoCodecVP9, header.codec);
  EXPECT_EQ(absl::make_optional(color_space), header.color_space);
  EXPECT_EQ(kPictureId + 1, vp9_header.picture_id);
  EXPECT_EQ(kTl0PicIdx, vp9_header.tl0_pic_idx);
  EXPECT_EQ(vp9_header.temporal_idx, codec_info.codecSpecific.VP9.temporal_idx);
  EXPECT_EQ(vp9_header.spatial_idx, encoded_image.SpatialIndex());
  EXPECT_EQ(vp9_header.num_spatial_layers,
            codec_info.codecSpecific.VP9.num_spatial_layers);
  EXPECT_EQ(vp9_header.end_of_picture, codec_info.end_of_picture);
}

TEST(RtpPayloadParamsTest, PictureIdIsSetForVp8) {
  RtpPayloadState state;
  state.picture_id = kInitialPictureId1;
  state.tl0_pic_idx = kInitialTl0PicIdx1;

  EncodedImage encoded_image;
  CodecSpecificInfo codec_info;
  codec_info.codecType = kVideoCodecVP8;

  RtpPayloadParams params(kSsrc1, &state, FieldTrialBasedConfig());
  RTPVideoHeader header =
      params.GetRtpVideoHeader(encoded_image, &codec_info, kDontCare);
  EXPECT_EQ(kVideoCodecVP8, header.codec);
  EXPECT_EQ(kInitialPictureId1 + 1,
            absl::get<RTPVideoHeaderVP8>(header.video_type_header).pictureId);

  // State should hold latest used picture id and tl0_pic_idx.
  state = params.state();
  EXPECT_EQ(kInitialPictureId1 + 1, state.picture_id);
  EXPECT_EQ(kInitialTl0PicIdx1 + 1, state.tl0_pic_idx);
}

TEST(RtpPayloadParamsTest, PictureIdWraps) {
  RtpPayloadState state;
  state.picture_id = kMaxTwoBytePictureId;
  state.tl0_pic_idx = kInitialTl0PicIdx1;

  EncodedImage encoded_image;
  CodecSpecificInfo codec_info;
  codec_info.codecType = kVideoCodecVP8;
  codec_info.codecSpecific.VP8.temporalIdx = kNoTemporalIdx;

  RtpPayloadParams params(kSsrc1, &state, FieldTrialBasedConfig());
  RTPVideoHeader header =
      params.GetRtpVideoHeader(encoded_image, &codec_info, kDontCare);
  EXPECT_EQ(kVideoCodecVP8, header.codec);
  EXPECT_EQ(0,
            absl::get<RTPVideoHeaderVP8>(header.video_type_header).pictureId);

  // State should hold latest used picture id and tl0_pic_idx.
  EXPECT_EQ(0, params.state().picture_id);  // Wrapped.
  EXPECT_EQ(kInitialTl0PicIdx1, params.state().tl0_pic_idx);
}

TEST(RtpPayloadParamsTest, Tl0PicIdxUpdatedForVp8) {
  RtpPayloadState state;
  state.picture_id = kInitialPictureId1;
  state.tl0_pic_idx = kInitialTl0PicIdx1;

  EncodedImage encoded_image;
  // Modules are sending for this test.
  // OnEncodedImage, temporalIdx: 1.
  CodecSpecificInfo codec_info;
  codec_info.codecType = kVideoCodecVP8;
  codec_info.codecSpecific.VP8.temporalIdx = 1;

  RtpPayloadParams params(kSsrc1, &state, FieldTrialBasedConfig());
  RTPVideoHeader header =
      params.GetRtpVideoHeader(encoded_image, &codec_info, kDontCare);

  EXPECT_EQ(kVideoCodecVP8, header.codec);
  const auto& vp8_header =
      absl::get<RTPVideoHeaderVP8>(header.video_type_header);
  EXPECT_EQ(kInitialPictureId1 + 1, vp8_header.pictureId);
  EXPECT_EQ(kInitialTl0PicIdx1, vp8_header.tl0PicIdx);

  // OnEncodedImage, temporalIdx: 0.
  codec_info.codecSpecific.VP8.temporalIdx = 0;

  header = params.GetRtpVideoHeader(encoded_image, &codec_info, kDontCare);
  EXPECT_EQ(kVideoCodecVP8, header.codec);
  EXPECT_EQ(kInitialPictureId1 + 2, vp8_header.pictureId);
  EXPECT_EQ(kInitialTl0PicIdx1 + 1, vp8_header.tl0PicIdx);

  // State should hold latest used picture id and tl0_pic_idx.
  EXPECT_EQ(kInitialPictureId1 + 2, params.state().picture_id);
  EXPECT_EQ(kInitialTl0PicIdx1 + 1, params.state().tl0_pic_idx);
}

TEST(RtpPayloadParamsTest, Tl0PicIdxUpdatedForVp9) {
  RtpPayloadState state;
  state.picture_id = kInitialPictureId1;
  state.tl0_pic_idx = kInitialTl0PicIdx1;

  EncodedImage encoded_image;
  // Modules are sending for this test.
  // OnEncodedImage, temporalIdx: 1.
  CodecSpecificInfo codec_info;
  codec_info.codecType = kVideoCodecVP9;
  codec_info.codecSpecific.VP9.temporal_idx = 1;
  codec_info.codecSpecific.VP9.first_frame_in_picture = true;

  RtpPayloadParams params(kSsrc1, &state, FieldTrialBasedConfig());
  RTPVideoHeader header =
      params.GetRtpVideoHeader(encoded_image, &codec_info, kDontCare);

  EXPECT_EQ(kVideoCodecVP9, header.codec);
  const auto& vp9_header =
      absl::get<RTPVideoHeaderVP9>(header.video_type_header);
  EXPECT_EQ(kInitialPictureId1 + 1, vp9_header.picture_id);
  EXPECT_EQ(kInitialTl0PicIdx1, vp9_header.tl0_pic_idx);

  // OnEncodedImage, temporalIdx: 0.
  codec_info.codecSpecific.VP9.temporal_idx = 0;

  header = params.GetRtpVideoHeader(encoded_image, &codec_info, kDontCare);

  EXPECT_EQ(kVideoCodecVP9, header.codec);
  EXPECT_EQ(kInitialPictureId1 + 2, vp9_header.picture_id);
  EXPECT_EQ(kInitialTl0PicIdx1 + 1, vp9_header.tl0_pic_idx);

  // OnEncodedImage, first_frame_in_picture = false
  codec_info.codecSpecific.VP9.first_frame_in_picture = false;

  header = params.GetRtpVideoHeader(encoded_image, &codec_info, kDontCare);

  EXPECT_EQ(kVideoCodecVP9, header.codec);
  EXPECT_EQ(kInitialPictureId1 + 2, vp9_header.picture_id);
  EXPECT_EQ(kInitialTl0PicIdx1 + 1, vp9_header.tl0_pic_idx);

  // State should hold latest used picture id and tl0_pic_idx.
  EXPECT_EQ(kInitialPictureId1 + 2, params.state().picture_id);
  EXPECT_EQ(kInitialTl0PicIdx1 + 1, params.state().tl0_pic_idx);
}

TEST(RtpPayloadParamsTest, PictureIdForOldGenericFormat) {
  test::ScopedFieldTrials generic_picture_id(
      "WebRTC-GenericPictureId/Enabled/");
  RtpPayloadState state{};

  EncodedImage encoded_image;
  CodecSpecificInfo codec_info;
  codec_info.codecType = kVideoCodecGeneric;
  encoded_image._frameType = VideoFrameType::kVideoFrameKey;

  RtpPayloadParams params(kSsrc1, &state, FieldTrialBasedConfig());
  RTPVideoHeader header =
      params.GetRtpVideoHeader(encoded_image, &codec_info, 10);

  EXPECT_EQ(kVideoCodecGeneric, header.codec);
  const auto* generic =
      absl::get_if<RTPVideoHeaderLegacyGeneric>(&header.video_type_header);
  ASSERT_TRUE(generic);
  EXPECT_EQ(0, generic->picture_id);

  encoded_image._frameType = VideoFrameType::kVideoFrameDelta;
  header = params.GetRtpVideoHeader(encoded_image, &codec_info, 20);
  generic =
      absl::get_if<RTPVideoHeaderLegacyGeneric>(&header.video_type_header);
  ASSERT_TRUE(generic);
  EXPECT_EQ(1, generic->picture_id);
}

TEST(RtpPayloadParamsTest, GenericDescriptorForGenericCodec) {
  RtpPayloadState state;

  EncodedImage encoded_image;
  encoded_image._frameType = VideoFrameType::kVideoFrameKey;
  CodecSpecificInfo codec_info;
  codec_info.codecType = kVideoCodecGeneric;

  RtpPayloadParams params(kSsrc1, &state, FieldTrialBasedConfig());
  RTPVideoHeader header =
      params.GetRtpVideoHeader(encoded_image, &codec_info, 0);

  EXPECT_THAT(header.codec, Eq(kVideoCodecGeneric));

  ASSERT_TRUE(header.generic);
  EXPECT_THAT(header.generic->frame_id, Eq(0));
  EXPECT_THAT(header.generic->spatial_index, Eq(0));
  EXPECT_THAT(header.generic->temporal_index, Eq(0));
  EXPECT_THAT(header.generic->decode_target_indications,
              ElementsAre(DecodeTargetIndication::kSwitch));
  EXPECT_THAT(header.generic->dependencies, IsEmpty());
  EXPECT_THAT(header.generic->chain_diffs, ElementsAre(0));

  encoded_image._frameType = VideoFrameType::kVideoFrameDelta;
  header = params.GetRtpVideoHeader(encoded_image, &codec_info, 3);
  ASSERT_TRUE(header.generic);
  EXPECT_THAT(header.generic->frame_id, Eq(3));
  EXPECT_THAT(header.generic->spatial_index, Eq(0));
  EXPECT_THAT(header.generic->temporal_index, Eq(0));
  EXPECT_THAT(header.generic->dependencies, ElementsAre(0));
  EXPECT_THAT(header.generic->decode_target_indications,
              ElementsAre(DecodeTargetIndication::kSwitch));
  EXPECT_THAT(header.generic->chain_diffs, ElementsAre(3));
}

TEST(RtpPayloadParamsTest, SetsGenericFromGenericFrameInfo) {
  RtpPayloadState state;
  EncodedImage encoded_image;
  CodecSpecificInfo codec_info;

  RtpPayloadParams params(kSsrc1, &state, FieldTrialBasedConfig());

  encoded_image._frameType = VideoFrameType::kVideoFrameKey;
  codec_info.generic_frame_info =
      GenericFrameInfo::Builder().S(1).T(0).Dtis("S").Build();
  codec_info.generic_frame_info->encoder_buffers = {
      {/*id=*/0, /*referenced=*/false, /*updated=*/true}};
  codec_info.generic_frame_info->part_of_chain = {true, false};
  RTPVideoHeader key_header =
      params.GetRtpVideoHeader(encoded_image, &codec_info, /*frame_id=*/1);

  ASSERT_TRUE(key_header.generic);
  EXPECT_EQ(key_header.generic->spatial_index, 1);
  EXPECT_EQ(key_header.generic->temporal_index, 0);
  EXPECT_EQ(key_header.generic->frame_id, 1);
  EXPECT_THAT(key_header.generic->dependencies, IsEmpty());
  EXPECT_THAT(key_header.generic->decode_target_indications,
              ElementsAre(DecodeTargetIndication::kSwitch));
  EXPECT_THAT(key_header.generic->chain_diffs, SizeIs(2));

  encoded_image._frameType = VideoFrameType::kVideoFrameDelta;
  codec_info.generic_frame_info =
      GenericFrameInfo::Builder().S(2).T(3).Dtis("D").Build();
  codec_info.generic_frame_info->encoder_buffers = {
      {/*id=*/0, /*referenced=*/true, /*updated=*/false}};
  codec_info.generic_frame_info->part_of_chain = {false, false};
  RTPVideoHeader delta_header =
      params.GetRtpVideoHeader(encoded_image, &codec_info, /*frame_id=*/3);

  ASSERT_TRUE(delta_header.generic);
  EXPECT_EQ(delta_header.generic->spatial_index, 2);
  EXPECT_EQ(delta_header.generic->temporal_index, 3);
  EXPECT_EQ(delta_header.generic->frame_id, 3);
  EXPECT_THAT(delta_header.generic->dependencies, ElementsAre(1));
  EXPECT_THAT(delta_header.generic->decode_target_indications,
              ElementsAre(DecodeTargetIndication::kDiscardable));
  EXPECT_THAT(delta_header.generic->chain_diffs, SizeIs(2));
}

class RtpPayloadParamsVp8ToGenericTest : public ::testing::Test {
 public:
  enum LayerSync { kNoSync, kSync };

  RtpPayloadParamsVp8ToGenericTest()
      : state_(), params_(123, &state_, trials_config_) {}

  void ConvertAndCheck(int temporal_index,
                       int64_t shared_frame_id,
                       VideoFrameType frame_type,
                       LayerSync layer_sync,
                       const std::set<int64_t>& expected_deps,
                       uint16_t width = 0,
                       uint16_t height = 0) {
    EncodedImage encoded_image;
    encoded_image._frameType = frame_type;
    encoded_image._encodedWidth = width;
    encoded_image._encodedHeight = height;

    CodecSpecificInfo codec_info;
    codec_info.codecType = kVideoCodecVP8;
    codec_info.codecSpecific.VP8.temporalIdx = temporal_index;
    codec_info.codecSpecific.VP8.layerSync = layer_sync == kSync;

    RTPVideoHeader header =
        params_.GetRtpVideoHeader(encoded_image, &codec_info, shared_frame_id);

    ASSERT_TRUE(header.generic);
    EXPECT_EQ(header.generic->spatial_index, 0);

    EXPECT_EQ(header.generic->frame_id, shared_frame_id);
    EXPECT_EQ(header.generic->temporal_index, temporal_index);
    std::set<int64_t> actual_deps(header.generic->dependencies.begin(),
                                  header.generic->dependencies.end());
    EXPECT_EQ(expected_deps, actual_deps);

    EXPECT_EQ(header.width, width);
    EXPECT_EQ(header.height, height);
  }

 protected:
  FieldTrialBasedConfig trials_config_;
  RtpPayloadState state_;
  RtpPayloadParams params_;
};

TEST_F(RtpPayloadParamsVp8ToGenericTest, Keyframe) {
  ConvertAndCheck(0, 0, VideoFrameType::kVideoFrameKey, kNoSync, {}, 480, 360);
  ConvertAndCheck(0, 1, VideoFrameType::kVideoFrameDelta, kNoSync, {0});
  ConvertAndCheck(0, 2, VideoFrameType::kVideoFrameKey, kNoSync, {}, 480, 360);
}

TEST_F(RtpPayloadParamsVp8ToGenericTest, TooHighTemporalIndex) {
  ConvertAndCheck(0, 0, VideoFrameType::kVideoFrameKey, kNoSync, {}, 480, 360);

  EncodedImage encoded_image;
  encoded_image._frameType = VideoFrameType::kVideoFrameDelta;
  CodecSpecificInfo codec_info;
  codec_info.codecType = kVideoCodecVP8;
  codec_info.codecSpecific.VP8.temporalIdx =
      RtpGenericFrameDescriptor::kMaxTemporalLayers;
  codec_info.codecSpecific.VP8.layerSync = false;

  RTPVideoHeader header =
      params_.GetRtpVideoHeader(encoded_image, &codec_info, 1);
  EXPECT_FALSE(header.generic);
}

TEST_F(RtpPayloadParamsVp8ToGenericTest, LayerSync) {
  // 02120212 pattern
  ConvertAndCheck(0, 0, VideoFrameType::kVideoFrameKey, kNoSync, {}, 480, 360);
  ConvertAndCheck(2, 1, VideoFrameType::kVideoFrameDelta, kNoSync, {0});
  ConvertAndCheck(1, 2, VideoFrameType::kVideoFrameDelta, kNoSync, {0});
  ConvertAndCheck(2, 3, VideoFrameType::kVideoFrameDelta, kNoSync, {0, 1, 2});

  ConvertAndCheck(0, 4, VideoFrameType::kVideoFrameDelta, kNoSync, {0});
  ConvertAndCheck(2, 5, VideoFrameType::kVideoFrameDelta, kNoSync, {2, 3, 4});
  ConvertAndCheck(1, 6, VideoFrameType::kVideoFrameDelta, kSync,
                  {4});  // layer sync
  ConvertAndCheck(2, 7, VideoFrameType::kVideoFrameDelta, kNoSync, {4, 5, 6});
}

TEST_F(RtpPayloadParamsVp8ToGenericTest, FrameIdGaps) {
  // 0101 pattern
  ConvertAndCheck(0, 0, VideoFrameType::kVideoFrameKey, kNoSync, {}, 480, 360);
  ConvertAndCheck(1, 1, VideoFrameType::kVideoFrameDelta, kNoSync, {0});

  ConvertAndCheck(0, 5, VideoFrameType::kVideoFrameDelta, kNoSync, {0});
  ConvertAndCheck(1, 10, VideoFrameType::kVideoFrameDelta, kNoSync, {1, 5});

  ConvertAndCheck(0, 15, VideoFrameType::kVideoFrameDelta, kNoSync, {5});
  ConvertAndCheck(1, 20, VideoFrameType::kVideoFrameDelta, kNoSync, {10, 15});
}

TEST(RtpPayloadParamsVp9ToGenericTest, NoScalability) {
  RtpPayloadState state;
  RtpPayloadParams params(/*ssrc=*/123, &state, FieldTrialBasedConfig());

  EncodedImage encoded_image;
  CodecSpecificInfo codec_info;
  codec_info.codecType = kVideoCodecVP9;
  codec_info.codecSpecific.VP9.num_spatial_layers = 1;
  codec_info.codecSpecific.VP9.temporal_idx = kNoTemporalIdx;
  codec_info.codecSpecific.VP9.first_frame_in_picture = true;
  codec_info.end_of_picture = true;

  // Key frame.
  encoded_image._frameType = VideoFrameType::kVideoFrameKey;
  codec_info.codecSpecific.VP9.inter_pic_predicted = false;
  codec_info.codecSpecific.VP9.num_ref_pics = 0;
  RTPVideoHeader header = params.GetRtpVideoHeader(encoded_image, &codec_info,
                                                   /*shared_frame_id=*/1);

  ASSERT_TRUE(header.generic);
  EXPECT_EQ(header.generic->spatial_index, 0);
  EXPECT_EQ(header.generic->temporal_index, 0);
  EXPECT_EQ(header.generic->frame_id, 1);
  ASSERT_THAT(header.generic->decode_target_indications, Not(IsEmpty()));
  EXPECT_EQ(header.generic->decode_target_indications[0],
            DecodeTargetIndication::kSwitch);
  EXPECT_THAT(header.generic->dependencies, IsEmpty());
  EXPECT_THAT(header.generic->chain_diffs, ElementsAre(0));

  // Delta frame.
  encoded_image._frameType = VideoFrameType::kVideoFrameDelta;
  codec_info.codecSpecific.VP9.inter_pic_predicted = true;
  codec_info.codecSpecific.VP9.num_ref_pics = 1;
  codec_info.codecSpecific.VP9.p_diff[0] = 1;
  header = params.GetRtpVideoHeader(encoded_image, &codec_info,
                                    /*shared_frame_id=*/3);

  ASSERT_TRUE(header.generic);
  EXPECT_EQ(header.generic->spatial_index, 0);
  EXPECT_EQ(header.generic->temporal_index, 0);
  EXPECT_EQ(header.generic->frame_id, 3);
  ASSERT_THAT(header.generic->decode_target_indications, Not(IsEmpty()));
  EXPECT_EQ(header.generic->decode_target_indications[0],
            DecodeTargetIndication::kSwitch);
  EXPECT_THAT(header.generic->dependencies, ElementsAre(1));
  // previous frame in the chain was frame#1,
  EXPECT_THAT(header.generic->chain_diffs, ElementsAre(3 - 1));
}

TEST(RtpPayloadParamsVp9ToGenericTest, TemporalScalabilityWith2Layers) {
  // Test with 2 temporal layers structure that is not used by webrtc:
  //    1---3   5
  //   /   /   /   ...
  //  0---2---4---
  RtpPayloadState state;
  RtpPayloadParams params(/*ssrc=*/123, &state, FieldTrialBasedConfig());

  EncodedImage image;
  CodecSpecificInfo info;
  info.codecType = kVideoCodecVP9;
  info.codecSpecific.VP9.num_spatial_layers = 1;
  info.codecSpecific.VP9.first_frame_in_picture = true;
  info.end_of_picture = true;

  RTPVideoHeader headers[6];
  // Key frame.
  image._frameType = VideoFrameType::kVideoFrameKey;
  info.codecSpecific.VP9.inter_pic_predicted = false;
  info.codecSpecific.VP9.num_ref_pics = 0;
  info.codecSpecific.VP9.temporal_up_switch = true;
  info.codecSpecific.VP9.temporal_idx = 0;
  headers[0] = params.GetRtpVideoHeader(image, &info, /*shared_frame_id=*/1);

  // Delta frames.
  info.codecSpecific.VP9.inter_pic_predicted = true;
  image._frameType = VideoFrameType::kVideoFrameDelta;

  info.codecSpecific.VP9.temporal_up_switch = true;
  info.codecSpecific.VP9.temporal_idx = 1;
  info.codecSpecific.VP9.num_ref_pics = 1;
  info.codecSpecific.VP9.p_diff[0] = 1;
  headers[1] = params.GetRtpVideoHeader(image, &info, /*shared_frame_id=*/3);

  info.codecSpecific.VP9.temporal_up_switch = false;
  info.codecSpecific.VP9.temporal_idx = 0;
  info.codecSpecific.VP9.num_ref_pics = 1;
  info.codecSpecific.VP9.p_diff[0] = 2;
  headers[2] = params.GetRtpVideoHeader(image, &info, /*shared_frame_id=*/5);

  info.codecSpecific.VP9.temporal_up_switch = false;
  info.codecSpecific.VP9.temporal_idx = 1;
  info.codecSpecific.VP9.num_ref_pics = 2;
  info.codecSpecific.VP9.p_diff[0] = 1;
  info.codecSpecific.VP9.p_diff[1] = 2;
  headers[3] = params.GetRtpVideoHeader(image, &info, /*shared_frame_id=*/7);

  info.codecSpecific.VP9.temporal_up_switch = true;
  info.codecSpecific.VP9.temporal_idx = 0;
  info.codecSpecific.VP9.num_ref_pics = 1;
  info.codecSpecific.VP9.p_diff[0] = 2;
  headers[4] = params.GetRtpVideoHeader(image, &info, /*shared_frame_id=*/9);

  info.codecSpecific.VP9.temporal_up_switch = true;
  info.codecSpecific.VP9.temporal_idx = 1;
  info.codecSpecific.VP9.num_ref_pics = 1;
  info.codecSpecific.VP9.p_diff[0] = 1;
  headers[5] = params.GetRtpVideoHeader(image, &info, /*shared_frame_id=*/11);

  ASSERT_TRUE(headers[0].generic);
  int num_decode_targets = headers[0].generic->decode_target_indications.size();
  ASSERT_GE(num_decode_targets, 2);

  for (int frame_idx = 0; frame_idx < 6; ++frame_idx) {
    const RTPVideoHeader& header = headers[frame_idx];
    ASSERT_TRUE(header.generic);
    EXPECT_EQ(header.generic->spatial_index, 0);
    EXPECT_EQ(header.generic->temporal_index, frame_idx % 2);
    EXPECT_EQ(header.generic->frame_id, 1 + 2 * frame_idx);
    ASSERT_THAT(header.generic->decode_target_indications,
                SizeIs(num_decode_targets));
    // Expect only T0 frames are needed for the 1st decode target.
    if (header.generic->temporal_index == 0) {
      EXPECT_NE(header.generic->decode_target_indications[0],
                DecodeTargetIndication::kNotPresent);
    } else {
      EXPECT_EQ(header.generic->decode_target_indications[0],
                DecodeTargetIndication::kNotPresent);
    }
    // Expect all frames are needed for the 2nd decode target.
    EXPECT_NE(header.generic->decode_target_indications[1],
              DecodeTargetIndication::kNotPresent);
  }

  // Expect switch at every beginning of the pattern.
  EXPECT_THAT(headers[0].generic->decode_target_indications,
              Each(DecodeTargetIndication::kSwitch));
  EXPECT_THAT(headers[4].generic->decode_target_indications,
              Each(DecodeTargetIndication::kSwitch));

  EXPECT_THAT(headers[0].generic->dependencies, IsEmpty());          // T0, 1
  EXPECT_THAT(headers[1].generic->dependencies, ElementsAre(1));     // T1, 3
  EXPECT_THAT(headers[2].generic->dependencies, ElementsAre(1));     // T0, 5
  EXPECT_THAT(headers[3].generic->dependencies, ElementsAre(5, 3));  // T1, 7
  EXPECT_THAT(headers[4].generic->dependencies, ElementsAre(5));     // T0, 9
  EXPECT_THAT(headers[5].generic->dependencies, ElementsAre(9));     // T1, 11

  EXPECT_THAT(headers[0].generic->chain_diffs, ElementsAre(0));
  EXPECT_THAT(headers[1].generic->chain_diffs, ElementsAre(2));
  EXPECT_THAT(headers[2].generic->chain_diffs, ElementsAre(4));
  EXPECT_THAT(headers[3].generic->chain_diffs, ElementsAre(2));
  EXPECT_THAT(headers[4].generic->chain_diffs, ElementsAre(4));
  EXPECT_THAT(headers[5].generic->chain_diffs, ElementsAre(2));
}

TEST(RtpPayloadParamsVp9ToGenericTest, TemporalScalabilityWith3Layers) {
  // Test with 3 temporal layers structure that is not used by webrtc, but used
  // by chromium: https://imgur.com/pURAGvp
  RtpPayloadState state;
  RtpPayloadParams params(/*ssrc=*/123, &state, FieldTrialBasedConfig());

  EncodedImage image;
  CodecSpecificInfo info;
  info.codecType = kVideoCodecVP9;
  info.codecSpecific.VP9.num_spatial_layers = 1;
  info.codecSpecific.VP9.first_frame_in_picture = true;
  info.end_of_picture = true;

  RTPVideoHeader headers[9];
  // Key frame.
  image._frameType = VideoFrameType::kVideoFrameKey;
  info.codecSpecific.VP9.inter_pic_predicted = false;
  info.codecSpecific.VP9.num_ref_pics = 0;
  info.codecSpecific.VP9.temporal_up_switch = true;
  info.codecSpecific.VP9.temporal_idx = 0;
  headers[0] = params.GetRtpVideoHeader(image, &info, /*shared_frame_id=*/1);

  // Delta frames.
  info.codecSpecific.VP9.inter_pic_predicted = true;
  image._frameType = VideoFrameType::kVideoFrameDelta;

  info.codecSpecific.VP9.temporal_up_switch = true;
  info.codecSpecific.VP9.temporal_idx = 2;
  info.codecSpecific.VP9.num_ref_pics = 1;
  info.codecSpecific.VP9.p_diff[0] = 1;
  headers[1] = params.GetRtpVideoHeader(image, &info, /*shared_frame_id=*/3);

  info.codecSpecific.VP9.temporal_up_switch = true;
  info.codecSpecific.VP9.temporal_idx = 1;
  info.codecSpecific.VP9.num_ref_pics = 1;
  info.codecSpecific.VP9.p_diff[0] = 2;
  headers[2] = params.GetRtpVideoHeader(image, &info, /*shared_frame_id=*/5);

  info.codecSpecific.VP9.temporal_up_switch = true;
  info.codecSpecific.VP9.temporal_idx = 2;
  info.codecSpecific.VP9.num_ref_pics = 1;
  info.codecSpecific.VP9.p_diff[0] = 1;
  headers[3] = params.GetRtpVideoHeader(image, &info, /*shared_frame_id=*/7);

  info.codecSpecific.VP9.temporal_up_switch = false;
  info.codecSpecific.VP9.temporal_idx = 0;
  info.codecSpecific.VP9.num_ref_pics = 1;
  info.codecSpecific.VP9.p_diff[0] = 4;
  headers[4] = params.GetRtpVideoHeader(image, &info, /*shared_frame_id=*/9);

  info.codecSpecific.VP9.temporal_up_switch = true;
  info.codecSpecific.VP9.temporal_idx = 2;
  info.codecSpecific.VP9.num_ref_pics = 2;
  info.codecSpecific.VP9.p_diff[0] = 1;
  info.codecSpecific.VP9.p_diff[1] = 3;
  headers[5] = params.GetRtpVideoHeader(image, &info, /*shared_frame_id=*/11);

  info.codecSpecific.VP9.temporal_up_switch = false;
  info.codecSpecific.VP9.temporal_idx = 1;
  info.codecSpecific.VP9.num_ref_pics = 2;
  info.codecSpecific.VP9.p_diff[0] = 2;
  info.codecSpecific.VP9.p_diff[1] = 4;
  headers[6] = params.GetRtpVideoHeader(image, &info, /*shared_frame_id=*/13);

  info.codecSpecific.VP9.temporal_up_switch = true;
  info.codecSpecific.VP9.temporal_idx = 2;
  info.codecSpecific.VP9.num_ref_pics = 1;
  info.codecSpecific.VP9.p_diff[0] = 1;
  headers[7] = params.GetRtpVideoHeader(image, &info, /*shared_frame_id=*/15);

  info.codecSpecific.VP9.temporal_up_switch = true;
  info.codecSpecific.VP9.temporal_idx = 0;
  info.codecSpecific.VP9.num_ref_pics = 1;
  info.codecSpecific.VP9.p_diff[0] = 4;
  headers[8] = params.GetRtpVideoHeader(image, &info, /*shared_frame_id=*/17);

  ASSERT_TRUE(headers[0].generic);
  int num_decode_targets = headers[0].generic->decode_target_indications.size();
  ASSERT_GE(num_decode_targets, 3);

  for (int frame_idx = 0; frame_idx < 9; ++frame_idx) {
    const RTPVideoHeader& header = headers[frame_idx];
    ASSERT_TRUE(header.generic);
    EXPECT_EQ(header.generic->spatial_index, 0);
    EXPECT_EQ(header.generic->frame_id, 1 + 2 * frame_idx);
    ASSERT_THAT(header.generic->decode_target_indications,
                SizeIs(num_decode_targets));
    // Expect only T0 frames are needed for the 1st decode target.
    if (header.generic->temporal_index == 0) {
      EXPECT_NE(header.generic->decode_target_indications[0],
                DecodeTargetIndication::kNotPresent);
    } else {
      EXPECT_EQ(header.generic->decode_target_indications[0],
                DecodeTargetIndication::kNotPresent);
    }
    // Expect only T0 and T1 frames are needed for the 2nd decode target.
    if (header.generic->temporal_index <= 1) {
      EXPECT_NE(header.generic->decode_target_indications[1],
                DecodeTargetIndication::kNotPresent);
    } else {
      EXPECT_EQ(header.generic->decode_target_indications[1],
                DecodeTargetIndication::kNotPresent);
    }
    // Expect all frames are needed for the 3rd decode target.
    EXPECT_NE(header.generic->decode_target_indications[2],
              DecodeTargetIndication::kNotPresent);
  }

  EXPECT_EQ(headers[0].generic->temporal_index, 0);
  EXPECT_EQ(headers[1].generic->temporal_index, 2);
  EXPECT_EQ(headers[2].generic->temporal_index, 1);
  EXPECT_EQ(headers[3].generic->temporal_index, 2);
  EXPECT_EQ(headers[4].generic->temporal_index, 0);
  EXPECT_EQ(headers[5].generic->temporal_index, 2);
  EXPECT_EQ(headers[6].generic->temporal_index, 1);
  EXPECT_EQ(headers[7].generic->temporal_index, 2);
  EXPECT_EQ(headers[8].generic->temporal_index, 0);

  // Expect switch at every beginning of the pattern.
  EXPECT_THAT(headers[0].generic->decode_target_indications,
              Each(DecodeTargetIndication::kSwitch));
  EXPECT_THAT(headers[8].generic->decode_target_indications,
              Each(DecodeTargetIndication::kSwitch));

  EXPECT_THAT(headers[0].generic->dependencies, IsEmpty());          // T0, 1
  EXPECT_THAT(headers[1].generic->dependencies, ElementsAre(1));     // T2, 3
  EXPECT_THAT(headers[2].generic->dependencies, ElementsAre(1));     // T1, 5
  EXPECT_THAT(headers[3].generic->dependencies, ElementsAre(5));     // T2, 7
  EXPECT_THAT(headers[4].generic->dependencies, ElementsAre(1));     // T0, 9
  EXPECT_THAT(headers[5].generic->dependencies, ElementsAre(9, 5));  // T2, 11
  EXPECT_THAT(headers[6].generic->dependencies, ElementsAre(9, 5));  // T1, 13
  EXPECT_THAT(headers[7].generic->dependencies, ElementsAre(13));    // T2, 15
  EXPECT_THAT(headers[8].generic->dependencies, ElementsAre(9));     // T0, 17

  EXPECT_THAT(headers[0].generic->chain_diffs, ElementsAre(0));
  EXPECT_THAT(headers[1].generic->chain_diffs, ElementsAre(2));
  EXPECT_THAT(headers[2].generic->chain_diffs, ElementsAre(4));
  EXPECT_THAT(headers[3].generic->chain_diffs, ElementsAre(6));
  EXPECT_THAT(headers[4].generic->chain_diffs, ElementsAre(8));
  EXPECT_THAT(headers[5].generic->chain_diffs, ElementsAre(2));
  EXPECT_THAT(headers[6].generic->chain_diffs, ElementsAre(4));
  EXPECT_THAT(headers[7].generic->chain_diffs, ElementsAre(6));
  EXPECT_THAT(headers[8].generic->chain_diffs, ElementsAre(8));
}

TEST(RtpPayloadParamsVp9ToGenericTest, SpatialScalabilityKSvc) {
  //  1---3--
  //  |     ...
  //  0---2--
  RtpPayloadState state;
  RtpPayloadParams params(/*ssrc=*/123, &state, FieldTrialBasedConfig());

  EncodedImage image;
  CodecSpecificInfo info;
  info.codecType = kVideoCodecVP9;
  info.codecSpecific.VP9.num_spatial_layers = 2;
  info.codecSpecific.VP9.first_frame_in_picture = true;

  RTPVideoHeader headers[4];
  // Key frame.
  image._frameType = VideoFrameType::kVideoFrameKey;
  image.SetSpatialIndex(0);
  info.codecSpecific.VP9.inter_pic_predicted = false;
  info.codecSpecific.VP9.inter_layer_predicted = false;
  info.codecSpecific.VP9.non_ref_for_inter_layer_pred = false;
  info.codecSpecific.VP9.num_ref_pics = 0;
  info.codecSpecific.VP9.first_frame_in_picture = true;
  info.end_of_picture = false;
  headers[0] = params.GetRtpVideoHeader(image, &info, /*shared_frame_id=*/1);

  image.SetSpatialIndex(1);
  info.codecSpecific.VP9.inter_layer_predicted = true;
  info.codecSpecific.VP9.non_ref_for_inter_layer_pred = true;
  info.codecSpecific.VP9.first_frame_in_picture = false;
  info.end_of_picture = true;
  headers[1] = params.GetRtpVideoHeader(image, &info, /*shared_frame_id=*/3);

  // Delta frames.
  info.codecSpecific.VP9.inter_pic_predicted = true;
  image._frameType = VideoFrameType::kVideoFrameDelta;
  info.codecSpecific.VP9.num_ref_pics = 1;
  info.codecSpecific.VP9.p_diff[0] = 1;

  image.SetSpatialIndex(0);
  info.codecSpecific.VP9.inter_layer_predicted = false;
  info.codecSpecific.VP9.non_ref_for_inter_layer_pred = true;
  info.codecSpecific.VP9.first_frame_in_picture = true;
  info.end_of_picture = false;
  headers[2] = params.GetRtpVideoHeader(image, &info, /*shared_frame_id=*/5);

  image.SetSpatialIndex(1);
  info.codecSpecific.VP9.inter_layer_predicted = false;
  info.codecSpecific.VP9.non_ref_for_inter_layer_pred = true;
  info.codecSpecific.VP9.first_frame_in_picture = false;
  info.end_of_picture = true;
  headers[3] = params.GetRtpVideoHeader(image, &info, /*shared_frame_id=*/7);

  ASSERT_TRUE(headers[0].generic);
  int num_decode_targets = headers[0].generic->decode_target_indications.size();
  // Rely on implementation detail there are always kMaxTemporalStreams temporal
  // layers assumed, in particular assume Decode Target#0 matches layer S0T0,
  // and Decode Target#kMaxTemporalStreams matches layer S1T0.
  ASSERT_EQ(num_decode_targets, kMaxTemporalStreams * 2);

  for (int frame_idx = 0; frame_idx < 4; ++frame_idx) {
    const RTPVideoHeader& header = headers[frame_idx];
    ASSERT_TRUE(header.generic);
    EXPECT_EQ(header.generic->spatial_index, frame_idx % 2);
    EXPECT_EQ(header.generic->temporal_index, 0);
    EXPECT_EQ(header.generic->frame_id, 1 + 2 * frame_idx);
    ASSERT_THAT(header.generic->decode_target_indications,
                SizeIs(num_decode_targets));
  }

  // Expect S0 key frame is switch for both Decode Targets.
  EXPECT_EQ(headers[0].generic->decode_target_indications[0],
            DecodeTargetIndication::kSwitch);
  EXPECT_EQ(headers[0].generic->decode_target_indications[kMaxTemporalStreams],
            DecodeTargetIndication::kSwitch);
  // S1 key frame is only needed for the 2nd Decode Targets.
  EXPECT_EQ(headers[1].generic->decode_target_indications[0],
            DecodeTargetIndication::kNotPresent);
  EXPECT_NE(headers[1].generic->decode_target_indications[kMaxTemporalStreams],
            DecodeTargetIndication::kNotPresent);
  // Delta frames are only needed for their own Decode Targets.
  EXPECT_NE(headers[2].generic->decode_target_indications[0],
            DecodeTargetIndication::kNotPresent);
  EXPECT_EQ(headers[2].generic->decode_target_indications[kMaxTemporalStreams],
            DecodeTargetIndication::kNotPresent);
  EXPECT_EQ(headers[3].generic->decode_target_indications[0],
            DecodeTargetIndication::kNotPresent);
  EXPECT_NE(headers[3].generic->decode_target_indications[kMaxTemporalStreams],
            DecodeTargetIndication::kNotPresent);

  EXPECT_THAT(headers[0].generic->dependencies, IsEmpty());       // S0, 1
  EXPECT_THAT(headers[1].generic->dependencies, ElementsAre(1));  // S1, 3
  EXPECT_THAT(headers[2].generic->dependencies, ElementsAre(1));  // S0, 5
  EXPECT_THAT(headers[3].generic->dependencies, ElementsAre(3));  // S1, 7

  EXPECT_THAT(headers[0].generic->chain_diffs, ElementsAre(0, 0));
  EXPECT_THAT(headers[1].generic->chain_diffs, ElementsAre(2, 2));
  EXPECT_THAT(headers[2].generic->chain_diffs, ElementsAre(4, 2));
  EXPECT_THAT(headers[3].generic->chain_diffs, ElementsAre(2, 4));
}

class RtpPayloadParamsH264ToGenericTest : public ::testing::Test {
 public:
  enum LayerSync { kNoSync, kSync };

  RtpPayloadParamsH264ToGenericTest()
      : state_(), params_(123, &state_, trials_config_) {}

  void ConvertAndCheck(int temporal_index,
                       int64_t shared_frame_id,
                       VideoFrameType frame_type,
                       LayerSync layer_sync,
                       const std::set<int64_t>& expected_deps,
                       uint16_t width = 0,
                       uint16_t height = 0) {
    EncodedImage encoded_image;
    encoded_image._frameType = frame_type;
    encoded_image._encodedWidth = width;
    encoded_image._encodedHeight = height;

    CodecSpecificInfo codec_info;
    codec_info.codecType = kVideoCodecH264;
    codec_info.codecSpecific.H264.temporal_idx = temporal_index;
    codec_info.codecSpecific.H264.base_layer_sync = layer_sync == kSync;

    RTPVideoHeader header =
        params_.GetRtpVideoHeader(encoded_image, &codec_info, shared_frame_id);

    ASSERT_TRUE(header.generic);
    EXPECT_EQ(header.generic->spatial_index, 0);

    EXPECT_EQ(header.generic->frame_id, shared_frame_id);
    EXPECT_EQ(header.generic->temporal_index, temporal_index);
    std::set<int64_t> actual_deps(header.generic->dependencies.begin(),
                                  header.generic->dependencies.end());
    EXPECT_EQ(expected_deps, actual_deps);

    EXPECT_EQ(header.width, width);
    EXPECT_EQ(header.height, height);
  }

 protected:
  FieldTrialBasedConfig trials_config_;
  RtpPayloadState state_;
  RtpPayloadParams params_;
};

TEST_F(RtpPayloadParamsH264ToGenericTest, Keyframe) {
  ConvertAndCheck(0, 0, VideoFrameType::kVideoFrameKey, kNoSync, {}, 480, 360);
  ConvertAndCheck(0, 1, VideoFrameType::kVideoFrameDelta, kNoSync, {0});
  ConvertAndCheck(0, 2, VideoFrameType::kVideoFrameKey, kNoSync, {}, 480, 360);
}

TEST_F(RtpPayloadParamsH264ToGenericTest, TooHighTemporalIndex) {
  ConvertAndCheck(0, 0, VideoFrameType::kVideoFrameKey, kNoSync, {}, 480, 360);

  EncodedImage encoded_image;
  encoded_image._frameType = VideoFrameType::kVideoFrameDelta;
  CodecSpecificInfo codec_info;
  codec_info.codecType = kVideoCodecH264;
  codec_info.codecSpecific.H264.temporal_idx =
      RtpGenericFrameDescriptor::kMaxTemporalLayers;
  codec_info.codecSpecific.H264.base_layer_sync = false;

  RTPVideoHeader header =
      params_.GetRtpVideoHeader(encoded_image, &codec_info, 1);
  EXPECT_FALSE(header.generic);
}

TEST_F(RtpPayloadParamsH264ToGenericTest, LayerSync) {
  // 02120212 pattern
  ConvertAndCheck(0, 0, VideoFrameType::kVideoFrameKey, kNoSync, {}, 480, 360);
  ConvertAndCheck(2, 1, VideoFrameType::kVideoFrameDelta, kNoSync, {0});
  ConvertAndCheck(1, 2, VideoFrameType::kVideoFrameDelta, kNoSync, {0});
  ConvertAndCheck(2, 3, VideoFrameType::kVideoFrameDelta, kNoSync, {0, 1, 2});

  ConvertAndCheck(0, 4, VideoFrameType::kVideoFrameDelta, kNoSync, {0});
  ConvertAndCheck(2, 5, VideoFrameType::kVideoFrameDelta, kNoSync, {2, 3, 4});
  ConvertAndCheck(1, 6, VideoFrameType::kVideoFrameDelta, kSync,
                  {4});  // layer sync
  ConvertAndCheck(2, 7, VideoFrameType::kVideoFrameDelta, kNoSync, {4, 5, 6});
}

TEST_F(RtpPayloadParamsH264ToGenericTest, FrameIdGaps) {
  // 0101 pattern
  ConvertAndCheck(0, 0, VideoFrameType::kVideoFrameKey, kNoSync, {}, 480, 360);
  ConvertAndCheck(1, 1, VideoFrameType::kVideoFrameDelta, kNoSync, {0});

  ConvertAndCheck(0, 5, VideoFrameType::kVideoFrameDelta, kNoSync, {0});
  ConvertAndCheck(1, 10, VideoFrameType::kVideoFrameDelta, kNoSync, {1, 5});

  ConvertAndCheck(0, 15, VideoFrameType::kVideoFrameDelta, kNoSync, {5});
  ConvertAndCheck(1, 20, VideoFrameType::kVideoFrameDelta, kNoSync, {10, 15});
}

}  // namespace webrtc
