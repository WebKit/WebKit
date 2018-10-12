/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include <memory>

#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "modules/video_coding/codecs/test/video_codec_unittest.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/codecs/vp8/include/vp8_temporal_layers.h"
#include "modules/video_coding/utility/vp8_header_parser.h"
#include "rtc_base/timeutils.h"
#include "test/video_codec_settings.h"

namespace webrtc {

namespace {
constexpr uint32_t kInitialTimestampRtp = 123;
constexpr int64_t kTestNtpTimeMs = 456;
constexpr int64_t kInitialTimestampMs = 789;
constexpr int kNumCores = 1;
constexpr size_t kMaxPayloadSize = 1440;
constexpr int kDefaultMinPixelsPerFrame = 320 * 180;
constexpr int kWidth = 172;
constexpr int kHeight = 144;
constexpr float kFramerateFps = 30;
}  // namespace

class TestVp8Impl : public VideoCodecUnitTest {
 protected:
  std::unique_ptr<VideoEncoder> CreateEncoder() override {
    return VP8Encoder::Create();
  }

  std::unique_ptr<VideoDecoder> CreateDecoder() override {
    return VP8Decoder::Create();
  }

  void ModifyCodecSettings(VideoCodec* codec_settings) override {
    webrtc::test::CodecSettings(kVideoCodecVP8, codec_settings);
    codec_settings->width = kWidth;
    codec_settings->height = kHeight;
    codec_settings->VP8()->denoisingOn = true;
    codec_settings->VP8()->frameDroppingOn = false;
    codec_settings->VP8()->automaticResizeOn = false;
    codec_settings->VP8()->complexity = VideoCodecComplexity::kComplexityNormal;
  }

  void EncodeAndWaitForFrame(const VideoFrame& input_frame,
                             EncodedImage* encoded_frame,
                             CodecSpecificInfo* codec_specific_info,
                             bool keyframe = false) {
    std::vector<FrameType> frame_types;
    if (keyframe) {
      frame_types.emplace_back(FrameType::kVideoFrameKey);
    } else {
      frame_types.emplace_back(FrameType::kVideoFrameDelta);
    }
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->Encode(input_frame, nullptr, &frame_types));
    ASSERT_TRUE(WaitForEncodedFrame(encoded_frame, codec_specific_info));
    VerifyQpParser(*encoded_frame);
    EXPECT_STREQ("libvpx", codec_specific_info->codec_name);
    EXPECT_EQ(kVideoCodecVP8, codec_specific_info->codecType);
    EXPECT_EQ(0, encoded_frame->SpatialIndex());
  }

  void EncodeAndExpectFrameWith(const VideoFrame& input_frame,
                                uint8_t temporal_idx,
                                bool keyframe = false) {
    EncodedImage encoded_frame;
    CodecSpecificInfo codec_specific_info;
    EncodeAndWaitForFrame(input_frame, &encoded_frame, &codec_specific_info,
                          keyframe);
    EXPECT_EQ(temporal_idx, codec_specific_info.codecSpecific.VP8.temporalIdx);
  }

  void VerifyQpParser(const EncodedImage& encoded_frame) const {
    int qp;
    EXPECT_GT(encoded_frame._length, 0u);
    ASSERT_TRUE(vp8::GetQp(encoded_frame._buffer, encoded_frame._length, &qp));
    EXPECT_EQ(encoded_frame.qp_, qp) << "Encoder QP != parsed bitstream QP.";
  }
};

TEST_F(TestVp8Impl, SetRateAllocation) {
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, encoder_->Release());

  const int kBitrateBps = 300000;
  VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(0, 0, kBitrateBps);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_UNINITIALIZED,
            encoder_->SetRateAllocation(bitrate_allocation,
                                        codec_settings_.maxFramerate));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, kNumCores, kMaxPayloadSize));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->SetRateAllocation(bitrate_allocation,
                                        codec_settings_.maxFramerate));
}

TEST_F(TestVp8Impl, EncodeFrameAndRelease) {
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, encoder_->Release());
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, kNumCores, kMaxPayloadSize));

  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  EncodeAndWaitForFrame(*NextInputFrame(), &encoded_frame,
                        &codec_specific_info);

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, encoder_->Release());
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_UNINITIALIZED,
            encoder_->Encode(*NextInputFrame(), nullptr, nullptr));
}

TEST_F(TestVp8Impl, InitDecode) {
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, decoder_->Release());
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            decoder_->InitDecode(&codec_settings_, kNumCores));
}

