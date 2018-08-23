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

#include "call/rtp_payload_params.h"
#include "modules/video_coding/include/video_codec_interface.h"
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

  CodecSpecificInfo codec_info;
  memset(&codec_info, 0, sizeof(CodecSpecificInfo));
  codec_info.codecType = kVideoCodecVP8;
  codec_info.codecSpecific.VP8.simulcastIdx = 1;
  codec_info.codecSpecific.VP8.temporalIdx = kTemporalIdx;
  codec_info.codecSpecific.VP8.keyIdx = kNoKeyIdx;
  codec_info.codecSpecific.VP8.layerSync = true;
  codec_info.codecSpecific.VP8.nonReference = true;

  RTPVideoHeader header = params.GetRtpVideoHeader(encoded_image, &codec_info);

  EXPECT_EQ(kVideoRotation_90, header.rotation);
  EXPECT_EQ(VideoContentType::SCREENSHARE, header.content_type);
  EXPECT_EQ(1, header.simulcastIdx);
  EXPECT_EQ(kVideoCodecVP8, header.codec);
  EXPECT_EQ(kPictureId + 1, header.vp8().pictureId);
  EXPECT_EQ(kTemporalIdx, header.vp8().temporalIdx);
  EXPECT_EQ(kTl0PicIdx, header.vp8().tl0PicIdx);
  EXPECT_EQ(kNoKeyIdx, header.vp8().keyIdx);
  EXPECT_TRUE(header.vp8().layerSync);
  EXPECT_TRUE(header.vp8().nonReference);
}

