/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/include/video_codec_initializer.h"
#include "api/video/video_bitrate_allocator.h"
#include "api/video_codecs/video_encoder.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/video_coding/codecs/vp8/include/vp8_temporal_layers.h"
#include "modules/video_coding/codecs/vp9/include/vp9_globals.h"
#include "rtc_base/refcountedobject.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
static const int kDefaultWidth = 1280;
static const int kDefaultHeight = 720;
static const int kDefaultFrameRate = 30;
static const uint32_t kDefaultMinBitrateBps = 60000;
static const uint32_t kDefaultTargetBitrateBps = 2000000;
static const uint32_t kDefaultMaxBitrateBps = 2000000;
static const uint32_t kDefaultMinTransmitBitrateBps = 400000;
static const int kDefaultMaxQp = 48;
static const uint32_t kScreenshareTl0BitrateBps = 200000;
static const uint32_t kScreenshareCodecTargetBitrateBps = 200000;
static const uint32_t kScreenshareDefaultFramerate = 5;
// Bitrates for the temporal layers of the higher screenshare simulcast stream.
static const uint32_t kHighScreenshareTl0Bps = 800000;
static const uint32_t kHighScreenshareTl1Bps = 1200000;
}  // namespace

// TODO(sprang): Extend coverage to handle the rest of the codec initializer.
class VideoCodecInitializerTest : public ::testing::Test {
 public:
  VideoCodecInitializerTest() {}
  virtual ~VideoCodecInitializerTest() {}

 protected:
  void SetUpFor(VideoCodecType type,
                int num_spatial_streams,
                int num_temporal_streams,
                bool screenshare) {
    config_ = VideoEncoderConfig();
    config_.codec_type = type;

    if (screenshare) {
      config_.min_transmit_bitrate_bps = kDefaultMinTransmitBitrateBps;
      config_.content_type = VideoEncoderConfig::ContentType::kScreen;
    }

    if (type == VideoCodecType::kVideoCodecVP8) {
      config_.number_of_streams = num_spatial_streams;
      VideoCodecVP8 vp8_settings = VideoEncoder::GetDefaultVp8Settings();
      vp8_settings.numberOfTemporalLayers = num_temporal_streams;
      config_.encoder_specific_settings = new rtc::RefCountedObject<
          webrtc::VideoEncoderConfig::Vp8EncoderSpecificSettings>(vp8_settings);
    } else if (type == VideoCodecType::kVideoCodecVP9) {
      VideoCodecVP9 vp9_settings = VideoEncoder::GetDefaultVp9Settings();
      vp9_settings.numberOfSpatialLayers = num_spatial_streams;
      vp9_settings.numberOfTemporalLayers = num_temporal_streams;
      config_.encoder_specific_settings = new rtc::RefCountedObject<
          webrtc::VideoEncoderConfig::Vp9EncoderSpecificSettings>(vp9_settings);
    } else if (type != VideoCodecType::kVideoCodecMultiplex) {
      ADD_FAILURE() << "Unexpected codec type: " << type;
    }
  }

  bool InitializeCodec() {
    codec_out_ = VideoCodec();
    bitrate_allocator_out_.reset();
    temporal_layers_.clear();
    if (!VideoCodecInitializer::SetupCodec(config_, streams_, &codec_out_,
                                           &bitrate_allocator_out_)) {
      return false;
    }
    if (codec_out_.codecType == VideoCodecType::kVideoCodecMultiplex)
      return true;

    // Make sure temporal layers instances have been created.
    if (codec_out_.codecType == VideoCodecType::kVideoCodecVP8) {
      for (int i = 0; i < codec_out_.numberOfSimulcastStreams; ++i) {
        temporal_layers_.emplace_back(TemporalLayers::CreateTemporalLayers(
            TemporalLayersType::kFixedPattern,
            codec_out_.VP8()->numberOfTemporalLayers));
      }
    }
    return true;
  }

  VideoStream DefaultStream() {
    VideoStream stream;
    stream.width = kDefaultWidth;
    stream.height = kDefaultHeight;
    stream.max_framerate = kDefaultFrameRate;
    stream.min_bitrate_bps = kDefaultMinBitrateBps;
    stream.target_bitrate_bps = kDefaultTargetBitrateBps;
    stream.max_bitrate_bps = kDefaultMaxBitrateBps;
    stream.max_qp = kDefaultMaxQp;
    stream.num_temporal_layers = 1;
    stream.active = true;
    return stream;
  }

  VideoStream DefaultScreenshareStream() {
    VideoStream stream = DefaultStream();
    stream.min_bitrate_bps = 30000;
    stream.target_bitrate_bps = kScreenshareTl0BitrateBps;
    stream.max_bitrate_bps = 1000000;
    stream.max_framerate = kScreenshareDefaultFramerate;
    stream.num_temporal_layers = 2;
    stream.active = true;
    return stream;
  }

