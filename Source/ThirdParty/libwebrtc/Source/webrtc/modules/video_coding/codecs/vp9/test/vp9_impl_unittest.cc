/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/color_space.h"
#include "api/video/i420_buffer.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "media/base/vp9_profile.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/video_coding/codecs/test/video_codec_unittest.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "modules/video_coding/codecs/vp9/svc_config.h"
#include "test/field_trial.h"
#include "test/video_codec_settings.h"

namespace webrtc {

namespace {
const size_t kWidth = 1280;
const size_t kHeight = 720;
}  // namespace

class TestVp9Impl : public VideoCodecUnitTest {
 protected:
  std::unique_ptr<VideoEncoder> CreateEncoder() override {
    return VP9Encoder::Create();
  }

  std::unique_ptr<VideoDecoder> CreateDecoder() override {
    return VP9Decoder::Create();
  }

  void ModifyCodecSettings(VideoCodec* codec_settings) override {
    webrtc::test::CodecSettings(kVideoCodecVP9, codec_settings);
    codec_settings->width = kWidth;
    codec_settings->height = kHeight;
    codec_settings->VP9()->numberOfTemporalLayers = 1;
    codec_settings->VP9()->numberOfSpatialLayers = 1;
  }

  void ExpectFrameWith(uint8_t temporal_idx) {
    EncodedImage encoded_frame;
    CodecSpecificInfo codec_specific_info;
    ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));
    EXPECT_EQ(temporal_idx, codec_specific_info.codecSpecific.VP9.temporal_idx);
  }

  void ExpectFrameWith(size_t num_spatial_layers,
                       uint8_t temporal_idx,
                       bool temporal_up_switch,
                       uint8_t num_ref_pics,
                       const std::vector<uint8_t>& p_diff) {
    std::vector<EncodedImage> encoded_frame;
    std::vector<CodecSpecificInfo> codec_specific;
    ASSERT_TRUE(WaitForEncodedFrames(&encoded_frame, &codec_specific));
    for (size_t spatial_idx = 0; spatial_idx < num_spatial_layers;
         ++spatial_idx) {
      const CodecSpecificInfoVP9& vp9 =
          codec_specific[spatial_idx].codecSpecific.VP9;
      if (vp9.temporal_idx == kNoTemporalIdx) {
        EXPECT_EQ(temporal_idx, 0);
      } else {
        EXPECT_EQ(vp9.temporal_idx, temporal_idx);
      }
      if (num_spatial_layers == 1) {
        EXPECT_FALSE(encoded_frame[spatial_idx].SpatialIndex());
      } else {
        EXPECT_EQ(encoded_frame[spatial_idx].SpatialIndex(),
                  static_cast<int>(spatial_idx));
      }
      EXPECT_EQ(vp9.temporal_up_switch, temporal_up_switch);

      // Ensure there are no duplicates in reference list.
      std::vector<uint8_t> vp9_p_diff(vp9.p_diff,
                                      vp9.p_diff + vp9.num_ref_pics);
      std::sort(vp9_p_diff.begin(), vp9_p_diff.end());
      EXPECT_EQ(std::unique(vp9_p_diff.begin(), vp9_p_diff.end()),
                vp9_p_diff.end());

      for (size_t ref_pic_num = 0; ref_pic_num < num_ref_pics; ++ref_pic_num) {
        EXPECT_NE(
            std::find(p_diff.begin(), p_diff.end(), vp9.p_diff[ref_pic_num]),
            p_diff.end());
      }
    }
  }

  void ConfigureSvc(size_t num_spatial_layers) {
    codec_settings_.VP9()->numberOfSpatialLayers =
        static_cast<unsigned char>(num_spatial_layers);
    codec_settings_.VP9()->numberOfTemporalLayers = 1;
    codec_settings_.VP9()->frameDroppingOn = false;

    std::vector<SpatialLayer> layers = GetSvcConfig(
        codec_settings_.width, codec_settings_.height,
        codec_settings_.maxFramerate, num_spatial_layers, 1, false);
    for (size_t i = 0; i < layers.size(); ++i) {
      codec_settings_.spatialLayers[i] = layers[i];
    }
  }

  HdrMetadata CreateTestHdrMetadata() const {
    // Random but reasonable HDR metadata.
    HdrMetadata hdr_metadata;
    hdr_metadata.mastering_metadata.luminance_max = 2000.0;
    hdr_metadata.mastering_metadata.luminance_min = 2.0001;
    hdr_metadata.mastering_metadata.primary_r.x = 0.30;
    hdr_metadata.mastering_metadata.primary_r.y = 0.40;
    hdr_metadata.mastering_metadata.primary_g.x = 0.32;
    hdr_metadata.mastering_metadata.primary_g.y = 0.46;
    hdr_metadata.mastering_metadata.primary_b.x = 0.34;
    hdr_metadata.mastering_metadata.primary_b.y = 0.49;
    hdr_metadata.mastering_metadata.white_point.x = 0.41;
    hdr_metadata.mastering_metadata.white_point.y = 0.48;
    hdr_metadata.max_content_light_level = 2345;
    hdr_metadata.max_frame_average_light_level = 1789;
    return hdr_metadata;
  }

  ColorSpace CreateTestColorSpace() const {
    HdrMetadata hdr_metadata = CreateTestHdrMetadata();
    ColorSpace color_space(ColorSpace::PrimaryID::kBT709,
                           ColorSpace::TransferID::kGAMMA22,
                           ColorSpace::MatrixID::kSMPTE2085,
                           ColorSpace::RangeID::kFull, &hdr_metadata);
    return color_space;
  }
};

