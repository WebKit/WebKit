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
#include "api/video_codecs/video_encoder.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "media/base/vp9_profile.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/video_coding/codecs/test/video_codec_unittest.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "modules/video_coding/codecs/vp9/svc_config.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/video_codec_settings.h"

namespace webrtc {

using ::testing::ElementsAreArray;
using EncoderInfo = webrtc::VideoEncoder::EncoderInfo;
using FramerateFractions =
    absl::InlinedVector<uint8_t, webrtc::kMaxTemporalStreams>;

namespace {
const size_t kWidth = 1280;
const size_t kHeight = 720;

const VideoEncoder::Capabilities kCapabilities(false);
const VideoEncoder::Settings kSettings(kCapabilities,
                                       /*number_of_cores=*/1,
                                       /*max_payload_size=*/0);
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

  void ConfigureSvc(size_t num_spatial_layers, size_t num_temporal_layers = 1) {
    codec_settings_.VP9()->numberOfSpatialLayers =
        static_cast<unsigned char>(num_spatial_layers);
    codec_settings_.VP9()->numberOfTemporalLayers = num_temporal_layers;
    codec_settings_.VP9()->frameDroppingOn = false;

    std::vector<SpatialLayer> layers =
        GetSvcConfig(codec_settings_.width, codec_settings_.height,
                     codec_settings_.maxFramerate, num_spatial_layers,
                     num_temporal_layers, false);
    for (size_t i = 0; i < layers.size(); ++i) {
      codec_settings_.spatialLayers[i] = layers[i];
    }
  }
};

// Disabled on ios as flake, see https://crbug.com/webrtc/7057
#if defined(WEBRTC_IOS)
TEST_F(TestVp9Impl, DISABLED_EncodeDecode) {
#else
TEST_F(TestVp9Impl, EncodeDecode) {
#endif
  VideoFrame* input_frame = NextInputFrame();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, encoder_->Encode(*input_frame, nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));
  // First frame should be a key frame.
  encoded_frame._frameType = VideoFrameType::kVideoFrameKey;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, decoder_->Decode(encoded_frame, false, 0));
  std::unique_ptr<VideoFrame> decoded_frame;
  absl::optional<uint8_t> decoded_qp;
  ASSERT_TRUE(WaitForDecodedFrame(&decoded_frame, &decoded_qp));
  ASSERT_TRUE(decoded_frame);
  EXPECT_GT(I420PSNR(input_frame, decoded_frame.get()), 36);

  const ColorSpace color_space = *decoded_frame->color_space();
  EXPECT_EQ(ColorSpace::PrimaryID::kUnspecified, color_space.primaries());
  EXPECT_EQ(ColorSpace::TransferID::kUnspecified, color_space.transfer());
  EXPECT_EQ(ColorSpace::MatrixID::kUnspecified, color_space.matrix());
  EXPECT_EQ(ColorSpace::RangeID::kLimited, color_space.range());
  EXPECT_EQ(ColorSpace::ChromaSiting::kUnspecified,
            color_space.chroma_siting_horizontal());
  EXPECT_EQ(ColorSpace::ChromaSiting::kUnspecified,
            color_space.chroma_siting_vertical());
}

TEST_F(TestVp9Impl, DecodedColorSpaceFromBitstream) {
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));

  // Encoded frame without explicit color space information.
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, decoder_->Decode(encoded_frame, false, 0));
  std::unique_ptr<VideoFrame> decoded_frame;
  absl::optional<uint8_t> decoded_qp;
  ASSERT_TRUE(WaitForDecodedFrame(&decoded_frame, &decoded_qp));
  ASSERT_TRUE(decoded_frame);
  // Color space present from encoded bitstream.
  ASSERT_TRUE(decoded_frame->color_space());
  // No HDR metadata present.
  EXPECT_FALSE(decoded_frame->color_space()->hdr_metadata());
}

TEST_F(TestVp9Impl, DecodedQpEqualsEncodedQp) {
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));
  // First frame should be a key frame.
  encoded_frame._frameType = VideoFrameType::kVideoFrameKey;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, decoder_->Decode(encoded_frame, false, 0));
  std::unique_ptr<VideoFrame> decoded_frame;
  absl::optional<uint8_t> decoded_qp;
  ASSERT_TRUE(WaitForDecodedFrame(&decoded_frame, &decoded_qp));
  ASSERT_TRUE(decoded_frame);
  ASSERT_TRUE(decoded_qp);
  EXPECT_EQ(encoded_frame.qp_, *decoded_qp);
}

TEST_F(TestVp9Impl, ParserQpEqualsEncodedQp) {
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));

  int qp = 0;
  ASSERT_TRUE(vp9::GetQp(encoded_frame.data(), encoded_frame.size(), &qp));
  EXPECT_EQ(encoded_frame.qp_, qp);
}

TEST_F(TestVp9Impl, EncoderWith2TemporalLayers) {
  // Override default settings.
  codec_settings_.VP9()->numberOfTemporalLayers = 2;
  // Tl0PidIdx is only used in non-flexible mode.
  codec_settings_.VP9()->flexibleMode = false;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, kSettings));

  // Temporal layer 0.
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));
  EXPECT_EQ(0, codec_specific_info.codecSpecific.VP9.temporal_idx);

  // Temporal layer 1.
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr));
  ExpectFrameWith(1);

  // Temporal layer 0.
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr));
  ExpectFrameWith(0);

  // Temporal layer 1.
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr));
  ExpectFrameWith(1);
}