TEST_F(TestVp8Impl, OnEncodedImageReportsInfo) {
  VideoFrame* input_frame = NextInputFrame();
  input_frame->set_timestamp(kInitialTimestampRtp);
  input_frame->set_timestamp_us(kInitialTimestampMs *
                                rtc::kNumMicrosecsPerMillisec);
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  EncodeAndWaitForFrame(*input_frame, &encoded_frame, &codec_specific_info);

  EXPECT_EQ(kInitialTimestampRtp, encoded_frame.Timestamp());
  EXPECT_EQ(kInitialTimestampMs, encoded_frame.capture_time_ms_);
  EXPECT_EQ(kWidth, static_cast<int>(encoded_frame._encodedWidth));
  EXPECT_EQ(kHeight, static_cast<int>(encoded_frame._encodedHeight));
}

// We only test the encoder here, since the decoded frame rotation is set based
// on the CVO RTP header extension in VCMDecodedFrameCallback::Decoded.
// TODO(brandtr): Consider passing through the rotation flag through the decoder
// in the same way as done in the encoder.
TEST_F(TestVp8Impl, EncodedRotationEqualsInputRotation) {
  VideoFrame* input_frame = NextInputFrame();
  input_frame->set_rotation(kVideoRotation_0);

  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  EncodeAndWaitForFrame(*input_frame, &encoded_frame, &codec_specific_info);
  EXPECT_EQ(kVideoRotation_0, encoded_frame.rotation_);

  input_frame->set_rotation(kVideoRotation_90);
  EncodeAndWaitForFrame(*input_frame, &encoded_frame, &codec_specific_info);
  EXPECT_EQ(kVideoRotation_90, encoded_frame.rotation_);
}

TEST_F(TestVp8Impl, DecodedQpEqualsEncodedQp) {
  VideoFrame* input_frame = NextInputFrame();
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  EncodeAndWaitForFrame(*input_frame, &encoded_frame, &codec_specific_info);

  // First frame should be a key frame.
  encoded_frame._frameType = kVideoFrameKey;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            decoder_->Decode(encoded_frame, false, nullptr, -1));
  std::unique_ptr<VideoFrame> decoded_frame;
  absl::optional<uint8_t> decoded_qp;
  ASSERT_TRUE(WaitForDecodedFrame(&decoded_frame, &decoded_qp));
  ASSERT_TRUE(decoded_frame);
  ASSERT_TRUE(decoded_qp);
  EXPECT_GT(I420PSNR(input_frame, decoded_frame.get()), 36);
  EXPECT_EQ(encoded_frame.qp_, *decoded_qp);
}