// Disabled on ios as flake, see https://crbug.com/webrtc/7057
#if defined(WEBRTC_IOS)
TEST_F(TestVp9Impl, DISABLED_EncodeDecode) {
#else
TEST_F(TestVp9Impl, EncodeDecode) {
#endif
  VideoFrame* input_frame = NextInputFrame();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*input_frame, nullptr, nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));
  // First frame should be a key frame.
  encoded_frame._frameType = kVideoFrameKey;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            decoder_->Decode(encoded_frame, false, nullptr, 0));
  std::unique_ptr<VideoFrame> decoded_frame;
  absl::optional<uint8_t> decoded_qp;
  ASSERT_TRUE(WaitForDecodedFrame(&decoded_frame, &decoded_qp));
  ASSERT_TRUE(decoded_frame);
  EXPECT_GT(I420PSNR(input_frame, decoded_frame.get()), 36);

  const ColorSpace color_space = *decoded_frame->color_space();
  EXPECT_EQ(ColorSpace::PrimaryID::kInvalid, color_space.primaries());
  EXPECT_EQ(ColorSpace::TransferID::kInvalid, color_space.transfer());
  EXPECT_EQ(ColorSpace::MatrixID::kInvalid, color_space.matrix());
  EXPECT_EQ(ColorSpace::RangeID::kLimited, color_space.range());
}

// We only test the encoder here, since the decoded frame rotation is set based
// on the CVO RTP header extension in VCMDecodedFrameCallback::Decoded.
// TODO(brandtr): Consider passing through the rotation flag through the decoder
// in the same way as done in the encoder.
TEST_F(TestVp9Impl, EncodedRotationEqualsInputRotation) {
  VideoFrame* input_frame = NextInputFrame();
  input_frame->set_rotation(kVideoRotation_0);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*input_frame, nullptr, nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));
  EXPECT_EQ(kVideoRotation_0, encoded_frame.rotation_);

  input_frame = NextInputFrame();
  input_frame->set_rotation(kVideoRotation_90);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*input_frame, nullptr, nullptr));
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));
  EXPECT_EQ(kVideoRotation_90, encoded_frame.rotation_);
}

TEST_F(TestVp9Impl, EncodedColorSpaceEqualsInputColorSpace) {
  // Video frame without explicit color space information.
  VideoFrame* input_frame = NextInputFrame();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*input_frame, nullptr, nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));
  EXPECT_FALSE(encoded_frame.ColorSpace());

  // Video frame with explicit color space information.
  ColorSpace color_space = CreateTestColorSpace();
  VideoFrame input_frame_w_hdr =
      VideoFrame::Builder()
          .set_video_frame_buffer(input_frame->video_frame_buffer())
          .set_color_space(&color_space)
          .build();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(input_frame_w_hdr, nullptr, nullptr));
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));
  ASSERT_TRUE(encoded_frame.ColorSpace());
  EXPECT_EQ(*encoded_frame.ColorSpace(), color_space);
}

TEST_F(TestVp9Impl, DecodedHdrMetadataEqualsEncodedHdrMetadata) {
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr, nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));

  // Encoded frame without explicit color space information.
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            decoder_->Decode(encoded_frame, false, nullptr, 0));
  std::unique_ptr<VideoFrame> decoded_frame;
  absl::optional<uint8_t> decoded_qp;
  ASSERT_TRUE(WaitForDecodedFrame(&decoded_frame, &decoded_qp));
  ASSERT_TRUE(decoded_frame);
  // Color space present from encoded bitstream.
  ASSERT_TRUE(decoded_frame->color_space());
  // No HDR metadata present.
  EXPECT_FALSE(decoded_frame->color_space()->hdr_metadata());

  // Encoded frame with explicit color space information.
  ColorSpace color_space = CreateTestColorSpace();
  encoded_frame.SetColorSpace(&color_space);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            decoder_->Decode(encoded_frame, false, nullptr, 0));
  ASSERT_TRUE(WaitForDecodedFrame(&decoded_frame, &decoded_qp));
  ASSERT_TRUE(decoded_frame);
  ASSERT_TRUE(decoded_frame->color_space());
  EXPECT_EQ(color_space, *decoded_frame->color_space());
}