TEST_F(TestVp9Impl, EncoderWith2SpatialLayers) {
  codec_settings_.VP9()->numberOfSpatialLayers = 2;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, kSettings));

  SetWaitForEncodedFramesThreshold(2);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr));
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
  codec_settings_.spatialLayers[0].active = true;

  codec_settings_.spatialLayers[1].minBitrate = 400;
  codec_settings_.spatialLayers[1].maxBitrate = 1500;
  codec_settings_.spatialLayers[1].targetBitrate =
      (codec_settings_.spatialLayers[1].minBitrate +
       codec_settings_.spatialLayers[1].maxBitrate) /
      2;
  codec_settings_.spatialLayers[1].active = true;

  codec_settings_.spatialLayers[0].width = codec_settings_.width / 2;
  codec_settings_.spatialLayers[0].height = codec_settings_.height / 2;
  codec_settings_.spatialLayers[0].maxFramerate = codec_settings_.maxFramerate;
  codec_settings_.spatialLayers[1].width = codec_settings_.width;
  codec_settings_.spatialLayers[1].height = codec_settings_.height;
  codec_settings_.spatialLayers[1].maxFramerate = codec_settings_.maxFramerate;

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, kSettings));

  // Ensure it fails if scaling factors in horz/vert dimentions are different.
  codec_settings_.spatialLayers[0].width = codec_settings_.width;
  codec_settings_.spatialLayers[0].height = codec_settings_.height / 2;
  codec_settings_.spatialLayers[1].width = codec_settings_.width;
  codec_settings_.spatialLayers[1].height = codec_settings_.height;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_ERR_PARAMETER,
            encoder_->InitEncode(&codec_settings_, kSettings));

  // Ensure it fails if scaling factor is not power of two.
  codec_settings_.spatialLayers[0].width = codec_settings_.width / 3;
  codec_settings_.spatialLayers[0].height = codec_settings_.height / 3;
  codec_settings_.spatialLayers[1].width = codec_settings_.width;
  codec_settings_.spatialLayers[1].height = codec_settings_.height;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_ERR_PARAMETER,
            encoder_->InitEncode(&codec_settings_, kSettings));
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
            encoder_->InitEncode(&codec_settings_, kSettings));

  VideoBitrateAllocation bitrate_allocation;
  for (size_t sl_idx = 0; sl_idx < num_spatial_layers; ++sl_idx) {
    // Allocate high bit rate to avoid frame dropping due to rate control.
    bitrate_allocation.SetBitrate(
        sl_idx, 0,
        codec_settings_.spatialLayers[sl_idx].targetBitrate * 1000 * 2);
    encoder_->SetRates(VideoEncoder::RateControlParameters(
        bitrate_allocation, codec_settings_.maxFramerate));

    for (size_t frame_num = 0; frame_num < num_frames_to_encode; ++frame_num) {
      SetWaitForEncodedFramesThreshold(sl_idx + 1);
      EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
                encoder_->Encode(*NextInputFrame(), nullptr));
      std::vector<EncodedImage> encoded_frame;
      std::vector<CodecSpecificInfo> codec_specific_info;
      ASSERT_TRUE(WaitForEncodedFrames(&encoded_frame, &codec_specific_info));
      EXPECT_EQ(codec_specific_info[0].codecSpecific.VP9.ss_data_available,
                frame_num == 0);
    }
  }

  for (size_t i = 0; i < num_spatial_layers - 1; ++i) {
    const size_t sl_idx = num_spatial_layers - i - 1;
    bitrate_allocation.SetBitrate(sl_idx, 0, 0);
    encoder_->SetRates(VideoEncoder::RateControlParameters(
        bitrate_allocation, codec_settings_.maxFramerate));

    for (size_t frame_num = 0; frame_num < num_frames_to_encode; ++frame_num) {
      SetWaitForEncodedFramesThreshold(sl_idx);
      EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
                encoder_->Encode(*NextInputFrame(), nullptr));
      std::vector<EncodedImage> encoded_frame;
      std::vector<CodecSpecificInfo> codec_specific_info;
      ASSERT_TRUE(WaitForEncodedFrames(&encoded_frame, &codec_specific_info));
      EXPECT_EQ(codec_specific_info[0].codecSpecific.VP9.ss_data_available,
                frame_num == 0);
    }
  }
}

TEST_F(TestVp9Impl, EndOfPicture) {
  const size_t num_spatial_layers = 2;
  ConfigureSvc(num_spatial_layers);

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, kSettings));

  // Encode both base and upper layers. Check that end-of-superframe flag is
  // set on upper layer frame but not on base layer frame.
  VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(
      0, 0, codec_settings_.spatialLayers[0].targetBitrate * 1000);
  bitrate_allocation.SetBitrate(
      1, 0, codec_settings_.spatialLayers[1].targetBitrate * 1000);
  encoder_->SetRates(VideoEncoder::RateControlParameters(
      bitrate_allocation, codec_settings_.maxFramerate));
  SetWaitForEncodedFramesThreshold(2);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr));

  std::vector<EncodedImage> frames;
  std::vector<CodecSpecificInfo> codec_specific;
  ASSERT_TRUE(WaitForEncodedFrames(&frames, &codec_specific));
  EXPECT_FALSE(codec_specific[0].codecSpecific.VP9.end_of_picture);
  EXPECT_TRUE(codec_specific[1].codecSpecific.VP9.end_of_picture);

  // Encode only base layer. Check that end-of-superframe flag is
  // set on base layer frame.
  bitrate_allocation.SetBitrate(1, 0, 0);
  encoder_->SetRates(VideoEncoder::RateControlParameters(
      bitrate_allocation, codec_settings_.maxFramerate));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, kSettings));

  SetWaitForEncodedFramesThreshold(1);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr));

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
              encoder_->InitEncode(&codec_settings_, kSettings));

    encoder_->SetRates(VideoEncoder::RateControlParameters(
        bitrate_allocation, codec_settings_.maxFramerate));

    SetWaitForEncodedFramesThreshold(2);
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->Encode(*NextInputFrame(), nullptr));

    std::vector<EncodedImage> frames;
    std::vector<CodecSpecificInfo> codec_specific;
    ASSERT_TRUE(WaitForEncodedFrames(&frames, &codec_specific));

    // Key frame.
    ASSERT_EQ(frames[0].SpatialIndex(), 0);
    ASSERT_FALSE(codec_specific[0].codecSpecific.VP9.inter_pic_predicted);
    EXPECT_FALSE(codec_specific[0].codecSpecific.VP9.inter_layer_predicted);
    EXPECT_EQ(codec_specific[0].codecSpecific.VP9.non_ref_for_inter_layer_pred,
              inter_layer_pred == InterLayerPredMode::kOff);
    EXPECT_TRUE(codec_specific[0].codecSpecific.VP9.ss_data_available);

    ASSERT_EQ(frames[1].SpatialIndex(), 1);
    ASSERT_FALSE(codec_specific[1].codecSpecific.VP9.inter_pic_predicted);
    EXPECT_EQ(codec_specific[1].codecSpecific.VP9.inter_layer_predicted,
              inter_layer_pred == InterLayerPredMode::kOn ||
                  inter_layer_pred == InterLayerPredMode::kOnKeyPic);
    EXPECT_EQ(codec_specific[1].codecSpecific.VP9.ss_data_available,
              inter_layer_pred == InterLayerPredMode::kOff);
    EXPECT_TRUE(
        codec_specific[1].codecSpecific.VP9.non_ref_for_inter_layer_pred);

    // Delta frame.
    SetWaitForEncodedFramesThreshold(2);
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->Encode(*NextInputFrame(), nullptr));
    ASSERT_TRUE(WaitForEncodedFrames(&frames, &codec_specific));

    ASSERT_EQ(frames[0].SpatialIndex(), 0);
    ASSERT_TRUE(codec_specific[0].codecSpecific.VP9.inter_pic_predicted);
    EXPECT_FALSE(codec_specific[0].codecSpecific.VP9.inter_layer_predicted);
    EXPECT_EQ(codec_specific[0].codecSpecific.VP9.non_ref_for_inter_layer_pred,
              inter_layer_pred != InterLayerPredMode::kOn);
    EXPECT_FALSE(codec_specific[0].codecSpecific.VP9.ss_data_available);

    ASSERT_EQ(frames[1].SpatialIndex(), 1);
    ASSERT_TRUE(codec_specific[1].codecSpecific.VP9.inter_pic_predicted);
    EXPECT_EQ(codec_specific[1].codecSpecific.VP9.inter_layer_predicted,
              inter_layer_pred == InterLayerPredMode::kOn);
    EXPECT_TRUE(
        codec_specific[1].codecSpecific.VP9.non_ref_for_inter_layer_pred);
    EXPECT_FALSE(codec_specific[1].codecSpecific.VP9.ss_data_available);
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
              encoder_->InitEncode(&codec_settings_, kSettings));

    VideoBitrateAllocation bitrate_allocation;
    for (size_t sl_idx = 0; sl_idx < num_spatial_layers; ++sl_idx) {
      bitrate_allocation.SetBitrate(
          sl_idx, 0,
          codec_settings_.spatialLayers[sl_idx].targetBitrate * 1000);
      encoder_->SetRates(VideoEncoder::RateControlParameters(
          bitrate_allocation, codec_settings_.maxFramerate));

      for (size_t frame_num = 0; frame_num < num_frames_to_encode;
           ++frame_num) {
        SetWaitForEncodedFramesThreshold(sl_idx + 1);
        EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
                  encoder_->Encode(*NextInputFrame(), nullptr));
        std::vector<EncodedImage> encoded_frame;
        std::vector<CodecSpecificInfo> codec_specific_info;
        ASSERT_TRUE(WaitForEncodedFrames(&encoded_frame, &codec_specific_info));

        const bool is_first_upper_layer_frame = (sl_idx > 0 && frame_num == 0);
        if (is_first_upper_layer_frame) {
          if (inter_layer_pred == InterLayerPredMode::kOn) {
            EXPECT_EQ(encoded_frame[0]._frameType,
                      VideoFrameType::kVideoFrameDelta);
          } else {
            EXPECT_EQ(encoded_frame[0]._frameType,
                      VideoFrameType::kVideoFrameKey);
          }
        } else if (sl_idx == 0 && frame_num == 0) {
          EXPECT_EQ(encoded_frame[0]._frameType,
                    VideoFrameType::kVideoFrameKey);
        } else {
          for (size_t i = 0; i <= sl_idx; ++i) {
            EXPECT_EQ(encoded_frame[i]._frameType,
                      VideoFrameType::kVideoFrameDelta);
          }
        }
      }
    }
  }
}