TEST_F(TestVp8Impl, ChecksSimulcastSettings) {
  codec_settings_.numberOfSimulcastStreams = 2;
  // Reslutions are not scaled by 2, temporal layers do not match.
  codec_settings_.simulcastStream[0] = {kWidth, kHeight, kFramerateFps, 2,
                                        4000,   3000,    2000,          80};
  codec_settings_.simulcastStream[1] = {kWidth, kHeight, 30,   3,
                                        4000,   3000,    2000, 80};
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_ERR_SIMULCAST_PARAMETERS_NOT_SUPPORTED,
            encoder_->InitEncode(&codec_settings_, kNumCores, kMaxPayloadSize));
  codec_settings_.numberOfSimulcastStreams = 3;
  // Reslutions are not scaled by 2.
  codec_settings_.simulcastStream[0] = {
      kWidth / 2, kHeight / 2, kFramerateFps, 1, 4000, 3000, 2000, 80};
  codec_settings_.simulcastStream[1] = {
      kWidth / 2, kHeight / 2, kFramerateFps, 1, 4000, 3000, 2000, 80};
  codec_settings_.simulcastStream[2] = {kWidth, kHeight, 30,   1,
                                        4000,   3000,    2000, 80};
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_ERR_SIMULCAST_PARAMETERS_NOT_SUPPORTED,
            encoder_->InitEncode(&codec_settings_, kNumCores, kMaxPayloadSize));
  // Reslutions are not scaled by 2.
  codec_settings_.simulcastStream[0] = {kWidth, kHeight, kFramerateFps, 1,
                                        4000,   3000,    2000,          80};
  codec_settings_.simulcastStream[1] = {kWidth, kHeight, kFramerateFps, 1,
                                        4000,   3000,    2000,          80};
  codec_settings_.simulcastStream[2] = {kWidth, kHeight, kFramerateFps, 1,
                                        4000,   3000,    2000,          80};
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_ERR_SIMULCAST_PARAMETERS_NOT_SUPPORTED,
            encoder_->InitEncode(&codec_settings_, kNumCores, kMaxPayloadSize));
  // Temporal layers do not match.
  codec_settings_.simulcastStream[0] = {
      kWidth / 4, kHeight / 4, kFramerateFps, 1, 4000, 3000, 2000, 80};
  codec_settings_.simulcastStream[1] = {
      kWidth / 2, kHeight / 2, kFramerateFps, 2, 4000, 3000, 2000, 80};
  codec_settings_.simulcastStream[2] = {kWidth, kHeight, kFramerateFps, 3,
                                        4000,   3000,    2000,          80};
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_ERR_SIMULCAST_PARAMETERS_NOT_SUPPORTED,
            encoder_->InitEncode(&codec_settings_, kNumCores, kMaxPayloadSize));
  // Resolutions do not match codec config.
  codec_settings_.simulcastStream[0] = {
      kWidth / 4 + 1, kHeight / 4 + 1, kFramerateFps, 1, 4000, 3000, 2000, 80};
  codec_settings_.simulcastStream[1] = {
      kWidth / 2 + 2, kHeight / 2 + 2, kFramerateFps, 1, 4000, 3000, 2000, 80};
  codec_settings_.simulcastStream[2] = {
      kWidth + 4, kHeight + 4, kFramerateFps, 1, 4000, 3000, 2000, 80};
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_ERR_SIMULCAST_PARAMETERS_NOT_SUPPORTED,
            encoder_->InitEncode(&codec_settings_, kNumCores, kMaxPayloadSize));
  // Everything fine: scaling by 2, top resolution matches video, temporal
  // settings are the same for all layers.
  codec_settings_.simulcastStream[0] = {
      kWidth / 4, kHeight / 4, kFramerateFps, 1, 4000, 3000, 2000, 80};
  codec_settings_.simulcastStream[1] = {
      kWidth / 2, kHeight / 2, kFramerateFps, 1, 4000, 3000, 2000, 80};
  codec_settings_.simulcastStream[2] = {kWidth, kHeight, kFramerateFps, 1,
                                        4000,   3000,    2000,          80};
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, kNumCores, kMaxPayloadSize));
}

#if defined(WEBRTC_ANDROID)
#define MAYBE_AlignedStrideEncodeDecode DISABLED_AlignedStrideEncodeDecode
#else
#define MAYBE_AlignedStrideEncodeDecode AlignedStrideEncodeDecode
#endif
TEST_F(TestVp8Impl, MAYBE_AlignedStrideEncodeDecode) {
  VideoFrame* input_frame = NextInputFrame();
  input_frame->set_timestamp(kInitialTimestampRtp);
  input_frame->set_timestamp_us(kInitialTimestampMs *
                                rtc::kNumMicrosecsPerMillisec);
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  EncodeAndWaitForFrame(*input_frame, &encoded_frame, &codec_specific_info);

  // First frame should be a key frame.
  encoded_frame._frameType = kVideoFrameKey;
  encoded_frame.ntp_time_ms_ = kTestNtpTimeMs;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            decoder_->Decode(encoded_frame, false, nullptr, -1));

  std::unique_ptr<VideoFrame> decoded_frame;
  absl::optional<uint8_t> decoded_qp;
  ASSERT_TRUE(WaitForDecodedFrame(&decoded_frame, &decoded_qp));
  ASSERT_TRUE(decoded_frame);
  // Compute PSNR on all planes (faster than SSIM).
  EXPECT_GT(I420PSNR(input_frame, decoded_frame.get()), 36);
  EXPECT_EQ(kInitialTimestampRtp, decoded_frame->timestamp());
  EXPECT_EQ(kTestNtpTimeMs, decoded_frame->ntp_time_ms());
}

