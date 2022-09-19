/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/av1/libaom_av1_encoder.h"

#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/video_coding/codecs/test/encoded_video_frame_producer.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::SizeIs;

VideoCodec DefaultCodecSettings() {
  VideoCodec codec_settings;
  codec_settings.width = 320;
  codec_settings.height = 180;
  codec_settings.maxFramerate = 30;
  codec_settings.maxBitrate = 1000;
  codec_settings.qpMax = 63;
  return codec_settings;
}

VideoEncoder::Settings DefaultEncoderSettings() {
  return VideoEncoder::Settings(
      VideoEncoder::Capabilities(/*loss_notification=*/false),
      /*number_of_cores=*/1, /*max_payload_size=*/1200);
}

TEST(LibaomAv1EncoderTest, CanCreate) {
  std::unique_ptr<VideoEncoder> encoder = CreateLibaomAv1Encoder();
  EXPECT_TRUE(encoder);
}

TEST(LibaomAv1EncoderTest, InitAndRelease) {
  std::unique_ptr<VideoEncoder> encoder = CreateLibaomAv1Encoder();
  ASSERT_TRUE(encoder);
  VideoCodec codec_settings = DefaultCodecSettings();
  EXPECT_EQ(encoder->InitEncode(&codec_settings, DefaultEncoderSettings()),
            WEBRTC_VIDEO_CODEC_OK);
  EXPECT_EQ(encoder->Release(), WEBRTC_VIDEO_CODEC_OK);
}