TEST_F(TestVp9Impl,
       EnablingUpperLayerUnsetsInterPicPredictedInInterlayerPredModeOn) {
  const size_t num_spatial_layers = 3;
  const size_t num_frames_to_encode = 2;

  ConfigureSvc(num_spatial_layers);
  codec_settings_.VP9()->frameDroppingOn = false;
  codec_settings_.VP9()->flexibleMode = false;

  const std::vector<InterLayerPredMode> inter_layer_pred_modes = {
      InterLayerPredMode::kOff, InterLayerPredMode::kOn,
      InterLayerPredMode::kOnKeyPic};

  for (const InterLayerPredMode inter_layer_pred : inter_layer_pred_modes) {
    codec_settings_.VP9()->interLayerPred = inter_layer_pred;
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->InitEncode(&codec_settings_, kSettings));

    VideoBitrateAllocation bitrate_allocation;
    for (size_t sl_idx = 0; sl_idx < num_spatial_layers; ++sl_idx) {
      bitrate_allocation.SetBitrate(
          sl_idx, 0,
          codec_settings_.spatialLayers[sl_idx].targetBitrate * 1000);
      encoder_->SetRates(VideoEncoder::RateControlParameters(
          bitrate_allocation, codec_settings_.maxFramerate));

      for (size_t frame_num = 0; frame_num < num_frames_to_encode;
           ++frame_num) {
        SetWaitForEncodedFramesThreshold(sl_idx + 1);
        EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
                  encoder_->Encode(*NextInputFrame(), nullptr));
        std::vector<EncodedImage> encoded_frame;
        std::vector<CodecSpecificInfo> codec_specific_info;
        ASSERT_TRUE(WaitForEncodedFrames(&encoded_frame, &codec_specific_info));

        ASSERT_EQ(codec_specific_info.size(), sl_idx + 1);

        for (size_t i = 0; i <= sl_idx; ++i) {
          const bool is_keyframe =
              encoded_frame[0]._frameType == VideoFrameType::kVideoFrameKey;
          const bool is_first_upper_layer_frame =
              (i == sl_idx && frame_num == 0);
          // Interframe references are there, unless it's a keyframe,
          // or it's a first activated frame in a upper layer
          const bool expect_no_references =
              is_keyframe || (is_first_upper_layer_frame &&
                              inter_layer_pred == InterLayerPredMode::kOn);
          EXPECT_EQ(
              codec_specific_info[i].codecSpecific.VP9.inter_pic_predicted,
              !expect_no_references);
        }
      }
    }
  }
}