TEST_F(TestVp9Impl, DecodedQpEqualsEncodedQp) {
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr, nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));
  // First frame should be a key frame.
  encoded_frame._frameType = kVideoFrameKey;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            decoder_->Decode(encoded_frame, false, nullptr, 0));
  std::unique_ptr<VideoFrame> decoded_frame;
  absl::optional<uint8_t> decoded_qp;
  ASSERT_TRUE(WaitForDecodedFrame(&decoded_frame, &decoded_qp));
  ASSERT_TRUE(decoded_frame);
  ASSERT_TRUE(decoded_qp);
  EXPECT_EQ(encoded_frame.qp_, *decoded_qp);
}

TEST_F(TestVp9Impl, ParserQpEqualsEncodedQp) {
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr, nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));

  int qp = 0;
  ASSERT_TRUE(vp9::GetQp(encoded_frame._buffer, encoded_frame._length, &qp));
  EXPECT_EQ(encoded_frame.qp_, qp);
}

TEST_F(TestVp9Impl, EncoderWith2TemporalLayers) {
  // Override default settings.
  codec_settings_.VP9()->numberOfTemporalLayers = 2;
  // Tl0PidIdx is only used in non-flexible mode.
  codec_settings_.VP9()->flexibleMode = false;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, 1 /* number of cores */,
                                 0 /* max payload size (unused) */));

  // Temporal layer 0.
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr, nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));
  EXPECT_EQ(0, codec_specific_info.codecSpecific.VP9.temporal_idx);

  // Temporal layer 1.
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr, nullptr));
  ExpectFrameWith(1);

  // Temporal layer 0.
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr, nullptr));
  ExpectFrameWith(0);

  // Temporal layer 1.
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr, nullptr));
  ExpectFrameWith(1);
}

TEST_F(TestVp9Impl, EncoderWith2SpatialLayers) {
  codec_settings_.VP9()->numberOfSpatialLayers = 2;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, 1 /* number of cores */,
                                 0 /* max payload size (unused) */));

  SetWaitForEncodedFramesThreshold(2);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr, nullptr));
  std::vector<EncodedImage> encoded_frame;
  std::vector<CodecSpecificInfo> codec_info;
  ASSERT_TRUE(WaitForEncodedFrames(&encoded_frame, &codec_info));
  EXPECT_EQ(encoded_frame[0].SpatialIndex(), 0);
  EXPECT_EQ(encoded_frame[1].SpatialIndex(), 1);
}

TEST_F(TestVp9Impl, EncoderExplicitLayering) {
  // Override default settings.
  codec_settings_.VP9()->numberOfTemporalLayers = 1;
  codec_settings_.VP9()->numberOfSpatialLayers = 2;

  codec_settings_.width = 960;
  codec_settings_.height = 540;
  codec_settings_.spatialLayers[0].minBitrate = 200;
  codec_settings_.spatialLayers[0].maxBitrate = 500;
  codec_settings_.spatialLayers[0].targetBitrate =
      (codec_settings_.spatialLayers[0].minBitrate +
       codec_settings_.spatialLayers[0].maxBitrate) /
      2;
  codec_settings_.spatialLayers[1].minBitrate = 400;
  codec_settings_.spatialLayers[1].maxBitrate = 1500;
  codec_settings_.spatialLayers[1].targetBitrate =
      (codec_settings_.spatialLayers[1].minBitrate +
       codec_settings_.spatialLayers[1].maxBitrate) /
      2;

  codec_settings_.spatialLayers[0].width = codec_settings_.width / 2;
  codec_settings_.spatialLayers[0].height = codec_settings_.height / 2;
  codec_settings_.spatialLayers[0].maxFramerate = codec_settings_.maxFramerate;
  codec_settings_.spatialLayers[1].width = codec_settings_.width;
  codec_settings_.spatialLayers[1].height = codec_settings_.height;
  codec_settings_.spatialLayers[1].maxFramerate = codec_settings_.maxFramerate;

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, 1 /* number of cores */,
                                 0 /* max payload size (unused) */));

  // Ensure it fails if scaling factors in horz/vert dimentions are different.
  codec_settings_.spatialLayers[0].width = codec_settings_.width;
  codec_settings_.spatialLayers[0].height = codec_settings_.height / 2;
  codec_settings_.spatialLayers[1].width = codec_settings_.width;
  codec_settings_.spatialLayers[1].height = codec_settings_.height;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_ERR_PARAMETER,
            encoder_->InitEncode(&codec_settings_, 1 /* number of cores */,
                                 0 /* max payload size (unused) */));

  // Ensure it fails if scaling factor is not power of two.
  codec_settings_.spatialLayers[0].width = codec_settings_.width / 3;
  codec_settings_.spatialLayers[0].height = codec_settings_.height / 3;
  codec_settings_.spatialLayers[1].width = codec_settings_.width;
  codec_settings_.spatialLayers[1].height = codec_settings_.height;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_ERR_PARAMETER,
            encoder_->InitEncode(&codec_settings_, 1 /* number of cores */,
                                 0 /* max payload size (unused) */));
}

