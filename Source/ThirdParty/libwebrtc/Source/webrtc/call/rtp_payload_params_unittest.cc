/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <set>

#include "call/rtp_payload_params.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "test/field_trial.h"
#include "test/gtest.h"

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

  RtpPayloadParams params(kSsrc2, &state2);
  EncodedImage encoded_image;
  encoded_image.rotation_ = kVideoRotation_90;
  encoded_image.content_type_ = VideoContentType::SCREENSHARE;
  encoded_image.SetSpatialIndex(1);

  CodecSpecificInfo codec_info;
  memset(&codec_info, 0, sizeof(CodecSpecificInfo));
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
  RtpPayloadParams params(kSsrc1, &state);

  EncodedImage encoded_image;
  encoded_image.rotation_ = kVideoRotation_90;
  encoded_image.content_type_ = VideoContentType::SCREENSHARE;
  encoded_image.SetSpatialIndex(0);
  CodecSpecificInfo codec_info;
  memset(&codec_info, 0, sizeof(CodecSpecificInfo));
  codec_info.codecType = kVideoCodecVP9;
  codec_info.codecSpecific.VP9.num_spatial_layers = 3;
  codec_info.codecSpecific.VP9.first_frame_in_picture = true;
  codec_info.codecSpecific.VP9.temporal_idx = 2;
  codec_info.codecSpecific.VP9.end_of_picture = false;

  RTPVideoHeader header =
      params.GetRtpVideoHeader(encoded_image, &codec_info, kDontCare);

  EXPECT_EQ(kVideoRotation_90, header.rotation);
  EXPECT_EQ(VideoContentType::SCREENSHARE, header.content_type);
  EXPECT_EQ(kVideoCodecVP9, header.codec);
  const auto& vp9_header =
      absl::get<RTPVideoHeaderVP9>(header.video_type_header);
  EXPECT_EQ(kPictureId + 1, vp9_header.picture_id);
  EXPECT_EQ(kTl0PicIdx, vp9_header.tl0_pic_idx);
  EXPECT_EQ(vp9_header.temporal_idx, codec_info.codecSpecific.VP9.temporal_idx);
  EXPECT_EQ(vp9_header.spatial_idx, encoded_image.SpatialIndex());
  EXPECT_EQ(vp9_header.num_spatial_layers,
            codec_info.codecSpecific.VP9.num_spatial_layers);
  EXPECT_EQ(vp9_header.end_of_picture,
            codec_info.codecSpecific.VP9.end_of_picture);

  // Next spatial layer.
  codec_info.codecSpecific.VP9.first_frame_in_picture = false;
  codec_info.codecSpecific.VP9.end_of_picture = true;

  encoded_image.SetSpatialIndex(1);
  header = params.GetRtpVideoHeader(encoded_image, &codec_info, kDontCare);

  EXPECT_EQ(kVideoRotation_90, header.rotation);
  EXPECT_EQ(VideoContentType::SCREENSHARE, header.content_type);
  EXPECT_EQ(kVideoCodecVP9, header.codec);
  EXPECT_EQ(kPictureId + 1, vp9_header.picture_id);
  EXPECT_EQ(kTl0PicIdx, vp9_header.tl0_pic_idx);
  EXPECT_EQ(vp9_header.temporal_idx, codec_info.codecSpecific.VP9.temporal_idx);
  EXPECT_EQ(vp9_header.spatial_idx, encoded_image.SpatialIndex());
  EXPECT_EQ(vp9_header.num_spatial_layers,
            codec_info.codecSpecific.VP9.num_spatial_layers);
  EXPECT_EQ(vp9_header.end_of_picture,
            codec_info.codecSpecific.VP9.end_of_picture);
}

TEST(RtpPayloadParamsTest, InfoMappedToRtpVideoHeader_H264) {
  RtpPayloadParams params(kSsrc1, {});

  EncodedImage encoded_image;
  CodecSpecificInfo codec_info;
  memset(&codec_info, 0, sizeof(CodecSpecificInfo));
  codec_info.codecType = kVideoCodecH264;
  codec_info.codecSpecific.H264.packetization_mode =
      H264PacketizationMode::SingleNalUnit;

  RTPVideoHeader header =
      params.GetRtpVideoHeader(encoded_image, &codec_info, kDontCare);

  EXPECT_EQ(0, header.simulcastIdx);
  EXPECT_EQ(kVideoCodecH264, header.codec);
  const auto& h264 = absl::get<RTPVideoHeaderH264>(header.video_type_header);
  EXPECT_EQ(H264PacketizationMode::SingleNalUnit, h264.packetization_mode);
}

