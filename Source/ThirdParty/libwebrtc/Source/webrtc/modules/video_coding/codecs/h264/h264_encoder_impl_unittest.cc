/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#include "modules/video_coding/codecs/h264/h264_encoder_impl.h"

#include <cstdint>
#include <optional>

#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "api/test/create_frame_generator.h"
#include "api/test/frame_generator_interface.h"
#include "api/test/mock_video_encoder.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_buffer.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "test/create_test_environment.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Return;

namespace webrtc {

namespace {

constexpr int kMaxPayloadSize = 1024;
constexpr int kNumCores = 1;

const VideoEncoder::Capabilities kCapabilities(false);
const VideoEncoder::Settings kSettings(kCapabilities,
                                       kNumCores,
                                       kMaxPayloadSize);

void SetDefaultSettings(VideoCodec* codec_settings) {
  codec_settings->codecType = kVideoCodecH264;
  codec_settings->maxFramerate = 60;
  codec_settings->width = 640;
  codec_settings->height = 480;
  // If frame dropping is false, we get a warning that bitrate can't
  // be controlled for RC_QUALITY_MODE; RC_BITRATE_MODE and RC_TIMESTAMP_MODE
  codec_settings->SetFrameDropEnabled(true);
  codec_settings->startBitrate = 2000;
  codec_settings->maxBitrate = 4000;
}

TEST(H264EncoderImplTest, CanInitializeWithDefaultParameters) {
  H264EncoderImpl encoder(CreateTestEnvironment(), {});
  VideoCodec codec_settings;
  SetDefaultSettings(&codec_settings);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder.InitEncode(&codec_settings, kSettings));
  EXPECT_EQ(H264PacketizationMode::NonInterleaved,
            encoder.PacketizationModeForTesting());
}

TEST(H264EncoderImplTest, CanInitializeWithNonInterleavedModeExplicitly) {
  H264EncoderImpl encoder(
      CreateTestEnvironment(),
      {.packetization_mode = H264PacketizationMode::NonInterleaved});
  VideoCodec codec_settings;
  SetDefaultSettings(&codec_settings);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder.InitEncode(&codec_settings, kSettings));
  EXPECT_EQ(H264PacketizationMode::NonInterleaved,
            encoder.PacketizationModeForTesting());
}

TEST(H264EncoderImplTest, CanInitializeWithSingleNalUnitModeExplicitly) {
  H264EncoderImpl encoder(
      CreateTestEnvironment(),
      {.packetization_mode = H264PacketizationMode::SingleNalUnit});
  VideoCodec codec_settings;
  SetDefaultSettings(&codec_settings);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder.InitEncode(&codec_settings, kSettings));
  EXPECT_EQ(H264PacketizationMode::SingleNalUnit,
            encoder.PacketizationModeForTesting());
}

TEST(H264EncoderImplTest, OnFrameDropped) {
  H264EncoderImpl encoder(CreateTestEnvironment(), {});
  VideoCodec codec_settings;
  SetDefaultSettings(&codec_settings);
  // Set a very low bitrate to force frame drops.
  codec_settings.startBitrate = 1;
  codec_settings.maxBitrate = 1;

  MockEncodedImageCallback callback;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder.InitEncode(&codec_settings, kSettings));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder.RegisterEncodeCompleteCallback(&callback));

  auto frame_generator = test::CreateSquareFrameGenerator(
      codec_settings.width, codec_settings.height,
      test::FrameGeneratorInterface::OutputType::kI420,
      /*num_squares=*/std::nullopt);

  // We need to encode enough frames to trigger rate control dropping.
  // The exact number might vary, but a loop should catch it.
  const int kNumFramesToEncode = 30;

  EXPECT_CALL(callback, OnEncodedImage)
      .Times(AnyNumber())
      .WillRepeatedly(Return(
          EncodedImageCallback::Result(EncodedImageCallback::Result::OK)));

  EXPECT_CALL(callback, OnFrameDropped)
      .Times(AtLeast(1))
      .WillRepeatedly([&](uint32_t rtp_timestamp, int spatial_id,
                          bool is_end_of_temporal_unit) {
        // Verify arguments match what we expect for a single layer drop.
        EXPECT_EQ(spatial_id, 0);  // H264 encoder usually uses simlucast index
                                   // as spatial_id, or just 0 for single layer.
        EXPECT_TRUE(is_end_of_temporal_unit);
      });

  for (int i = 0; i < kNumFramesToEncode; ++i) {
    VideoFrame frame =
        VideoFrame::Builder()
            .set_video_frame_buffer(frame_generator->NextFrame().buffer)
            .set_rtp_timestamp(i * 90000 / codec_settings.maxFramerate)
            .build();
    encoder.Encode(frame, nullptr);
  }
}

TEST(H264EncoderImplTest, RejectsI420FramesWithUnequalChromaStrides) {
  H264EncoderImpl encoder(CreateTestEnvironment(), {});
  VideoCodec codec_settings;
  SetDefaultSettings(&codec_settings);
  MockEncodedImageCallback callback;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder.InitEncode(&codec_settings, kSettings));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder.RegisterEncodeCompleteCallback(&callback));
  // Create a VideoFrame where the U and V strides are different.
  auto buffer = I420Buffer::Create(
      /*width=*/codec_settings.width,
      /*height=*/codec_settings.height,
      /*stride_y=*/codec_settings.width,
      /*stride_u=*/(codec_settings.width + 1) / 2,
      /*stride_v=*/(codec_settings.width + 1) / 2 + 1);

  VideoFrame frame = VideoFrame::Builder()
                         .set_video_frame_buffer(buffer)
                         .set_rtp_timestamp(0)
                         .build();

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_ENCODER_FAILURE, encoder.Encode(frame, nullptr));
}

TEST(H264EncoderImplTest, RejectsNativeFramesWithUnequalChromaStrides) {
  H264EncoderImpl encoder(CreateTestEnvironment(), {});
  VideoCodec codec_settings;
  SetDefaultSettings(&codec_settings);
  MockEncodedImageCallback callback;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder.InitEncode(&codec_settings, kSettings));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder.RegisterEncodeCompleteCallback(&callback));

  class FakeNativeBuffer : public VideoFrameBuffer {
   public:
    FakeNativeBuffer(int width, int height) : width_(width), height_(height) {}
    Type type() const override { return Type::kNative; }
    int width() const override { return width_; }
    int height() const override { return height_; }
    scoped_refptr<I420BufferInterface> ToI420() override {
      return I420Buffer::Create(width_, height_, width_, (width_ + 1) / 2,
                                (width_ + 1) / 2 + 1);
    }

   private:
    int width_;
    int height_;
  };

  auto buffer = make_ref_counted<FakeNativeBuffer>(codec_settings.width,
                                                   codec_settings.height);

  VideoFrame frame = VideoFrame::Builder()
                         .set_video_frame_buffer(buffer)
                         .set_rtp_timestamp(0)
                         .build();

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_ENCODER_FAILURE, encoder.Encode(frame, nullptr));
}

}  // anonymous namespace

}  // namespace webrtc