TEST(RtpPayloadParamsTest, InfoMappedToRtpVideoHeader_Vp9) {
  RtpPayloadState state;
  state.picture_id = kPictureId;
  state.tl0_pic_idx = kTl0PicIdx;
  RtpPayloadParams params(kSsrc1, &state);

  EncodedImage encoded_image;
  encoded_image.rotation_ = kVideoRotation_90;
  encoded_image.content_type_ = VideoContentType::SCREENSHARE;

  CodecSpecificInfo codec_info;
  memset(&codec_info, 0, sizeof(CodecSpecificInfo));
  codec_info.codecType = kVideoCodecVP9;
  codec_info.codecSpecific.VP9.num_spatial_layers = 3;
  codec_info.codecSpecific.VP9.first_frame_in_picture = true;
  codec_info.codecSpecific.VP9.spatial_idx = 0;
  codec_info.codecSpecific.VP9.temporal_idx = 2;
  codec_info.codecSpecific.VP9.end_of_picture = false;

  RTPVideoHeader header = params.GetRtpVideoHeader(encoded_image, &codec_info);

  EXPECT_EQ(kVideoRotation_90, header.rotation);
  EXPECT_EQ(VideoContentType::SCREENSHARE, header.content_type);
  EXPECT_EQ(kVideoCodecVP9, header.codec);
  const auto& vp9_header =
      absl::get<RTPVideoHeaderVP9>(header.video_type_header);
  EXPECT_EQ(kPictureId + 1, vp9_header.picture_id);
  EXPECT_EQ(kTl0PicIdx, vp9_header.tl0_pic_idx);
  EXPECT_EQ(vp9_header.temporal_idx, codec_info.codecSpecific.VP9.temporal_idx);
  EXPECT_EQ(vp9_header.spatial_idx, codec_info.codecSpecific.VP9.spatial_idx);
  EXPECT_EQ(vp9_header.num_spatial_layers,
            codec_info.codecSpecific.VP9.num_spatial_layers);
  EXPECT_EQ(vp9_header.end_of_picture,
            codec_info.codecSpecific.VP9.end_of_picture);

  // Next spatial layer.
  codec_info.codecSpecific.VP9.first_frame_in_picture = false;
  codec_info.codecSpecific.VP9.spatial_idx += 1;
  codec_info.codecSpecific.VP9.end_of_picture = true;

  header = params.GetRtpVideoHeader(encoded_image, &codec_info);

  EXPECT_EQ(kVideoRotation_90, header.rotation);
  EXPECT_EQ(VideoContentType::SCREENSHARE, header.content_type);
  EXPECT_EQ(kVideoCodecVP9, header.codec);
  EXPECT_EQ(kPictureId + 1, vp9_header.picture_id);
  EXPECT_EQ(kTl0PicIdx, vp9_header.tl0_pic_idx);
  EXPECT_EQ(vp9_header.temporal_idx, codec_info.codecSpecific.VP9.temporal_idx);
  EXPECT_EQ(vp9_header.spatial_idx, codec_info.codecSpecific.VP9.spatial_idx);
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

  RTPVideoHeader header = params.GetRtpVideoHeader(encoded_image, &codec_info);

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
  codec_info.codecSpecific.VP8.simulcastIdx = 0;

  RtpPayloadParams params(kSsrc1, &state);
  RTPVideoHeader header = params.GetRtpVideoHeader(encoded_image, &codec_info);
  EXPECT_EQ(kVideoCodecVP8, header.codec);
  EXPECT_EQ(kInitialPictureId1 + 1, header.vp8().pictureId);

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
  RTPVideoHeader header = params.GetRtpVideoHeader(encoded_image, &codec_info);
  EXPECT_EQ(kVideoCodecVP8, header.codec);
  EXPECT_EQ(0, header.vp8().pictureId);

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
  RTPVideoHeader header = params.GetRtpVideoHeader(encoded_image, &codec_info);

  EXPECT_EQ(kVideoCodecVP8, header.codec);
  EXPECT_EQ(kInitialPictureId1 + 1, header.vp8().pictureId);
  EXPECT_EQ(kInitialTl0PicIdx1, header.vp8().tl0PicIdx);

  // OnEncodedImage, temporalIdx: 0.
  codec_info.codecSpecific.VP8.temporalIdx = 0;

  header = params.GetRtpVideoHeader(encoded_image, &codec_info);
  EXPECT_EQ(kVideoCodecVP8, header.codec);
  EXPECT_EQ(kInitialPictureId1 + 2, header.vp8().pictureId);
  EXPECT_EQ(kInitialTl0PicIdx1 + 1, header.vp8().tl0PicIdx);

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
  RTPVideoHeader header = params.GetRtpVideoHeader(encoded_image, &codec_info);

  EXPECT_EQ(kVideoCodecVP9, header.codec);
  const auto& vp9_header =
      absl::get<RTPVideoHeaderVP9>(header.video_type_header);
  EXPECT_EQ(kInitialPictureId1 + 1, vp9_header.picture_id);
  EXPECT_EQ(kInitialTl0PicIdx1, vp9_header.tl0_pic_idx);

  // OnEncodedImage, temporalIdx: 0.
  codec_info.codecSpecific.VP9.temporal_idx = 0;

  header = params.GetRtpVideoHeader(encoded_image, &codec_info);

  EXPECT_EQ(kVideoCodecVP9, header.codec);
  EXPECT_EQ(kInitialPictureId1 + 2, vp9_header.picture_id);
  EXPECT_EQ(kInitialTl0PicIdx1 + 1, vp9_header.tl0_pic_idx);

  // OnEncodedImage, first_frame_in_picture = false
  codec_info.codecSpecific.VP9.first_frame_in_picture = false;

  header = params.GetRtpVideoHeader(encoded_image, &codec_info);

  EXPECT_EQ(kVideoCodecVP9, header.codec);
  EXPECT_EQ(kInitialPictureId1 + 2, vp9_header.picture_id);
  EXPECT_EQ(kInitialTl0PicIdx1 + 1, vp9_header.tl0_pic_idx);

  // State should hold latest used picture id and tl0_pic_idx.
  EXPECT_EQ(kInitialPictureId1 + 2, params.state().picture_id);
  EXPECT_EQ(kInitialTl0PicIdx1 + 1, params.state().tl0_pic_idx);
}
}  // namespace webrtc
