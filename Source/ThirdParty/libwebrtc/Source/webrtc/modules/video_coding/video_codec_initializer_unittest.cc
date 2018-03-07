/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/video_encoder.h"
#include "common_video/include/video_bitrate_allocator.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/video_coding/codecs/vp8/temporal_layers.h"
#include "modules/video_coding/include/video_codec_initializer.h"
#include "rtc_base/refcountedobject.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
static const char* kVp8PayloadName = "VP8";
static const int kVp8PayloadType = 100;
static const int kDefaultWidth = 1280;
static const int kDefaultHeight = 720;
static const int kDefaultFrameRate = 30;
static const uint32_t kDefaultMinBitrateBps = 60000;
static const uint32_t kDefaultTargetBitrateBps = 2000000;
static const uint32_t kDefaultMaxBitrateBps = 2000000;
static const uint32_t kDefaultMinTransmitBitrateBps = 400000;
static const int kDefaultMaxQp = 48;
static const uint32_t kScreenshareTl0BitrateBps = 100000;
static const uint32_t kScreenshareCodecTargetBitrateBps = 200000;
static const uint32_t kScreenshareDefaultFramerate = 5;
// Bitrates for the temporal layers of the higher screenshare simulcast stream.
static const uint32_t kHighScreenshareTl0Bps = 800000;
static const uint32_t kHighScreenshareTl1Bps = 1200000;
}  // namespace

/*
 *   static bool SetupCodec(
      const VideoEncoderConfig& config,
      const VideoSendStream::Config::EncoderSettings settings,
      const std::vector<VideoStream>& streams,
      bool nack_enabled,
      VideoCodec* codec,
      std::unique_ptr<VideoBitrateAllocator>* bitrate_allocator);

  // Create a bitrate allocator for the specified codec. |tl_factory| is
  // optional, if it is populated, ownership of that instance will be
  // transferred to the VideoBitrateAllocator instance.
  static std::unique_ptr<VideoBitrateAllocator> CreateBitrateAllocator(
      const VideoCodec& codec,
      std::unique_ptr<TemporalLayersFactory> tl_factory);
 */

// TODO(sprang): Extend coverage to handle the rest of the codec initializer.
class VideoCodecInitializerTest : public ::testing::Test {
 public:
  VideoCodecInitializerTest() : nack_enabled_(false) {}
  virtual ~VideoCodecInitializerTest() {}