TEST_F(TestVp9Impl, EnableDisableSpatialLayers) {
  // Configure encoder to produce N spatial layers. Encode frames of layer 0
  // then enable layer 1 and encode more frames and so on until layer N-1.
  // Then disable layers one by one in the same way.
  // Note: bit rate allocation is high to avoid frame dropping due to rate
  // control, the encoder should always produce a frame. A dropped
  // frame indicates a problem and the test will fail.
  const size_t num_spatial_layers = 3;
  const size_t num_frames_to_encode = 5;

  ConfigureSvc(num_spatial_layers);
  codec_settings_.VP9()->frameDroppingOn = true;

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, 1 /* number of cores */,
                                 0 /* max payload size (unused) */));

  VideoBitrateAllocation bitrate_allocation;
  for (size_t sl_idx = 0; sl_idx < num_spatial_layers; ++sl_idx) {
    // Allocate high bit rate to avoid frame dropping due to rate control.
    bitrate_allocation.SetBitrate(
        sl_idx, 0,
        codec_settings_.spatialLayers[sl_idx].targetBitrate * 1000 * 2);
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->SetRateAllocation(bitrate_allocation,
                                          codec_settings_.maxFramerate));

    for (size_t frame_num = 0; frame_num < num_frames_to_encode; ++frame_num) {
      SetWaitForEncodedFramesThreshold(sl_idx + 1);
      EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
                encoder_->Encode(*NextInputFrame(), nullptr, nullptr));
      std::vector<EncodedImage> encoded_frame;
      std::vector<CodecSpecificInfo> codec_specific_info;
      ASSERT_TRUE(WaitForEncodedFrames(&encoded_frame, &codec_specific_info));
    }
  }

  for (size_t i = 0; i < num_spatial_layers - 1; ++i) {
    const size_t sl_idx = num_spatial_layers - i - 1;
    bitrate_allocation.SetBitrate(sl_idx, 0, 0);
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->SetRateAllocation(bitrate_allocation,
                                          codec_settings_.maxFramerate));

    for (size_t frame_num = 0; frame_num < num_frames_to_encode; ++frame_num) {
      SetWaitForEncodedFramesThreshold(sl_idx);
      EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
                encoder_->Encode(*NextInputFrame(), nullptr, nullptr));
      std::vector<EncodedImage> encoded_frame;
      std::vector<CodecSpecificInfo> codec_specific_info;
      ASSERT_TRUE(WaitForEncodedFrames(&encoded_frame, &codec_specific_info));
    }
  }
}

TEST_F(TestVp9Impl, EndOfPicture) {
  const size_t num_spatial_layers = 2;
  ConfigureSvc(num_spatial_layers);

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, 1 /* number of cores */,
                                 0 /* max payload size (unused) */));

  // Encode both base and upper layers. Check that end-of-superframe flag is
  // set on upper layer frame but not on base layer frame.
  VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(
      0, 0, codec_settings_.spatialLayers[0].targetBitrate * 1000);
  bitrate_allocation.SetBitrate(
      1, 0, codec_settings_.spatialLayers[1].targetBitrate * 1000);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->SetRateAllocation(bitrate_allocation,
                                        codec_settings_.maxFramerate));
  SetWaitForEncodedFramesThreshold(2);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr, nullptr));

  std::vector<EncodedImage> frames;
  std::vector<CodecSpecificInfo> codec_specific;
  ASSERT_TRUE(WaitForEncodedFrames(&frames, &codec_specific));
  EXPECT_FALSE(codec_specific[0].codecSpecific.VP9.end_of_picture);
  EXPECT_TRUE(codec_specific[1].codecSpecific.VP9.end_of_picture);

  // Encode only base layer. Check that end-of-superframe flag is
  // set on base layer frame.
  bitrate_allocation.SetBitrate(1, 0, 0);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->SetRateAllocation(bitrate_allocation,
                                        codec_settings_.maxFramerate));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, 1 /* number of cores */,
                                 0 /* max payload size (unused) */));

  SetWaitForEncodedFramesThreshold(1);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr, nullptr));

  ASSERT_TRUE(WaitForEncodedFrames(&frames, &codec_specific));
  EXPECT_FALSE(frames[0].SpatialIndex());
  EXPECT_TRUE(codec_specific[0].codecSpecific.VP9.end_of_picture);
}