  // Input settings.
  VideoEncoderConfig config_;
  std::vector<VideoStream> streams_;

  // Output.
  VideoCodec codec_out_;
  std::unique_ptr<VideoBitrateAllocator> bitrate_allocator_out_;
  std::vector<std::unique_ptr<TemporalLayers>> temporal_layers_;
};

TEST_F(VideoCodecInitializerTest, SingleStreamVp8Screenshare) {
  SetUpFor(VideoCodecType::kVideoCodecVP8, 1, 1, true);
  streams_.push_back(DefaultStream());
  EXPECT_TRUE(InitializeCodec());

  VideoBitrateAllocation bitrate_allocation =
      bitrate_allocator_out_->GetAllocation(kDefaultTargetBitrateBps,
                                            kDefaultFrameRate);
  EXPECT_EQ(1u, codec_out_.numberOfSimulcastStreams);
  EXPECT_EQ(1u, codec_out_.VP8()->numberOfTemporalLayers);
  EXPECT_EQ(kDefaultTargetBitrateBps, bitrate_allocation.get_sum_bps());
}

TEST_F(VideoCodecInitializerTest, SingleStreamVp8ScreenshareInactive) {
  SetUpFor(VideoCodecType::kVideoCodecVP8, 1, 1, true);
  VideoStream inactive_stream = DefaultStream();
  inactive_stream.active = false;
  streams_.push_back(inactive_stream);
  EXPECT_TRUE(InitializeCodec());

  VideoBitrateAllocation bitrate_allocation =
      bitrate_allocator_out_->GetAllocation(kDefaultTargetBitrateBps,
                                            kDefaultFrameRate);
  EXPECT_EQ(1u, codec_out_.numberOfSimulcastStreams);
  EXPECT_EQ(1u, codec_out_.VP8()->numberOfTemporalLayers);
  EXPECT_EQ(0U, bitrate_allocation.get_sum_bps());
}

TEST_F(VideoCodecInitializerTest, TemporalLayeredVp8Screenshare) {
  SetUpFor(VideoCodecType::kVideoCodecVP8, 1, 2, true);
  streams_.push_back(DefaultScreenshareStream());
  EXPECT_TRUE(InitializeCodec());

  EXPECT_EQ(1u, codec_out_.numberOfSimulcastStreams);
  EXPECT_EQ(2u, codec_out_.VP8()->numberOfTemporalLayers);
  VideoBitrateAllocation bitrate_allocation =
      bitrate_allocator_out_->GetAllocation(kScreenshareCodecTargetBitrateBps,
                                            kScreenshareDefaultFramerate);
  EXPECT_EQ(kScreenshareCodecTargetBitrateBps,
            bitrate_allocation.get_sum_bps());
  EXPECT_EQ(kScreenshareTl0BitrateBps, bitrate_allocation.GetBitrate(0, 0));
}

TEST_F(VideoCodecInitializerTest, SimulcastVp8Screenshare) {
  SetUpFor(VideoCodecType::kVideoCodecVP8, 2, 1, true);
  streams_.push_back(DefaultScreenshareStream());
  VideoStream video_stream = DefaultStream();
  video_stream.max_framerate = kScreenshareDefaultFramerate;
  streams_.push_back(video_stream);
  EXPECT_TRUE(InitializeCodec());

  EXPECT_EQ(2u, codec_out_.numberOfSimulcastStreams);
  EXPECT_EQ(1u, codec_out_.VP8()->numberOfTemporalLayers);
  const uint32_t max_bitrate_bps =
      streams_[0].target_bitrate_bps + streams_[1].max_bitrate_bps;
  VideoBitrateAllocation bitrate_allocation =
      bitrate_allocator_out_->GetAllocation(max_bitrate_bps,
                                            kScreenshareDefaultFramerate);
  EXPECT_EQ(max_bitrate_bps, bitrate_allocation.get_sum_bps());
  EXPECT_EQ(static_cast<uint32_t>(streams_[0].target_bitrate_bps),
            bitrate_allocation.GetSpatialLayerSum(0));
  EXPECT_EQ(static_cast<uint32_t>(streams_[1].max_bitrate_bps),
            bitrate_allocation.GetSpatialLayerSum(1));
}