 protected:
  void SetUpFor(VideoCodecType type,
                int num_spatial_streams,
                int num_temporal_streams,
                bool screenshare) {
    config_ = VideoEncoderConfig();
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
      settings_.payload_name = kVp8PayloadName;
      settings_.payload_type = kVp8PayloadType;
    } else {
      ADD_FAILURE() << "Unexpected codec type: " << type;
    }
  }

  bool InitializeCodec() {
    codec_out_ = VideoCodec();
    bitrate_allocator_out_.reset();
    temporal_layers_.clear();
    if (!VideoCodecInitializer::SetupCodec(config_, settings_, streams_,
                                           nack_enabled_, &codec_out_,
                                           &bitrate_allocator_out_)) {
      return false;
    }

    // Make sure temporal layers instances have been created.
    if (codec_out_.codecType == VideoCodecType::kVideoCodecVP8) {
      if (!codec_out_.VP8()->tl_factory)
        return false;

      for (int i = 0; i < codec_out_.numberOfSimulcastStreams; ++i) {
        temporal_layers_.emplace_back(codec_out_.VP8()->tl_factory->Create(
            i, streams_[i].temporal_layer_thresholds_bps.size() + 1, 0));
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
    return stream;
  }

  VideoStream DefaultScreenshareStream() {
    VideoStream stream = DefaultStream();
    stream.min_bitrate_bps = 30000;
    stream.target_bitrate_bps = kScreenshareTl0BitrateBps;
    stream.max_bitrate_bps = 1000000;
    stream.max_framerate = kScreenshareDefaultFramerate;
    stream.temporal_layer_thresholds_bps.push_back(kScreenshareTl0BitrateBps);
    return stream;
  }

  // Input settings.
  VideoEncoderConfig config_;
  VideoSendStream::Config::EncoderSettings settings_;
  std::vector<VideoStream> streams_;
  bool nack_enabled_;

  // Output.
  VideoCodec codec_out_;
  std::unique_ptr<VideoBitrateAllocator> bitrate_allocator_out_;
  std::vector<std::unique_ptr<TemporalLayers>> temporal_layers_;
};

TEST_F(VideoCodecInitializerTest, SingleStreamVp8Screenshare) {
  SetUpFor(VideoCodecType::kVideoCodecVP8, 1, 1, true);
  streams_.push_back(DefaultStream());
  EXPECT_TRUE(InitializeCodec());

  BitrateAllocation bitrate_allocation = bitrate_allocator_out_->GetAllocation(
      kDefaultTargetBitrateBps, kDefaultFrameRate);
  EXPECT_EQ(1u, codec_out_.numberOfSimulcastStreams);
  EXPECT_EQ(1u, codec_out_.VP8()->numberOfTemporalLayers);
  EXPECT_EQ(kDefaultTargetBitrateBps, bitrate_allocation.get_sum_bps());
}

TEST_F(VideoCodecInitializerTest, TemporalLayeredVp8Screenshare) {
  SetUpFor(VideoCodecType::kVideoCodecVP8, 1, 2, true);
  streams_.push_back(DefaultScreenshareStream());
  EXPECT_TRUE(InitializeCodec());

  EXPECT_EQ(1u, codec_out_.numberOfSimulcastStreams);
  EXPECT_EQ(2u, codec_out_.VP8()->numberOfTemporalLayers);
  BitrateAllocation bitrate_allocation = bitrate_allocator_out_->GetAllocation(
      kScreenshareCodecTargetBitrateBps, kScreenshareDefaultFramerate);
  EXPECT_EQ(kScreenshareCodecTargetBitrateBps,
            bitrate_allocation.get_sum_bps());
  EXPECT_EQ(kScreenshareTl0BitrateBps, bitrate_allocation.GetBitrate(0, 0));
}

TEST_F(VideoCodecInitializerTest, SimlucastVp8Screenshare) {
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
  BitrateAllocation bitrate_allocation = bitrate_allocator_out_->GetAllocation(
      max_bitrate_bps, kScreenshareDefaultFramerate);
  EXPECT_EQ(max_bitrate_bps, bitrate_allocation.get_sum_bps());
  EXPECT_EQ(static_cast<uint32_t>(streams_[0].target_bitrate_bps),
            bitrate_allocation.GetSpatialLayerSum(0));
  EXPECT_EQ(static_cast<uint32_t>(streams_[1].max_bitrate_bps),
            bitrate_allocation.GetSpatialLayerSum(1));
}

TEST_F(VideoCodecInitializerTest, HighFpsSimlucastVp8Screenshare) {
  // Two simulcast streams, the lower one using legacy settings (two temporal
  // streams, 5fps), the higher one using 3 temporal streams and 30fps.
  SetUpFor(VideoCodecType::kVideoCodecVP8, 2, 3, true);
  streams_.push_back(DefaultScreenshareStream());
  VideoStream video_stream = DefaultStream();
  video_stream.temporal_layer_thresholds_bps.push_back(kHighScreenshareTl0Bps);
  video_stream.temporal_layer_thresholds_bps.push_back(kHighScreenshareTl1Bps);
  streams_.push_back(video_stream);
  EXPECT_TRUE(InitializeCodec());

  EXPECT_EQ(2u, codec_out_.numberOfSimulcastStreams);
  EXPECT_EQ(3u, codec_out_.VP8()->numberOfTemporalLayers);
  const uint32_t max_bitrate_bps =
      streams_[0].target_bitrate_bps + streams_[1].max_bitrate_bps;
  BitrateAllocation bitrate_allocation =
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

}  // namespace webrtc