TEST_F(TestVp9Impl, InterLayerPred) {
  const size_t num_spatial_layers = 2;
  ConfigureSvc(num_spatial_layers);
  codec_settings_.VP9()->frameDroppingOn = false;

  VideoBitrateAllocation bitrate_allocation;
  for (size_t i = 0; i < num_spatial_layers; ++i) {
    bitrate_allocation.SetBitrate(
        i, 0, codec_settings_.spatialLayers[i].targetBitrate * 1000);
  }

  const std::vector<InterLayerPredMode> inter_layer_pred_modes = {
      InterLayerPredMode::kOff, InterLayerPredMode::kOn,
      InterLayerPredMode::kOnKeyPic};

  for (const InterLayerPredMode inter_layer_pred : inter_layer_pred_modes) {
    codec_settings_.VP9()->interLayerPred = inter_layer_pred;
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->InitEncode(&codec_settings_, 1 /* number of cores */,
                                   0 /* max payload size (unused) */));

    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->SetRateAllocation(bitrate_allocation,
                                          codec_settings_.maxFramerate));

    SetWaitForEncodedFramesThreshold(2);
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->Encode(*NextInputFrame(), nullptr, nullptr));

    std::vector<EncodedImage> frames;
    std::vector<CodecSpecificInfo> codec_specific;
    ASSERT_TRUE(WaitForEncodedFrames(&frames, &codec_specific));

    // Key frame.
    EXPECT_FALSE(codec_specific[0].codecSpecific.VP9.inter_pic_predicted);
    EXPECT_EQ(frames[0].SpatialIndex(), 0);
    EXPECT_EQ(codec_specific[0].codecSpecific.VP9.non_ref_for_inter_layer_pred,
              inter_layer_pred == InterLayerPredMode::kOff);
    EXPECT_TRUE(
        codec_specific[1].codecSpecific.VP9.non_ref_for_inter_layer_pred);

    SetWaitForEncodedFramesThreshold(2);
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->Encode(*NextInputFrame(), nullptr, nullptr));
    ASSERT_TRUE(WaitForEncodedFrames(&frames, &codec_specific));

    // Delta frame.
    EXPECT_TRUE(codec_specific[0].codecSpecific.VP9.inter_pic_predicted);
    EXPECT_EQ(frames[0].SpatialIndex(), 0);
    EXPECT_EQ(codec_specific[0].codecSpecific.VP9.non_ref_for_inter_layer_pred,
              inter_layer_pred == InterLayerPredMode::kOff ||
                  inter_layer_pred == InterLayerPredMode::kOnKeyPic);
    EXPECT_TRUE(
        codec_specific[1].codecSpecific.VP9.non_ref_for_inter_layer_pred);
  }
}

TEST_F(TestVp9Impl,
       EnablingUpperLayerTriggersKeyFrameIfInterLayerPredIsDisabled) {
  const size_t num_spatial_layers = 3;
  const size_t num_frames_to_encode = 2;

  ConfigureSvc(num_spatial_layers);
  codec_settings_.VP9()->frameDroppingOn = false;

  const std::vector<InterLayerPredMode> inter_layer_pred_modes = {
      InterLayerPredMode::kOff, InterLayerPredMode::kOn,
      InterLayerPredMode::kOnKeyPic};

  for (const InterLayerPredMode inter_layer_pred : inter_layer_pred_modes) {
    codec_settings_.VP9()->interLayerPred = inter_layer_pred;
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->InitEncode(&codec_settings_, 1 /* number of cores */,
                                   0 /* max payload size (unused) */));

    VideoBitrateAllocation bitrate_allocation;
    for (size_t sl_idx = 0; sl_idx < num_spatial_layers; ++sl_idx) {
      bitrate_allocation.SetBitrate(
          sl_idx, 0,
          codec_settings_.spatialLayers[sl_idx].targetBitrate * 1000);
      EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
                encoder_->SetRateAllocation(bitrate_allocation,
                                            codec_settings_.maxFramerate));

      for (size_t frame_num = 0; frame_num < num_frames_to_encode;
           ++frame_num) {
        SetWaitForEncodedFramesThreshold(sl_idx + 1);
        EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
                  encoder_->Encode(*NextInputFrame(), nullptr, nullptr));
        std::vector<EncodedImage> encoded_frame;
        std::vector<CodecSpecificInfo> codec_specific_info;
        ASSERT_TRUE(WaitForEncodedFrames(&encoded_frame, &codec_specific_info));

        const bool is_first_upper_layer_frame = (sl_idx > 0 && frame_num == 0);
        if (is_first_upper_layer_frame) {
          if (inter_layer_pred == InterLayerPredMode::kOn) {
            EXPECT_EQ(encoded_frame[0]._frameType, kVideoFrameDelta);
          } else {
            EXPECT_EQ(encoded_frame[0]._frameType, kVideoFrameKey);
          }
        } else if (sl_idx == 0 && frame_num == 0) {
          EXPECT_EQ(encoded_frame[0]._frameType, kVideoFrameKey);
        } else {
          for (size_t i = 0; i <= sl_idx; ++i) {
            EXPECT_EQ(encoded_frame[i]._frameType, kVideoFrameDelta);
          }
        }
      }
    }
  }
}