#if defined(WEBRTC_ANDROID)
#define MAYBE_DecodeWithACompleteKeyFrame DISABLED_DecodeWithACompleteKeyFrame
#else
#define MAYBE_DecodeWithACompleteKeyFrame DecodeWithACompleteKeyFrame
#endif
TEST_F(TestVp8Impl, MAYBE_DecodeWithACompleteKeyFrame) {
  VideoFrame* input_frame = NextInputFrame();
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  EncodeAndWaitForFrame(*input_frame, &encoded_frame, &codec_specific_info);

  // Setting complete to false -> should return an error.
  encoded_frame._completeFrame = false;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_ERROR,
            decoder_->Decode(encoded_frame, false, nullptr, -1));
  // Setting complete back to true. Forcing a delta frame.
  encoded_frame._frameType = kVideoFrameDelta;
  encoded_frame._completeFrame = true;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_ERROR,
            decoder_->Decode(encoded_frame, false, nullptr, -1));
  // Now setting a key frame.
  encoded_frame._frameType = kVideoFrameKey;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            decoder_->Decode(encoded_frame, false, nullptr, -1));
  std::unique_ptr<VideoFrame> decoded_frame;
  absl::optional<uint8_t> decoded_qp;
  ASSERT_TRUE(WaitForDecodedFrame(&decoded_frame, &decoded_qp));
  ASSERT_TRUE(decoded_frame);
  EXPECT_GT(I420PSNR(input_frame, decoded_frame.get()), 36);
}

TEST_F(TestVp8Impl, EncoderWith2TemporalLayers) {
  codec_settings_.VP8()->numberOfTemporalLayers = 2;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, kNumCores, kMaxPayloadSize));

  // Temporal layer 0.
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  EncodeAndWaitForFrame(*NextInputFrame(), &encoded_frame,
                        &codec_specific_info);

  EXPECT_EQ(0, codec_specific_info.codecSpecific.VP8.temporalIdx);
  // Temporal layer 1.
  EncodeAndExpectFrameWith(*NextInputFrame(), 1);
  // Temporal layer 0.
  EncodeAndExpectFrameWith(*NextInputFrame(), 0);
  // Temporal layer 1.
  EncodeAndExpectFrameWith(*NextInputFrame(), 1);
}

TEST_F(TestVp8Impl, ScalingDisabledIfAutomaticResizeOff) {
  codec_settings_.VP8()->frameDroppingOn = true;
  codec_settings_.VP8()->automaticResizeOn = false;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, kNumCores, kMaxPayloadSize));

  VideoEncoder::ScalingSettings settings = encoder_->GetScalingSettings();
  EXPECT_FALSE(settings.thresholds.has_value());
}

TEST_F(TestVp8Impl, ScalingEnabledIfAutomaticResizeOn) {
  codec_settings_.VP8()->frameDroppingOn = true;
  codec_settings_.VP8()->automaticResizeOn = true;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, kNumCores, kMaxPayloadSize));

  VideoEncoder::ScalingSettings settings = encoder_->GetScalingSettings();
  EXPECT_TRUE(settings.thresholds.has_value());
  EXPECT_EQ(kDefaultMinPixelsPerFrame, settings.min_pixels_per_frame);
}

TEST_F(TestVp8Impl, DontDropKeyframes) {
  // Set very high resolution to trigger overuse more easily.
  const int kScreenWidth = 1920;
  const int kScreenHeight = 1080;

  codec_settings_.width = kScreenWidth;
  codec_settings_.height = kScreenHeight;

  // Screensharing has the internal frame dropper off, and instead per frame
  // asks ScreenshareLayers to decide if it should be dropped or not.
  codec_settings_.VP8()->frameDroppingOn = false;
  codec_settings_.mode = VideoCodecMode::kScreensharing;
  // ScreenshareLayers triggers on 2 temporal layers and 1000kbps max bitrate.
  codec_settings_.VP8()->numberOfTemporalLayers = 2;
  codec_settings_.maxBitrate = 1000;

  // Reset the frame generator with large number of squares, leading to lots of
  // details and high probability of overshoot.
  input_frame_generator_ = test::FrameGenerator::CreateSquareGenerator(
      codec_settings_.width, codec_settings_.height,
      test::FrameGenerator::OutputType::I420,
      /* num_squares = */ absl::optional<int>(300));

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, kNumCores, kMaxPayloadSize));

  VideoBitrateAllocation bitrate_allocation;
  // Bitrate only enough for TL0.
  bitrate_allocation.SetBitrate(0, 0, 200000);
  encoder_->SetRateAllocation(bitrate_allocation, 5);

  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  EncodeAndWaitForFrame(*NextInputFrame(), &encoded_frame, &codec_specific_info,
                        true);
  EncodeAndExpectFrameWith(*NextInputFrame(), 0, true);
  EncodeAndExpectFrameWith(*NextInputFrame(), 0, true);
  EncodeAndExpectFrameWith(*NextInputFrame(), 0, true);
}

}  // namespace webrtc