TEST_F(TestVp9Impl, EnablingDisablingUpperLayerInTheSameGof) {
  const size_t num_spatial_layers = 2;
  const size_t num_temporal_layers = 2;

  ConfigureSvc(num_spatial_layers, num_temporal_layers);
  codec_settings_.VP9()->frameDroppingOn = false;
  codec_settings_.VP9()->flexibleMode = false;

  codec_settings_.VP9()->interLayerPred = InterLayerPredMode::kOn;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, kSettings));

  VideoBitrateAllocation bitrate_allocation;

  // Enable both spatial and both temporal layers.
  bitrate_allocation.SetBitrate(
      0, 0, codec_settings_.spatialLayers[0].targetBitrate * 1000 / 2);
  bitrate_allocation.SetBitrate(
      0, 1, codec_settings_.spatialLayers[0].targetBitrate * 1000 / 2);
  bitrate_allocation.SetBitrate(
      1, 0, codec_settings_.spatialLayers[1].targetBitrate * 1000 / 2);
  bitrate_allocation.SetBitrate(
      1, 1, codec_settings_.spatialLayers[1].targetBitrate * 1000 / 2);
  encoder_->SetRates(VideoEncoder::RateControlParameters(
      bitrate_allocation, codec_settings_.maxFramerate));

  std::vector<EncodedImage> encoded_frame;
  std::vector<CodecSpecificInfo> codec_specific_info;

  // Encode 3 frames.
  for (int i = 0; i < 3; ++i) {
    SetWaitForEncodedFramesThreshold(2);
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->Encode(*NextInputFrame(), nullptr));
    ASSERT_TRUE(WaitForEncodedFrames(&encoded_frame, &codec_specific_info));
    ASSERT_EQ(codec_specific_info.size(), 2u);
  }

  // Disable SL1 layer.
  bitrate_allocation.SetBitrate(1, 0, 0);
  bitrate_allocation.SetBitrate(1, 1, 0);
  encoder_->SetRates(VideoEncoder::RateControlParameters(
      bitrate_allocation, codec_settings_.maxFramerate));

  // Encode 1 frame.
  SetWaitForEncodedFramesThreshold(1);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr));
  ASSERT_TRUE(WaitForEncodedFrames(&encoded_frame, &codec_specific_info));
  ASSERT_EQ(codec_specific_info.size(), 1u);
  EXPECT_EQ(encoded_frame[0]._frameType, VideoFrameType::kVideoFrameDelta);
  EXPECT_EQ(codec_specific_info[0].codecSpecific.VP9.temporal_idx, 1);
  EXPECT_EQ(codec_specific_info[0].codecSpecific.VP9.inter_pic_predicted, true);

  // Enable SL1 layer.
  bitrate_allocation.SetBitrate(
      1, 0, codec_settings_.spatialLayers[1].targetBitrate * 1000 / 2);
  bitrate_allocation.SetBitrate(
      1, 1, codec_settings_.spatialLayers[1].targetBitrate * 1000 / 2);
  encoder_->SetRates(VideoEncoder::RateControlParameters(
      bitrate_allocation, codec_settings_.maxFramerate));

  // Encode 1 frame.
  SetWaitForEncodedFramesThreshold(2);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr));
  ASSERT_TRUE(WaitForEncodedFrames(&encoded_frame, &codec_specific_info));
  ASSERT_EQ(codec_specific_info.size(), 2u);
  EXPECT_EQ(encoded_frame[0]._frameType, VideoFrameType::kVideoFrameDelta);
  EXPECT_EQ(codec_specific_info[0].codecSpecific.VP9.temporal_idx, 0);
  EXPECT_EQ(codec_specific_info[0].codecSpecific.VP9.inter_pic_predicted, true);
  EXPECT_EQ(codec_specific_info[1].codecSpecific.VP9.inter_pic_predicted, true);
}

TEST_F(TestVp9Impl, EnablingDisablingUpperLayerAccrossGof) {
  const size_t num_spatial_layers = 2;
  const size_t num_temporal_layers = 2;

  ConfigureSvc(num_spatial_layers, num_temporal_layers);
  codec_settings_.VP9()->frameDroppingOn = false;
  codec_settings_.VP9()->flexibleMode = false;

  codec_settings_.VP9()->interLayerPred = InterLayerPredMode::kOn;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, kSettings));

  VideoBitrateAllocation bitrate_allocation;

  // Enable both spatial and both temporal layers.
  bitrate_allocation.SetBitrate(
      0, 0, codec_settings_.spatialLayers[0].targetBitrate * 1000 / 2);
  bitrate_allocation.SetBitrate(
      0, 1, codec_settings_.spatialLayers[0].targetBitrate * 1000 / 2);
  bitrate_allocation.SetBitrate(
      1, 0, codec_settings_.spatialLayers[1].targetBitrate * 1000 / 2);
  bitrate_allocation.SetBitrate(
      1, 1, codec_settings_.spatialLayers[1].targetBitrate * 1000 / 2);
  encoder_->SetRates(VideoEncoder::RateControlParameters(
      bitrate_allocation, codec_settings_.maxFramerate));

  std::vector<EncodedImage> encoded_frame;
  std::vector<CodecSpecificInfo> codec_specific_info;

  // Encode 3 frames.
  for (int i = 0; i < 3; ++i) {
    SetWaitForEncodedFramesThreshold(2);
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->Encode(*NextInputFrame(), nullptr));
    ASSERT_TRUE(WaitForEncodedFrames(&encoded_frame, &codec_specific_info));
    ASSERT_EQ(codec_specific_info.size(), 2u);
  }

  // Disable SL1 layer.
  bitrate_allocation.SetBitrate(1, 0, 0);
  bitrate_allocation.SetBitrate(1, 1, 0);
  encoder_->SetRates(VideoEncoder::RateControlParameters(
      bitrate_allocation, codec_settings_.maxFramerate));

  // Encode 11 frames. More than Gof length 2, and odd to end at TL1 frame.
  for (int i = 0; i < 11; ++i) {
    SetWaitForEncodedFramesThreshold(1);
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->Encode(*NextInputFrame(), nullptr));
    ASSERT_TRUE(WaitForEncodedFrames(&encoded_frame, &codec_specific_info));
    ASSERT_EQ(codec_specific_info.size(), 1u);
    EXPECT_EQ(encoded_frame[0]._frameType, VideoFrameType::kVideoFrameDelta);
    EXPECT_EQ(codec_specific_info[0].codecSpecific.VP9.temporal_idx, 1 - i % 2);
    EXPECT_EQ(codec_specific_info[0].codecSpecific.VP9.inter_pic_predicted,
              true);
  }

  // Enable SL1 layer.
  bitrate_allocation.SetBitrate(
      1, 0, codec_settings_.spatialLayers[1].targetBitrate * 1000 / 2);
  bitrate_allocation.SetBitrate(
      1, 1, codec_settings_.spatialLayers[1].targetBitrate * 1000 / 2);
  encoder_->SetRates(VideoEncoder::RateControlParameters(
      bitrate_allocation, codec_settings_.maxFramerate));

  // Encode 1 frame.
  SetWaitForEncodedFramesThreshold(2);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr));
  ASSERT_TRUE(WaitForEncodedFrames(&encoded_frame, &codec_specific_info));
  ASSERT_EQ(codec_specific_info.size(), 2u);
  EXPECT_EQ(encoded_frame[0]._frameType, VideoFrameType::kVideoFrameDelta);
  EXPECT_EQ(codec_specific_info[0].codecSpecific.VP9.temporal_idx, 0);
  EXPECT_EQ(codec_specific_info[0].codecSpecific.VP9.inter_pic_predicted, true);
  EXPECT_EQ(codec_specific_info[1].codecSpecific.VP9.inter_pic_predicted,
            false);
}