TEST(RtpPayloadParamsTest, PictureIdIsSetForVp8) {
  RtpPayloadState state;
  state.picture_id = kInitialPictureId1;
  state.tl0_pic_idx = kInitialTl0PicIdx1;

  EncodedImage encoded_image;
  CodecSpecificInfo codec_info;
  memset(&codec_info, 0, sizeof(CodecSpecificInfo));
  codec_info.codecType = kVideoCodecVP8;

  RtpPayloadParams params(kSsrc1, &state);
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
  memset(&codec_info, 0, sizeof(CodecSpecificInfo));
  codec_info.codecType = kVideoCodecVP8;
  codec_info.codecSpecific.VP8.temporalIdx = kNoTemporalIdx;

  RtpPayloadParams params(kSsrc1, &state);
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
  memset(&codec_info, 0, sizeof(CodecSpecificInfo));
  codec_info.codecType = kVideoCodecVP8;
  codec_info.codecSpecific.VP8.temporalIdx = 1;

  RtpPayloadParams params(kSsrc1, &state);
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
  memset(&codec_info, 0, sizeof(CodecSpecificInfo));
  codec_info.codecType = kVideoCodecVP9;
  codec_info.codecSpecific.VP9.temporal_idx = 1;
  codec_info.codecSpecific.VP9.first_frame_in_picture = true;

  RtpPayloadParams params(kSsrc1, &state);
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
  CodecSpecificInfo codec_info{};
  codec_info.codecType = kVideoCodecGeneric;

  RtpPayloadParams params(kSsrc1, &state);
  RTPVideoHeader header =
      params.GetRtpVideoHeader(encoded_image, &codec_info, kDontCare);

  EXPECT_EQ(kVideoCodecGeneric, header.codec);
  ASSERT_TRUE(header.generic);
  EXPECT_EQ(0, header.generic->frame_id);

  header = params.GetRtpVideoHeader(encoded_image, &codec_info, kDontCare);
  ASSERT_TRUE(header.generic);
  EXPECT_EQ(1, header.generic->frame_id);
}

class RtpPayloadParamsVp8ToGenericTest : public ::testing::Test {
 public:
  enum LayerSync { kNoSync, kSync };

  RtpPayloadParamsVp8ToGenericTest()
      : generic_descriptor_field_trial_("WebRTC-GenericDescriptor/Enabled/"),
        state_(),
        params_(123, &state_) {}

  void ConvertAndCheck(int temporal_index,
                       int64_t shared_frame_id,
                       FrameType frame_type,
                       LayerSync layer_sync,
                       const std::set<int64_t>& expected_deps) {
    EncodedImage encoded_image;
    encoded_image._frameType = frame_type;

    CodecSpecificInfo codec_info{};
    codec_info.codecType = kVideoCodecVP8;
    codec_info.codecSpecific.VP8.temporalIdx = temporal_index;
    codec_info.codecSpecific.VP8.layerSync = layer_sync == kSync;

    RTPVideoHeader header =
        params_.GetRtpVideoHeader(encoded_image, &codec_info, shared_frame_id);

    ASSERT_TRUE(header.generic);
    EXPECT_TRUE(header.generic->higher_spatial_layers.empty());
    EXPECT_EQ(header.generic->spatial_index, 0);

    EXPECT_EQ(header.generic->frame_id, shared_frame_id);
    EXPECT_EQ(header.generic->temporal_index, temporal_index);
    std::set<int64_t> actual_deps(header.generic->dependencies.begin(),
                                  header.generic->dependencies.end());
    EXPECT_EQ(expected_deps, actual_deps);
  }

 protected:
  test::ScopedFieldTrials generic_descriptor_field_trial_;
  RtpPayloadState state_;
  RtpPayloadParams params_;
};

TEST_F(RtpPayloadParamsVp8ToGenericTest, Keyframe) {
  ConvertAndCheck(0, 0, kVideoFrameKey, kNoSync, {});
  ConvertAndCheck(0, 1, kVideoFrameDelta, kNoSync, {0});
  ConvertAndCheck(0, 2, kVideoFrameKey, kNoSync, {});
}

TEST_F(RtpPayloadParamsVp8ToGenericTest, TooHighTemporalIndex) {
  ConvertAndCheck(0, 0, kVideoFrameKey, kNoSync, {});

  EncodedImage encoded_image;
  encoded_image._frameType = kVideoFrameDelta;
  CodecSpecificInfo codec_info{};
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
  ConvertAndCheck(0, 0, kVideoFrameKey, kNoSync, {});
  ConvertAndCheck(2, 1, kVideoFrameDelta, kNoSync, {0});
  ConvertAndCheck(1, 2, kVideoFrameDelta, kNoSync, {0});
  ConvertAndCheck(2, 3, kVideoFrameDelta, kNoSync, {0, 1, 2});

  ConvertAndCheck(0, 4, kVideoFrameDelta, kNoSync, {0});
  ConvertAndCheck(2, 5, kVideoFrameDelta, kNoSync, {2, 3, 4});
  ConvertAndCheck(1, 6, kVideoFrameDelta, kSync, {4});  // layer sync
  ConvertAndCheck(2, 7, kVideoFrameDelta, kNoSync, {4, 5, 6});
}

TEST_F(RtpPayloadParamsVp8ToGenericTest, FrameIdGaps) {
  // 0101 pattern
  ConvertAndCheck(0, 0, kVideoFrameKey, kNoSync, {});
  ConvertAndCheck(1, 1, kVideoFrameDelta, kNoSync, {0});

  ConvertAndCheck(0, 5, kVideoFrameDelta, kNoSync, {0});
  ConvertAndCheck(1, 10, kVideoFrameDelta, kNoSync, {1, 5});

  ConvertAndCheck(0, 15, kVideoFrameDelta, kNoSync, {5});
  ConvertAndCheck(1, 20, kVideoFrameDelta, kNoSync, {10, 15});
}

}  // namespace webrtc