// Tests that when a video stream is inactive, then the bitrate allocation will
// be 0 for that stream.
TEST_F(VideoCodecInitializerTest, SimulcastVp8ScreenshareInactive) {
  SetUpFor(VideoCodecType::kVideoCodecVP8, 2, 1, true);
  streams_.push_back(DefaultScreenshareStream());
  VideoStream inactive_video_stream = DefaultStream();
  inactive_video_stream.active = false;
  inactive_video_stream.max_framerate = kScreenshareDefaultFramerate;
  streams_.push_back(inactive_video_stream);
  EXPECT_TRUE(InitializeCodec());

  EXPECT_EQ(2u, codec_out_.numberOfSimulcastStreams);
  EXPECT_EQ(1u, codec_out_.VP8()->numberOfTemporalLayers);
  const uint32_t target_bitrate =
      streams_[0].target_bitrate_bps + streams_[1].target_bitrate_bps;
  VideoBitrateAllocation bitrate_allocation =
      bitrate_allocator_out_->GetAllocation(target_bitrate,
                                            kScreenshareDefaultFramerate);
  EXPECT_EQ(static_cast<uint32_t>(streams_[0].max_bitrate_bps),
            bitrate_allocation.get_sum_bps());
  EXPECT_EQ(static_cast<uint32_t>(streams_[0].max_bitrate_bps),
            bitrate_allocation.GetSpatialLayerSum(0));
  EXPECT_EQ(0U, bitrate_allocation.GetSpatialLayerSum(1));
}

TEST_F(VideoCodecInitializerTest, HighFpsSimulcastVp8Screenshare) {
  // Two simulcast streams, the lower one using legacy settings (two temporal
  // streams, 5fps), the higher one using 3 temporal streams and 30fps.
  SetUpFor(VideoCodecType::kVideoCodecVP8, 2, 3, true);
  streams_.push_back(DefaultScreenshareStream());
  VideoStream video_stream = DefaultStream();
  video_stream.num_temporal_layers = 3;
  streams_.push_back(video_stream);
  EXPECT_TRUE(InitializeCodec());

  EXPECT_EQ(2u, codec_out_.numberOfSimulcastStreams);
  EXPECT_EQ(3u, codec_out_.VP8()->numberOfTemporalLayers);
  const uint32_t max_bitrate_bps =
      streams_[0].target_bitrate_bps + streams_[1].max_bitrate_bps;
  VideoBitrateAllocation bitrate_allocation =
      bitrate_allocator_out_->GetAllocation(max_bitrate_bps, kDefaultFrameRate);
  EXPECT_EQ(max_bitrate_bps, bitrate_allocation.get_sum_bps());
  EXPECT_EQ(static_cast<uint32_t>(streams_[0].target_bitrate_bps),
            bitrate_allocation.GetSpatialLayerSum(0));
  EXPECT_EQ(static_cast<uint32_t>(streams_[1].max_bitrate_bps),
            bitrate_allocation.GetSpatialLayerSum(1));
  EXPECT_EQ(kHighScreenshareTl0Bps, bitrate_allocation.GetBitrate(1, 0));
  EXPECT_EQ(kHighScreenshareTl1Bps - kHighScreenshareTl0Bps,
            bitrate_allocation.GetBitrate(1, 1));
}

TEST_F(VideoCodecInitializerTest, SingleStreamMultiplexCodec) {
  SetUpFor(VideoCodecType::kVideoCodecMultiplex, 1, 1, true);
  streams_.push_back(DefaultStream());
  EXPECT_TRUE(InitializeCodec());
}

TEST_F(VideoCodecInitializerTest, Vp9SvcDefaultLayering) {
  SetUpFor(VideoCodecType::kVideoCodecVP9, 3, 3, false);
  VideoStream stream = DefaultStream();
  stream.num_temporal_layers = 3;
  streams_.push_back(stream);

  EXPECT_TRUE(InitializeCodec());
  EXPECT_EQ(codec_out_.VP9()->numberOfSpatialLayers, 3u);
  EXPECT_EQ(codec_out_.VP9()->numberOfTemporalLayers, 3u);
}

TEST_F(VideoCodecInitializerTest, Vp9SvcAdjustedLayering) {
  SetUpFor(VideoCodecType::kVideoCodecVP9, 3, 3, false);
  VideoStream stream = DefaultStream();
  stream.num_temporal_layers = 3;
  // Set resolution which is only enough to produce 2 spatial layers.
  stream.width = kMinVp9SpatialLayerWidth * 2;
  stream.height = kMinVp9SpatialLayerHeight * 2;

  streams_.push_back(stream);

  EXPECT_TRUE(InitializeCodec());
  EXPECT_EQ(codec_out_.VP9()->numberOfSpatialLayers, 2u);
}

TEST_F(VideoCodecInitializerTest,
       Vp9SingleSpatialLayerMaxBitrateIsEqualToCodecMaxBitrate) {
  SetUpFor(VideoCodecType::kVideoCodecVP9, 1, 3, false);
  VideoStream stream = DefaultStream();
  stream.num_temporal_layers = 3;
  streams_.push_back(stream);

  EXPECT_TRUE(InitializeCodec());
  EXPECT_EQ(codec_out_.spatialLayers[0].maxBitrate,
            kDefaultMaxBitrateBps / 1000);
}

}  // namespace webrtc