TEST_F(TestVp9Impl, EnablingNewLayerIsDelayedInScreenshareAndAddsSsInfo) {
  const size_t num_spatial_layers = 3;
  // Chosen by hand, the 2nd frame is dropped with configured per-layer max
  // framerate.
  const size_t num_frames_to_encode_before_drop = 1;
  // Chosen by hand, exactly 5 frames are dropped for input fps=30 and max
  // framerate = 5.
  const size_t num_dropped_frames = 5;

  codec_settings_.maxFramerate = 30;
  ConfigureSvc(num_spatial_layers);
  codec_settings_.spatialLayers[0].maxFramerate = 5.0;
  // use 30 for the SL 1 instead of 10, so even if SL 0 frame is dropped due to
  // framerate capping we would still get back at least a middle layer. It
  // simplifies the test.
  codec_settings_.spatialLayers[1].maxFramerate = 30.0;
  codec_settings_.spatialLayers[2].maxFramerate = 30.0;
  codec_settings_.VP9()->frameDroppingOn = false;
  codec_settings_.mode = VideoCodecMode::kScreensharing;
  codec_settings_.VP9()->interLayerPred = InterLayerPredMode::kOn;
  codec_settings_.VP9()->flexibleMode = true;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, kSettings));

  // Enable all but the last layer.
  VideoBitrateAllocation bitrate_allocation;
  for (size_t sl_idx = 0; sl_idx < num_spatial_layers - 1; ++sl_idx) {
    bitrate_allocation.SetBitrate(
        sl_idx, 0, codec_settings_.spatialLayers[sl_idx].targetBitrate * 1000);
  }
  encoder_->SetRates(VideoEncoder::RateControlParameters(
      bitrate_allocation, codec_settings_.maxFramerate));

  // Encode enough frames to force drop due to framerate capping.
  for (size_t frame_num = 0; frame_num < num_frames_to_encode_before_drop;
       ++frame_num) {
    SetWaitForEncodedFramesThreshold(num_spatial_layers - 1);
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->Encode(*NextInputFrame(), nullptr));
    std::vector<EncodedImage> encoded_frames;
    std::vector<CodecSpecificInfo> codec_specific_info;
    ASSERT_TRUE(WaitForEncodedFrames(&encoded_frames, &codec_specific_info));
  }

  // Enable the last layer.
  bitrate_allocation.SetBitrate(
      num_spatial_layers - 1, 0,
      codec_settings_.spatialLayers[num_spatial_layers - 1].targetBitrate *
          1000);
  encoder_->SetRates(VideoEncoder::RateControlParameters(
      bitrate_allocation, codec_settings_.maxFramerate));

  for (size_t frame_num = 0; frame_num < num_dropped_frames; ++frame_num) {
    SetWaitForEncodedFramesThreshold(1);
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->Encode(*NextInputFrame(), nullptr));
    // First layer is dropped due to frame rate cap. The last layer should not
    // be enabled yet.
    std::vector<EncodedImage> encoded_frames;
    std::vector<CodecSpecificInfo> codec_specific_info;
    ASSERT_TRUE(WaitForEncodedFrames(&encoded_frames, &codec_specific_info));
  }

  SetWaitForEncodedFramesThreshold(2);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr));
  // Now all 3 layers should be encoded.
  std::vector<EncodedImage> encoded_frames;
  std::vector<CodecSpecificInfo> codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrames(&encoded_frames, &codec_specific_info));
  EXPECT_EQ(encoded_frames.size(), 3u);
  // Scalability structure has to be triggered.
  EXPECT_TRUE(codec_specific_info[0].codecSpecific.VP9.ss_data_available);
}

TEST_F(TestVp9Impl, ScreenshareFrameDropping) {
  const int num_spatial_layers = 3;
  const int num_frames_to_detect_drops = 2;

  codec_settings_.maxFramerate = 30;
  ConfigureSvc(num_spatial_layers);
  // use 30 for the SL0 and SL1 because it simplifies the test.
  codec_settings_.spatialLayers[0].maxFramerate = 30.0;
  codec_settings_.spatialLayers[1].maxFramerate = 30.0;
  codec_settings_.spatialLayers[2].maxFramerate = 30.0;
  codec_settings_.VP9()->frameDroppingOn = true;
  codec_settings_.mode = VideoCodecMode::kScreensharing;
  codec_settings_.VP9()->interLayerPred = InterLayerPredMode::kOn;
  codec_settings_.VP9()->flexibleMode = true;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, kSettings));

  // Enable all but the last layer.
  VideoBitrateAllocation bitrate_allocation;
  // Very low bitrate for the lowest spatial layer to ensure rate-control drops.
  bitrate_allocation.SetBitrate(0, 0, 1000);
  bitrate_allocation.SetBitrate(
      1, 0, codec_settings_.spatialLayers[1].targetBitrate * 1000);
  // Disable highest layer.
  bitrate_allocation.SetBitrate(2, 0, 0);

  encoder_->SetRates(VideoEncoder::RateControlParameters(
      bitrate_allocation, codec_settings_.maxFramerate));

  bool frame_dropped = false;
  // Encode enough frames to force drop due to rate-control.
  for (size_t frame_num = 0; frame_num < num_frames_to_detect_drops;
       ++frame_num) {
    SetWaitForEncodedFramesThreshold(1);
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->Encode(*NextInputFrame(), nullptr));
    std::vector<EncodedImage> encoded_frames;
    std::vector<CodecSpecificInfo> codec_specific_info;
    ASSERT_TRUE(WaitForEncodedFrames(&encoded_frames, &codec_specific_info));
    EXPECT_LE(encoded_frames.size(), 2u);
    EXPECT_GE(encoded_frames.size(), 1u);
    if (encoded_frames.size() == 1) {
      frame_dropped = true;
      // Dropped frame is on the SL0.
      EXPECT_EQ(encoded_frames[0].SpatialIndex(), 1);
    }
  }
  EXPECT_TRUE(frame_dropped);

  // Enable the last layer.
  bitrate_allocation.SetBitrate(
      2, 0, codec_settings_.spatialLayers[2].targetBitrate * 1000);
  encoder_->SetRates(VideoEncoder::RateControlParameters(
      bitrate_allocation, codec_settings_.maxFramerate));
  SetWaitForEncodedFramesThreshold(1);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr));
  std::vector<EncodedImage> encoded_frames;
  std::vector<CodecSpecificInfo> codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrames(&encoded_frames, &codec_specific_info));
  // No drop allowed.
  EXPECT_EQ(encoded_frames.size(), 3u);

  // Verify that frame-dropping is re-enabled back.
  frame_dropped = false;
  // Encode enough frames to force drop due to rate-control.
  for (size_t frame_num = 0; frame_num < num_frames_to_detect_drops;
       ++frame_num) {
    SetWaitForEncodedFramesThreshold(1);
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->Encode(*NextInputFrame(), nullptr));
    std::vector<EncodedImage> encoded_frames;
    std::vector<CodecSpecificInfo> codec_specific_info;
    ASSERT_TRUE(WaitForEncodedFrames(&encoded_frames, &codec_specific_info));
    EXPECT_LE(encoded_frames.size(), 3u);
    EXPECT_GE(encoded_frames.size(), 2u);
    if (encoded_frames.size() == 2) {
      frame_dropped = true;
      // Dropped frame is on the SL0.
      EXPECT_EQ(encoded_frames[0].SpatialIndex(), 1);
      EXPECT_EQ(encoded_frames[1].SpatialIndex(), 2);
    }
  }
  EXPECT_TRUE(frame_dropped);
}