TEST_F(TestVp9Impl,
       LowLayerMarkedAsRefIfHighLayerNotEncodedAndInterLayerPredIsEnabled) {
  ConfigureSvc(3);
  codec_settings_.VP9()->frameDroppingOn = false;
  codec_settings_.VP9()->interLayerPred = InterLayerPredMode::kOn;

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, 1 /* number of cores */,
                                 0 /* max payload size (unused) */));

  VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(
      0, 0, codec_settings_.spatialLayers[0].targetBitrate * 1000);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->SetRateAllocation(bitrate_allocation,
                                        codec_settings_.maxFramerate));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr, nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_info));
  EXPECT_TRUE(codec_info.codecSpecific.VP9.ss_data_available);
  EXPECT_FALSE(codec_info.codecSpecific.VP9.non_ref_for_inter_layer_pred);
}

TEST_F(TestVp9Impl, ScalabilityStructureIsAvailableInFlexibleMode) {
  codec_settings_.VP9()->flexibleMode = true;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, 1 /* number of cores */,
                                 0 /* max payload size (unused) */));

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr, nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));
  EXPECT_TRUE(codec_specific_info.codecSpecific.VP9.ss_data_available);
}

class TestVp9ImplWithLayering
    : public TestVp9Impl,
      public ::testing::WithParamInterface<::testing::tuple<uint8_t, uint8_t>> {
 protected:
  TestVp9ImplWithLayering()
      : num_spatial_layers_(::testing::get<0>(GetParam())),
        num_temporal_layers_(::testing::get<1>(GetParam())) {}

  const uint8_t num_spatial_layers_;
  const uint8_t num_temporal_layers_;
};

TEST_P(TestVp9ImplWithLayering, FlexibleMode) {
  // In flexible mode encoder wrapper obtains actual list of references from
  // encoder and writes it into RTP payload descriptor. Check that reference
  // list in payload descriptor matches the predefined one, which is used
  // in non-flexible mode.
  codec_settings_.VP9()->flexibleMode = true;
  codec_settings_.VP9()->frameDroppingOn = false;
  codec_settings_.VP9()->numberOfSpatialLayers = num_spatial_layers_;
  codec_settings_.VP9()->numberOfTemporalLayers = num_temporal_layers_;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, 1 /* number of cores */,
                                 0 /* max payload size (unused) */));

  GofInfoVP9 gof;
  if (num_temporal_layers_ == 1) {
    gof.SetGofInfoVP9(kTemporalStructureMode1);
  } else if (num_temporal_layers_ == 2) {
    gof.SetGofInfoVP9(kTemporalStructureMode2);
  } else if (num_temporal_layers_ == 3) {
    gof.SetGofInfoVP9(kTemporalStructureMode3);
  }

  // Encode at least (num_frames_in_gof + 1) frames to verify references
  // of non-key frame with gof_idx = 0.
  for (size_t frame_num = 0; frame_num < gof.num_frames_in_gof + 1;
       ++frame_num) {
    SetWaitForEncodedFramesThreshold(num_spatial_layers_);
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->Encode(*NextInputFrame(), nullptr, nullptr));

    const bool is_key_frame = frame_num == 0;
    const size_t gof_idx = frame_num % gof.num_frames_in_gof;
    const std::vector<uint8_t> p_diff(std::begin(gof.pid_diff[gof_idx]),
                                      std::end(gof.pid_diff[gof_idx]));

    ExpectFrameWith(num_spatial_layers_, gof.temporal_idx[gof_idx],
                    gof.temporal_up_switch[gof_idx],
                    is_key_frame ? 0 : gof.num_ref_pics[gof_idx], p_diff);
  }
}

TEST_P(TestVp9ImplWithLayering, ExternalRefControl) {
  test::ScopedFieldTrials override_field_trials(
      "WebRTC-Vp9ExternalRefCtrl/Enabled/");
  codec_settings_.VP9()->flexibleMode = true;
  codec_settings_.VP9()->frameDroppingOn = false;
  codec_settings_.VP9()->numberOfSpatialLayers = num_spatial_layers_;
  codec_settings_.VP9()->numberOfTemporalLayers = num_temporal_layers_;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, 1 /* number of cores */,
                                 0 /* max payload size (unused) */));

  GofInfoVP9 gof;
  if (num_temporal_layers_ == 1) {
    gof.SetGofInfoVP9(kTemporalStructureMode1);
  } else if (num_temporal_layers_ == 2) {
    gof.SetGofInfoVP9(kTemporalStructureMode2);
  } else if (num_temporal_layers_ == 3) {
    gof.SetGofInfoVP9(kTemporalStructureMode3);
  }

  // Encode at least (num_frames_in_gof + 1) frames to verify references
  // of non-key frame with gof_idx = 0.
  for (size_t frame_num = 0; frame_num < gof.num_frames_in_gof + 1;
       ++frame_num) {
    SetWaitForEncodedFramesThreshold(num_spatial_layers_);
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->Encode(*NextInputFrame(), nullptr, nullptr));

    const bool is_key_frame = frame_num == 0;
    const size_t gof_idx = frame_num % gof.num_frames_in_gof;
    const std::vector<uint8_t> p_diff(std::begin(gof.pid_diff[gof_idx]),
                                      std::end(gof.pid_diff[gof_idx]));

    ExpectFrameWith(num_spatial_layers_, gof.temporal_idx[gof_idx],
                    gof.temporal_up_switch[gof_idx],
                    is_key_frame ? 0 : gof.num_ref_pics[gof_idx], p_diff);
  }
}