TEST(LibaomAv1EncoderTest, NoBitrateOnTopLayerRefecltedInActiveDecodeTargets) {
  // Configure encoder with 2 temporal layers.
  std::unique_ptr<VideoEncoder> encoder = CreateLibaomAv1Encoder();
  VideoCodec codec_settings = DefaultCodecSettings();
  codec_settings.SetScalabilityMode(ScalabilityMode::kL1T2);
  ASSERT_EQ(encoder->InitEncode(&codec_settings, DefaultEncoderSettings()),
            WEBRTC_VIDEO_CODEC_OK);

  VideoEncoder::RateControlParameters rate_parameters;
  rate_parameters.framerate_fps = 30;
  rate_parameters.bitrate.SetBitrate(0, /*temporal_index=*/0, 300'000);
  rate_parameters.bitrate.SetBitrate(0, /*temporal_index=*/1, 0);
  encoder->SetRates(rate_parameters);

  std::vector<EncodedVideoFrameProducer::EncodedFrame> encoded_frames =
      EncodedVideoFrameProducer(*encoder).SetNumInputFrames(1).Encode();
  ASSERT_THAT(encoded_frames, SizeIs(1));
  ASSERT_NE(encoded_frames[0].codec_specific_info.generic_frame_info,
            absl::nullopt);
  // Assuming L1T2 structure uses 1st decode target for T0 and 2nd decode target
  // for T0+T1 frames, expect only 1st decode target is active.
  EXPECT_EQ(encoded_frames[0]
                .codec_specific_info.generic_frame_info->active_decode_targets,
            0b01);
}

TEST(LibaomAv1EncoderTest,
     SpatialScalabilityInTemporalUnitReportedAsDeltaFrame) {
  std::unique_ptr<VideoEncoder> encoder = CreateLibaomAv1Encoder();
  VideoCodec codec_settings = DefaultCodecSettings();
  codec_settings.SetScalabilityMode(ScalabilityMode::kL2T1);
  ASSERT_EQ(encoder->InitEncode(&codec_settings, DefaultEncoderSettings()),
            WEBRTC_VIDEO_CODEC_OK);

  VideoEncoder::RateControlParameters rate_parameters;
  rate_parameters.framerate_fps = 30;
  rate_parameters.bitrate.SetBitrate(/*spatial_index=*/0, 0, 300'000);
  rate_parameters.bitrate.SetBitrate(/*spatial_index=*/1, 0, 300'000);
  encoder->SetRates(rate_parameters);

  std::vector<EncodedVideoFrameProducer::EncodedFrame> encoded_frames =
      EncodedVideoFrameProducer(*encoder).SetNumInputFrames(1).Encode();
  ASSERT_THAT(encoded_frames, SizeIs(2));
  EXPECT_THAT(encoded_frames[0].encoded_image._frameType,
              Eq(VideoFrameType::kVideoFrameKey));
  EXPECT_THAT(encoded_frames[1].encoded_image._frameType,
              Eq(VideoFrameType::kVideoFrameDelta));
}

TEST(LibaomAv1EncoderTest, NoBitrateOnTopSpatialLayerProduceDeltaFrames) {
  std::unique_ptr<VideoEncoder> encoder = CreateLibaomAv1Encoder();
  VideoCodec codec_settings = DefaultCodecSettings();
  codec_settings.SetScalabilityMode(ScalabilityMode::kL2T1);
  ASSERT_EQ(encoder->InitEncode(&codec_settings, DefaultEncoderSettings()),
            WEBRTC_VIDEO_CODEC_OK);

  VideoEncoder::RateControlParameters rate_parameters;
  rate_parameters.framerate_fps = 30;
  rate_parameters.bitrate.SetBitrate(/*spatial_index=*/0, 0, 300'000);
  rate_parameters.bitrate.SetBitrate(/*spatial_index=*/1, 0, 0);
  encoder->SetRates(rate_parameters);

  std::vector<EncodedVideoFrameProducer::EncodedFrame> encoded_frames =
      EncodedVideoFrameProducer(*encoder).SetNumInputFrames(2).Encode();
  ASSERT_THAT(encoded_frames, SizeIs(2));
  EXPECT_THAT(encoded_frames[0].encoded_image._frameType,
              Eq(VideoFrameType::kVideoFrameKey));
  EXPECT_THAT(encoded_frames[1].encoded_image._frameType,
              Eq(VideoFrameType::kVideoFrameDelta));
}

TEST(LibaomAv1EncoderTest, SetsEndOfPictureForLastFrameInTemporalUnit) {
  VideoBitrateAllocation allocation;
  allocation.SetBitrate(0, 0, 30000);
  allocation.SetBitrate(1, 0, 40000);
  allocation.SetBitrate(2, 0, 30000);

  std::unique_ptr<VideoEncoder> encoder = CreateLibaomAv1Encoder();
  VideoCodec codec_settings = DefaultCodecSettings();
  // Configure encoder with 3 spatial layers.
  codec_settings.SetScalabilityMode(ScalabilityMode::kL3T1);
  codec_settings.maxBitrate = allocation.get_sum_kbps();
  ASSERT_EQ(encoder->InitEncode(&codec_settings, DefaultEncoderSettings()),
            WEBRTC_VIDEO_CODEC_OK);

  encoder->SetRates(VideoEncoder::RateControlParameters(
      allocation, codec_settings.maxFramerate));

  std::vector<EncodedVideoFrameProducer::EncodedFrame> encoded_frames =
      EncodedVideoFrameProducer(*encoder).SetNumInputFrames(2).Encode();
  ASSERT_THAT(encoded_frames, SizeIs(6));
  EXPECT_FALSE(encoded_frames[0].codec_specific_info.end_of_picture);
  EXPECT_FALSE(encoded_frames[1].codec_specific_info.end_of_picture);
  EXPECT_TRUE(encoded_frames[2].codec_specific_info.end_of_picture);
  EXPECT_FALSE(encoded_frames[3].codec_specific_info.end_of_picture);
  EXPECT_FALSE(encoded_frames[4].codec_specific_info.end_of_picture);
  EXPECT_TRUE(encoded_frames[5].codec_specific_info.end_of_picture);
}

TEST(LibaomAv1EncoderTest, CheckOddDimensionsWithSpatialLayers) {
  VideoBitrateAllocation allocation;
  allocation.SetBitrate(0, 0, 30000);
  allocation.SetBitrate(1, 0, 40000);
  allocation.SetBitrate(2, 0, 30000);
  std::unique_ptr<VideoEncoder> encoder = CreateLibaomAv1Encoder();
  VideoCodec codec_settings = DefaultCodecSettings();
  // Configure encoder with 3 spatial layers.
  codec_settings.SetScalabilityMode(ScalabilityMode::kL3T1);
  // Odd width and height values should not make encoder crash.
  codec_settings.width = 623;
  codec_settings.height = 405;
  codec_settings.maxBitrate = allocation.get_sum_kbps();
  ASSERT_EQ(encoder->InitEncode(&codec_settings, DefaultEncoderSettings()),
            WEBRTC_VIDEO_CODEC_OK);
  encoder->SetRates(VideoEncoder::RateControlParameters(
      allocation, codec_settings.maxFramerate));
  EncodedVideoFrameProducer evfp(*encoder);
  evfp.SetResolution(RenderResolution{623, 405});
  std::vector<EncodedVideoFrameProducer::EncodedFrame> encoded_frames =
      evfp.SetNumInputFrames(2).Encode();
  ASSERT_THAT(encoded_frames, SizeIs(6));
}

TEST(LibaomAv1EncoderTest, EncoderInfoProvidesFpsAllocation) {
  std::unique_ptr<VideoEncoder> encoder = CreateLibaomAv1Encoder();
  VideoCodec codec_settings = DefaultCodecSettings();
  codec_settings.SetScalabilityMode(ScalabilityMode::kL3T3);
  codec_settings.maxFramerate = 60;
  ASSERT_EQ(encoder->InitEncode(&codec_settings, DefaultEncoderSettings()),
            WEBRTC_VIDEO_CODEC_OK);

  const auto& encoder_info = encoder->GetEncoderInfo();
  EXPECT_THAT(encoder_info.fps_allocation[0], ElementsAre(15, 30, 60));
  EXPECT_THAT(encoder_info.fps_allocation[1], ElementsAre(15, 30, 60));
  EXPECT_THAT(encoder_info.fps_allocation[2], ElementsAre(15, 30, 60));
  EXPECT_THAT(encoder_info.fps_allocation[3], IsEmpty());
}

TEST(LibaomAv1EncoderTest, PopulatesEncodedFrameSize) {
  VideoBitrateAllocation allocation;
  allocation.SetBitrate(0, 0, 30000);
  allocation.SetBitrate(1, 0, 40000);
  allocation.SetBitrate(2, 0, 30000);
  std::unique_ptr<VideoEncoder> encoder = CreateLibaomAv1Encoder();
  VideoCodec codec_settings = DefaultCodecSettings();
  codec_settings.maxBitrate = allocation.get_sum_kbps();
  ASSERT_GT(codec_settings.width, 4);
  // Configure encoder with 3 spatial layers.
  codec_settings.SetScalabilityMode(ScalabilityMode::kL3T1);
  ASSERT_EQ(encoder->InitEncode(&codec_settings, DefaultEncoderSettings()),
            WEBRTC_VIDEO_CODEC_OK);
  encoder->SetRates(VideoEncoder::RateControlParameters(
      allocation, codec_settings.maxFramerate));
  using Frame = EncodedVideoFrameProducer::EncodedFrame;
  std::vector<Frame> encoded_frames =
      EncodedVideoFrameProducer(*encoder).SetNumInputFrames(1).Encode();
  EXPECT_THAT(
      encoded_frames,
      ElementsAre(
          Field(&Frame::encoded_image,
                AllOf(Field(&EncodedImage::_encodedWidth,
                            codec_settings.width / 4),
                      Field(&EncodedImage::_encodedHeight,
                            codec_settings.height / 4))),
          Field(&Frame::encoded_image,
                AllOf(Field(&EncodedImage::_encodedWidth,
                            codec_settings.width / 2),
                      Field(&EncodedImage::_encodedHeight,
                            codec_settings.height / 2))),
          Field(&Frame::encoded_image,
                AllOf(Field(&EncodedImage::_encodedWidth, codec_settings.width),
                      Field(&EncodedImage::_encodedHeight,
                            codec_settings.height)))));
}

}  // namespace
}  // namespace webrtc