TEST_F(TestVp9Impl, RemovingLayerIsNotDelayedInScreenshareAndAddsSsInfo) {
  const size_t num_spatial_layers = 3;
  // Chosen by hand, the 2nd frame is dropped with configured per-layer max
  // framerate.
  const size_t num_frames_to_encode_before_drop = 1;
  // Chosen by hand, exactly 5 frames are dropped for input fps=30 and max
  // framerate = 5.
  const size_t num_dropped_frames = 5;

  codec_settings_.maxFramerate = 30;
  ConfigureSvc(num_spatial_layers);
  codec_settings_.spatialLayers[0].maxFramerate = 5.0;
  // use 30 for the SL 1 instead of 5, so even if SL 0 frame is dropped due to
  // framerate capping we would still get back at least a middle layer. It
  // simplifies the test.
  codec_settings_.spatialLayers[1].maxFramerate = 30.0;
  codec_settings_.spatialLayers[2].maxFramerate = 30.0;
  codec_settings_.VP9()->frameDroppingOn = false;
  codec_settings_.mode = VideoCodecMode::kScreensharing;
  codec_settings_.VP9()->interLayerPred = InterLayerPredMode::kOn;
  codec_settings_.VP9()->flexibleMode = true;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, kSettings));

  // All layers are enabled from the start.
  VideoBitrateAllocation bitrate_allocation;
  for (size_t sl_idx = 0; sl_idx < num_spatial_layers; ++sl_idx) {
    bitrate_allocation.SetBitrate(
        sl_idx, 0, codec_settings_.spatialLayers[sl_idx].targetBitrate * 1000);
  }
  encoder_->SetRates(VideoEncoder::RateControlParameters(
      bitrate_allocation, codec_settings_.maxFramerate));

  // Encode enough frames to force drop due to framerate capping.
  for (size_t frame_num = 0; frame_num < num_frames_to_encode_before_drop;
       ++frame_num) {
    SetWaitForEncodedFramesThreshold(num_spatial_layers);
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->Encode(*NextInputFrame(), nullptr));
    std::vector<EncodedImage> encoded_frames;
    std::vector<CodecSpecificInfo> codec_specific_info;
    ASSERT_TRUE(WaitForEncodedFrames(&encoded_frames, &codec_specific_info));
  }

  // Now the first layer should not have frames in it.
  for (size_t frame_num = 0; frame_num < num_dropped_frames - 2; ++frame_num) {
    SetWaitForEncodedFramesThreshold(2);
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->Encode(*NextInputFrame(), nullptr));
    // First layer is dropped due to frame rate cap. The last layer should not
    // be enabled yet.
    std::vector<EncodedImage> encoded_frames;
    std::vector<CodecSpecificInfo> codec_specific_info;
    ASSERT_TRUE(WaitForEncodedFrames(&encoded_frames, &codec_specific_info));
    // First layer is skipped.
    EXPECT_EQ(encoded_frames[0].SpatialIndex().value_or(-1), 1);
  }

  // Disable the last layer.
  bitrate_allocation.SetBitrate(num_spatial_layers - 1, 0, 0);
  encoder_->SetRates(VideoEncoder::RateControlParameters(
      bitrate_allocation, codec_settings_.maxFramerate));

  // Still expected to drop first layer. Last layer has to be disable also.
  for (size_t frame_num = num_dropped_frames - 2;
       frame_num < num_dropped_frames; ++frame_num) {
    // Expect back one frame.
    SetWaitForEncodedFramesThreshold(1);
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->Encode(*NextInputFrame(), nullptr));
    // First layer is dropped due to frame rate cap. The last layer should not
    // be enabled yet.
    std::vector<EncodedImage> encoded_frames;
    std::vector<CodecSpecificInfo> codec_specific_info;
    ASSERT_TRUE(WaitForEncodedFrames(&encoded_frames, &codec_specific_info));
    // First layer is skipped.
    EXPECT_EQ(encoded_frames[0].SpatialIndex().value_or(-1), 1);
    // No SS data on non-base spatial layer.
    EXPECT_FALSE(codec_specific_info[0].codecSpecific.VP9.ss_data_available);
  }

  SetWaitForEncodedFramesThreshold(2);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr));
  std::vector<EncodedImage> encoded_frames;
  std::vector<CodecSpecificInfo> codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrames(&encoded_frames, &codec_specific_info));
  // First layer is not skipped now.
  EXPECT_EQ(encoded_frames[0].SpatialIndex().value_or(-1), 0);
  // SS data should be present.
  EXPECT_TRUE(codec_specific_info[0].codecSpecific.VP9.ss_data_available);
}

TEST_F(TestVp9Impl, DisableNewLayerInVideoDelaysSsInfoTillTL0) {
  const size_t num_spatial_layers = 3;
  const size_t num_temporal_layers = 2;
  // Chosen by hand, the 2nd frame is dropped with configured per-layer max
  // framerate.
  ConfigureSvc(num_spatial_layers, num_temporal_layers);
  codec_settings_.VP9()->frameDroppingOn = false;
  codec_settings_.mode = VideoCodecMode::kRealtimeVideo;
  codec_settings_.VP9()->interLayerPred = InterLayerPredMode::kOnKeyPic;
  codec_settings_.VP9()->flexibleMode = false;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, kSettings));

  // Enable all the layers.
  VideoBitrateAllocation bitrate_allocation;
  for (size_t sl_idx = 0; sl_idx < num_spatial_layers; ++sl_idx) {
    for (size_t tl_idx = 0; tl_idx < num_temporal_layers; ++tl_idx) {
      bitrate_allocation.SetBitrate(
          sl_idx, tl_idx,
          codec_settings_.spatialLayers[sl_idx].targetBitrate * 1000 /
              num_temporal_layers);
    }
  }
  encoder_->SetRates(VideoEncoder::RateControlParameters(
      bitrate_allocation, codec_settings_.maxFramerate));

  std::vector<EncodedImage> encoded_frames;
  std::vector<CodecSpecificInfo> codec_specific_info;

  // Encode one TL0 frame
  SetWaitForEncodedFramesThreshold(num_spatial_layers);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr));
  ASSERT_TRUE(WaitForEncodedFrames(&encoded_frames, &codec_specific_info));
  EXPECT_EQ(codec_specific_info[0].codecSpecific.VP9.temporal_idx, 0u);

  // Disable the last layer.
  for (size_t tl_idx = 0; tl_idx < num_temporal_layers; ++tl_idx) {
    bitrate_allocation.SetBitrate(num_spatial_layers - 1, tl_idx, 0);
  }
  encoder_->SetRates(VideoEncoder::RateControlParameters(
      bitrate_allocation, codec_settings_.maxFramerate));

  // Next is TL1 frame. The last layer is disabled immediately, but SS structure
  // is not provided here.
  SetWaitForEncodedFramesThreshold(num_spatial_layers - 1);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr));
  ASSERT_TRUE(WaitForEncodedFrames(&encoded_frames, &codec_specific_info));
  EXPECT_EQ(codec_specific_info[0].codecSpecific.VP9.temporal_idx, 1u);
  EXPECT_FALSE(codec_specific_info[0].codecSpecific.VP9.ss_data_available);

  // Next is TL0 frame, which should have delayed SS structure.
  SetWaitForEncodedFramesThreshold(num_spatial_layers - 1);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr));
  ASSERT_TRUE(WaitForEncodedFrames(&encoded_frames, &codec_specific_info));
  EXPECT_EQ(codec_specific_info[0].codecSpecific.VP9.temporal_idx, 0u);
  EXPECT_TRUE(codec_specific_info[0].codecSpecific.VP9.ss_data_available);
  EXPECT_TRUE(codec_specific_info[0]
                  .codecSpecific.VP9.spatial_layer_resolution_present);
  EXPECT_EQ(codec_specific_info[0].codecSpecific.VP9.num_spatial_layers,
            num_spatial_layers - 1);
}