INSTANTIATE_TEST_CASE_P(,
                        TestVp9ImplWithLayering,
                        ::testing::Combine(::testing::Values(1, 2, 3),
                                           ::testing::Values(1, 2, 3)));

class TestVp9ImplFrameDropping : public TestVp9Impl {
 protected:
  void ModifyCodecSettings(VideoCodec* codec_settings) override {
    webrtc::test::CodecSettings(kVideoCodecVP9, codec_settings);
    // We need to encode quite a lot of frames in this test. Use low resolution
    // to reduce execution time.
    codec_settings->width = 64;
    codec_settings->height = 64;
    codec_settings->mode = VideoCodecMode::kScreensharing;
  }
};

TEST_F(TestVp9ImplFrameDropping, PreEncodeFrameDropping) {
  const size_t num_frames_to_encode = 100;
  const float input_framerate_fps = 30.0;
  const float video_duration_secs = num_frames_to_encode / input_framerate_fps;
  const float expected_framerate_fps = 5.0f;
  const float max_abs_framerate_error_fps = expected_framerate_fps * 0.1f;

  codec_settings_.maxFramerate = static_cast<uint32_t>(expected_framerate_fps);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, 1 /* number of cores */,
                                 0 /* max payload size (unused) */));

  VideoFrame* input_frame = NextInputFrame();
  for (size_t frame_num = 0; frame_num < num_frames_to_encode; ++frame_num) {
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->Encode(*input_frame, nullptr, nullptr));
    const size_t timestamp = input_frame->timestamp() +
                             kVideoPayloadTypeFrequency / input_framerate_fps;
    input_frame->set_timestamp(static_cast<uint32_t>(timestamp));
  }

  const size_t num_encoded_frames = GetNumEncodedFrames();
  const float encoded_framerate_fps = num_encoded_frames / video_duration_secs;
  EXPECT_NEAR(encoded_framerate_fps, expected_framerate_fps,
              max_abs_framerate_error_fps);
}

TEST_F(TestVp9ImplFrameDropping, DifferentFrameratePerSpatialLayer) {
  // Assign different frame rate to spatial layers and check that result frame
  // rate is close to the assigned one.
  const uint8_t num_spatial_layers = 3;
  const float input_framerate_fps = 30.0;
  const size_t video_duration_secs = 3;
  const size_t num_input_frames = video_duration_secs * input_framerate_fps;

  codec_settings_.VP9()->numberOfSpatialLayers = num_spatial_layers;
  codec_settings_.VP9()->frameDroppingOn = false;

  VideoBitrateAllocation bitrate_allocation;
  for (uint8_t sl_idx = 0; sl_idx < num_spatial_layers; ++sl_idx) {
    // Frame rate increases from low to high layer.
    const uint32_t framerate_fps = 10 * (sl_idx + 1);

    codec_settings_.spatialLayers[sl_idx].width = codec_settings_.width;
    codec_settings_.spatialLayers[sl_idx].height = codec_settings_.height;
    codec_settings_.spatialLayers[sl_idx].maxFramerate = framerate_fps;
    codec_settings_.spatialLayers[sl_idx].minBitrate =
        codec_settings_.startBitrate;
    codec_settings_.spatialLayers[sl_idx].maxBitrate =
        codec_settings_.startBitrate;
    codec_settings_.spatialLayers[sl_idx].targetBitrate =
        codec_settings_.startBitrate;

    bitrate_allocation.SetBitrate(
        sl_idx, 0, codec_settings_.spatialLayers[sl_idx].targetBitrate * 1000);
  }

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, 1 /* number of cores */,
                                 0 /* max payload size (unused) */));

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->SetRateAllocation(bitrate_allocation,
                                        codec_settings_.maxFramerate));

  VideoFrame* input_frame = NextInputFrame();
  for (size_t frame_num = 0; frame_num < num_input_frames; ++frame_num) {
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->Encode(*input_frame, nullptr, nullptr));
    const size_t timestamp = input_frame->timestamp() +
                             kVideoPayloadTypeFrequency / input_framerate_fps;
    input_frame->set_timestamp(static_cast<uint32_t>(timestamp));
  }

  std::vector<EncodedImage> encoded_frames;
  std::vector<CodecSpecificInfo> codec_infos;
  ASSERT_TRUE(WaitForEncodedFrames(&encoded_frames, &codec_infos));

  std::vector<size_t> num_encoded_frames(num_spatial_layers, 0);
  for (EncodedImage& encoded_frame : encoded_frames) {
    ++num_encoded_frames[encoded_frame.SpatialIndex().value_or(0)];
  }

  for (uint8_t sl_idx = 0; sl_idx < num_spatial_layers; ++sl_idx) {
    const float layer_target_framerate_fps =
        codec_settings_.spatialLayers[sl_idx].maxFramerate;
    const float layer_output_framerate_fps =
        static_cast<float>(num_encoded_frames[sl_idx]) / video_duration_secs;
    const float max_framerate_error_fps = layer_target_framerate_fps * 0.1f;
    EXPECT_NEAR(layer_output_framerate_fps, layer_target_framerate_fps,
                max_framerate_error_fps);
  }
}