TEST_F(TestVp9Impl,
       LowLayerMarkedAsRefIfHighLayerNotEncodedAndInterLayerPredIsEnabled) {
  ConfigureSvc(3);
  codec_settings_.VP9()->frameDroppingOn = false;
  codec_settings_.VP9()->interLayerPred = InterLayerPredMode::kOn;

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, kSettings));

  VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(
      0, 0, codec_settings_.spatialLayers[0].targetBitrate * 1000);
  encoder_->SetRates(VideoEncoder::RateControlParameters(
      bitrate_allocation, codec_settings_.maxFramerate));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_info));
  EXPECT_TRUE(codec_info.codecSpecific.VP9.ss_data_available);
  EXPECT_FALSE(codec_info.codecSpecific.VP9.non_ref_for_inter_layer_pred);
}

TEST_F(TestVp9Impl, ScalabilityStructureIsAvailableInFlexibleMode) {
  codec_settings_.VP9()->flexibleMode = true;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, kSettings));

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));
  EXPECT_TRUE(codec_specific_info.codecSpecific.VP9.ss_data_available);
}

TEST_F(TestVp9Impl, EncoderInfoFpsAllocation) {
  const uint8_t kNumSpatialLayers = 3;
  const uint8_t kNumTemporalLayers = 3;

  codec_settings_.maxFramerate = 30;
  codec_settings_.VP9()->numberOfSpatialLayers = kNumSpatialLayers;
  codec_settings_.VP9()->numberOfTemporalLayers = kNumTemporalLayers;

  for (uint8_t sl_idx = 0; sl_idx < kNumSpatialLayers; ++sl_idx) {
    codec_settings_.spatialLayers[sl_idx].width = codec_settings_.width;
    codec_settings_.spatialLayers[sl_idx].height = codec_settings_.height;
    codec_settings_.spatialLayers[sl_idx].minBitrate =
        codec_settings_.startBitrate;
    codec_settings_.spatialLayers[sl_idx].maxBitrate =
        codec_settings_.startBitrate;
    codec_settings_.spatialLayers[sl_idx].targetBitrate =
        codec_settings_.startBitrate;
    codec_settings_.spatialLayers[sl_idx].active = true;
    codec_settings_.spatialLayers[sl_idx].maxFramerate =
        codec_settings_.maxFramerate;
  }

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, kSettings));

  FramerateFractions expected_fps_allocation[kMaxSpatialLayers];
  expected_fps_allocation[0].push_back(EncoderInfo::kMaxFramerateFraction / 4);
  expected_fps_allocation[0].push_back(EncoderInfo::kMaxFramerateFraction / 2);
  expected_fps_allocation[0].push_back(EncoderInfo::kMaxFramerateFraction);
  expected_fps_allocation[1] = expected_fps_allocation[0];
  expected_fps_allocation[2] = expected_fps_allocation[0];
  EXPECT_THAT(encoder_->GetEncoderInfo().fps_allocation,
              ::testing::ElementsAreArray(expected_fps_allocation));
}

TEST_F(TestVp9Impl, EncoderInfoFpsAllocationFlexibleMode) {
  const uint8_t kNumSpatialLayers = 3;

  codec_settings_.maxFramerate = 30;
  codec_settings_.VP9()->numberOfSpatialLayers = kNumSpatialLayers;
  codec_settings_.VP9()->numberOfTemporalLayers = 1;
  codec_settings_.VP9()->flexibleMode = true;

  for (uint8_t sl_idx = 0; sl_idx < kNumSpatialLayers; ++sl_idx) {
    codec_settings_.spatialLayers[sl_idx].width = codec_settings_.width;
    codec_settings_.spatialLayers[sl_idx].height = codec_settings_.height;
    codec_settings_.spatialLayers[sl_idx].minBitrate =
        codec_settings_.startBitrate;
    codec_settings_.spatialLayers[sl_idx].maxBitrate =
        codec_settings_.startBitrate;
    codec_settings_.spatialLayers[sl_idx].targetBitrate =
        codec_settings_.startBitrate;
    codec_settings_.spatialLayers[sl_idx].active = true;
    // Force different frame rates for different layers, to verify that total
    // fraction is correct.
    codec_settings_.spatialLayers[sl_idx].maxFramerate =
        codec_settings_.maxFramerate / (kNumSpatialLayers - sl_idx);
  }

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, kSettings));

  // No temporal layers allowed when spatial layers have different fps targets.
  FramerateFractions expected_fps_allocation[kMaxSpatialLayers];
  expected_fps_allocation[0].push_back(EncoderInfo::kMaxFramerateFraction / 3);
  expected_fps_allocation[1].push_back(EncoderInfo::kMaxFramerateFraction / 2);
  expected_fps_allocation[2].push_back(EncoderInfo::kMaxFramerateFraction);
  EXPECT_THAT(encoder_->GetEncoderInfo().fps_allocation,
              ::testing::ElementsAreArray(expected_fps_allocation));
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
            encoder_->InitEncode(&codec_settings_, kSettings));

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
              encoder_->Encode(*NextInputFrame(), nullptr));

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
            encoder_->InitEncode(&codec_settings_, kSettings));

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
              encoder_->Encode(*NextInputFrame(), nullptr));

    const bool is_key_frame = frame_num == 0;
    const size_t gof_idx = frame_num % gof.num_frames_in_gof;
    const std::vector<uint8_t> p_diff(std::begin(gof.pid_diff[gof_idx]),
                                      std::end(gof.pid_diff[gof_idx]));

    ExpectFrameWith(num_spatial_layers_, gof.temporal_idx[gof_idx],
                    gof.temporal_up_switch[gof_idx],
                    is_key_frame ? 0 : gof.num_ref_pics[gof_idx], p_diff);
  }
}