TEST_F(TestVp9ImplFrameDropping, LayerMaxFramerateIsCappedByCodecMaxFramerate) {
  const float input_framerate_fps = 30.0;
  const float layer_max_framerate_fps = 30.0;
  const uint32_t codec_max_framerate_fps = layer_max_framerate_fps / 3;
  const size_t video_duration_secs = 3;
  const size_t num_input_frames = video_duration_secs * input_framerate_fps;

  VideoBitrateAllocation bitrate_allocation;
  codec_settings_.spatialLayers[0].width = codec_settings_.width;
  codec_settings_.spatialLayers[0].height = codec_settings_.height;
  codec_settings_.spatialLayers[0].maxFramerate = layer_max_framerate_fps;
  codec_settings_.spatialLayers[0].minBitrate = codec_settings_.startBitrate;
  codec_settings_.spatialLayers[0].maxBitrate = codec_settings_.startBitrate;
  codec_settings_.spatialLayers[0].targetBitrate = codec_settings_.startBitrate;

  bitrate_allocation.SetBitrate(
      0, 0, codec_settings_.spatialLayers[0].targetBitrate * 1000);

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, 1 /* number of cores */,
                                 0 /* max payload size (unused) */));

  EXPECT_EQ(
      WEBRTC_VIDEO_CODEC_OK,
      encoder_->SetRateAllocation(bitrate_allocation, codec_max_framerate_fps));

  VideoFrame* input_frame = NextInputFrame();
  for (size_t frame_num = 0; frame_num < num_input_frames; ++frame_num) {
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->Encode(*input_frame, nullptr, nullptr));
    const size_t timestamp = input_frame->timestamp() +
                             kVideoPayloadTypeFrequency / input_framerate_fps;
    input_frame->set_timestamp(static_cast<uint32_t>(timestamp));
  }

  const size_t num_encoded_frames = GetNumEncodedFrames();
  const float encoded_framerate_fps = num_encoded_frames / video_duration_secs;
  const float max_framerate_error_fps = codec_max_framerate_fps * 0.1f;
  EXPECT_NEAR(encoded_framerate_fps, codec_max_framerate_fps,
              max_framerate_error_fps);
}

class TestVp9ImplProfile2 : public TestVp9Impl {
 protected:
  void SetUp() override {
    // Profile 2 might not be available on some platforms until
    // https://bugs.chromium.org/p/webm/issues/detail?id=1544 is solved.
    bool profile_2_is_supported = false;
    for (const auto& codec : SupportedVP9Codecs()) {
      if (ParseSdpForVP9Profile(codec.parameters)
              .value_or(VP9Profile::kProfile0) == VP9Profile::kProfile2) {
        profile_2_is_supported = true;
      }
    }
    if (!profile_2_is_supported)
      return;

    TestVp9Impl::SetUp();
    input_frame_generator_ = test::FrameGenerator::CreateSquareGenerator(
        codec_settings_.width, codec_settings_.height,
        test::FrameGenerator::OutputType::I010, absl::optional<int>());
  }

  std::unique_ptr<VideoEncoder> CreateEncoder() override {
    cricket::VideoCodec profile2_codec;
    profile2_codec.SetParam(kVP9FmtpProfileId,
                            VP9ProfileToString(VP9Profile::kProfile2));
    return VP9Encoder::Create(profile2_codec);
  }

  std::unique_ptr<VideoDecoder> CreateDecoder() override {
    return VP9Decoder::Create();
  }
};

TEST_F(TestVp9ImplProfile2, EncodeDecode) {
  if (!encoder_)
    return;

  VideoFrame* input_frame = NextInputFrame();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*input_frame, nullptr, nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));
  // First frame should be a key frame.
  encoded_frame._frameType = kVideoFrameKey;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            decoder_->Decode(encoded_frame, false, nullptr, 0));
  std::unique_ptr<VideoFrame> decoded_frame;
  absl::optional<uint8_t> decoded_qp;
  ASSERT_TRUE(WaitForDecodedFrame(&decoded_frame, &decoded_qp));
  ASSERT_TRUE(decoded_frame);

  // TODO(emircan): Add PSNR for different color depths.
  EXPECT_GT(I420PSNR(*input_frame->video_frame_buffer()->ToI420(),
                     *decoded_frame->video_frame_buffer()->ToI420()),
            31);
}

}  // namespace webrtc