INSTANTIATE_TEST_SUITE_P(,
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
            encoder_->InitEncode(&codec_settings_, kSettings));

  VideoFrame* input_frame = NextInputFrame();
  for (size_t frame_num = 0; frame_num < num_frames_to_encode; ++frame_num) {
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, encoder_->Encode(*input_frame, nullptr));
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
  codec_settings_.VP9()->flexibleMode = true;

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
    codec_settings_.spatialLayers[sl_idx].active = true;

    bitrate_allocation.SetBitrate(
        sl_idx, 0, codec_settings_.spatialLayers[sl_idx].targetBitrate * 1000);
  }

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, kSettings));

  encoder_->SetRates(VideoEncoder::RateControlParameters(
      bitrate_allocation, codec_settings_.maxFramerate));

  VideoFrame* input_frame = NextInputFrame();
  for (size_t frame_num = 0; frame_num < num_input_frames; ++frame_num) {
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, encoder_->Encode(*input_frame, nullptr));
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
        test::FrameGenerator::OutputType::kI010, absl::optional<int>());
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
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, encoder_->Encode(*input_frame, nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));
  // First frame should be a key frame.
  encoded_frame._frameType = VideoFrameType::kVideoFrameKey;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, decoder_->Decode(encoded_frame, false, 0));
  std::unique_ptr<VideoFrame> decoded_frame;
  absl::optional<uint8_t> decoded_qp;
  ASSERT_TRUE(WaitForDecodedFrame(&decoded_frame, &decoded_qp));
  ASSERT_TRUE(decoded_frame);

  // TODO(emircan): Add PSNR for different color depths.
  EXPECT_GT(I420PSNR(*input_frame->video_frame_buffer()->ToI420(),
                     *decoded_frame->video_frame_buffer()->ToI420()),
            31);
}

TEST_F(TestVp9Impl, EncodeWithDynamicRate) {
  // Configured dynamic rate field trial and re-create the encoder.
  test::ScopedFieldTrials field_trials(
      "WebRTC-VideoRateControl/vp9_dynamic_rate:true/");
  SetUp();

  // Set 300kbps target with 100% headroom.
  VideoEncoder::RateControlParameters params;
  params.bandwidth_allocation = DataRate::bps(300000);
  params.bitrate.SetBitrate(0, 0, params.bandwidth_allocation.bps());
  params.framerate_fps = 30.0;

  encoder_->SetRates(params);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));

  // Set no headroom and encode again.
  params.bandwidth_allocation = DataRate::Zero();
  encoder_->SetRates(params);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr));
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));
}

TEST_F(TestVp9Impl, ReenablingUpperLayerAfterKFWithInterlayerPredIsEnabled) {
  const size_t num_spatial_layers = 2;
  const int num_frames_to_encode = 10;
  codec_settings_.VP9()->flexibleMode = true;
  codec_settings_.VP9()->frameDroppingOn = false;
  codec_settings_.VP9()->numberOfSpatialLayers = num_spatial_layers;
  codec_settings_.VP9()->numberOfTemporalLayers = 1;
  codec_settings_.VP9()->interLayerPred = InterLayerPredMode::kOn;
  // Force low frame-rate, so all layers are present for all frames.
  codec_settings_.maxFramerate = 5;

  ConfigureSvc(num_spatial_layers);

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, kSettings));

  VideoBitrateAllocation bitrate_allocation;
  for (size_t sl_idx = 0; sl_idx < num_spatial_layers; ++sl_idx) {
    bitrate_allocation.SetBitrate(
        sl_idx, 0, codec_settings_.spatialLayers[sl_idx].targetBitrate * 1000);
  }
  encoder_->SetRates(VideoEncoder::RateControlParameters(
      bitrate_allocation, codec_settings_.maxFramerate));

  std::vector<EncodedImage> encoded_frames;
  std::vector<CodecSpecificInfo> codec_specific;

  for (int i = 0; i < num_frames_to_encode; ++i) {
    SetWaitForEncodedFramesThreshold(num_spatial_layers);
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->Encode(*NextInputFrame(), nullptr));
    ASSERT_TRUE(WaitForEncodedFrames(&encoded_frames, &codec_specific));
    EXPECT_EQ(encoded_frames.size(), num_spatial_layers);
  }

  // Disable the last layer.
  bitrate_allocation.SetBitrate(num_spatial_layers - 1, 0, 0);
  encoder_->SetRates(VideoEncoder::RateControlParameters(
      bitrate_allocation, codec_settings_.maxFramerate));

  for (int i = 0; i < num_frames_to_encode; ++i) {
    SetWaitForEncodedFramesThreshold(num_spatial_layers - 1);
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->Encode(*NextInputFrame(), nullptr));
    ASSERT_TRUE(WaitForEncodedFrames(&encoded_frames, &codec_specific));
    EXPECT_EQ(encoded_frames.size(), num_spatial_layers - 1);
  }

  std::vector<VideoFrameType> frame_types = {VideoFrameType::kVideoFrameKey};

  // Force a key-frame with the last layer still disabled.
  SetWaitForEncodedFramesThreshold(num_spatial_layers - 1);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), &frame_types));
  ASSERT_TRUE(WaitForEncodedFrames(&encoded_frames, &codec_specific));
  EXPECT_EQ(encoded_frames.size(), num_spatial_layers - 1);
  ASSERT_EQ(encoded_frames[0]._frameType, VideoFrameType::kVideoFrameKey);

  // Re-enable the last layer.
  bitrate_allocation.SetBitrate(
      num_spatial_layers - 1, 0,
      codec_settings_.spatialLayers[num_spatial_layers - 1].targetBitrate *
          1000);
  encoder_->SetRates(VideoEncoder::RateControlParameters(
      bitrate_allocation, codec_settings_.maxFramerate));

  SetWaitForEncodedFramesThreshold(num_spatial_layers);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr));
  ASSERT_TRUE(WaitForEncodedFrames(&encoded_frames, &codec_specific));
  EXPECT_EQ(encoded_frames.size(), num_spatial_layers);
  EXPECT_EQ(encoded_frames[0]._frameType, VideoFrameType::kVideoFrameDelta);
}

}  // namespace webrtc
