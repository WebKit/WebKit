/*
 *  Copyright 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "api/environment/environment.h"
#include "api/make_ref_counted.h"
#include "api/units/data_rate.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "call/video_receive_stream.h"
#include "call/video_send_stream.h"
#include "modules/video_coding/svc/scalability_mode_util.h"
#include "rtc_base/experiments/encoder_info_settings.h"
#include "test/call_test.h"
#include "test/fake_encoder.h"
#include "test/frame_generator_capturer.h"
#include "test/gtest.h"
#include "test/video_encoder_proxy_factory.h"
#include "test/video_test_constants.h"
#include "video/config/video_encoder_config.h"

namespace webrtc {
namespace test {
namespace {
void SetEncoderSpecific(VideoEncoderConfig* encoder_config,
                        VideoCodecType type,
                        size_t num_spatial_layers) {
  if (type == kVideoCodecVP9) {
    VideoCodecVP9 vp9 = VideoEncoder::GetDefaultVp9Settings();
    vp9.numberOfSpatialLayers = num_spatial_layers;
    encoder_config->encoder_specific_settings =
        make_ref_counted<VideoEncoderConfig::Vp9EncoderSpecificSettings>(vp9);
  }
}

struct BitrateLimits {
  DataRate min = DataRate::Zero();
  DataRate max = DataRate::Zero();
};

BitrateLimits GetLayerBitrateLimits(int pixels, const VideoCodec& codec) {
  if (codec.codecType == VideoCodecType::kVideoCodecAV1) {
    EXPECT_TRUE(codec.GetScalabilityMode().has_value());
    for (int i = 0;
         i < ScalabilityModeToNumSpatialLayers(*(codec.GetScalabilityMode()));
         ++i) {
      if (codec.spatialLayers[i].width * codec.spatialLayers[i].height ==
          pixels) {
        return {
            .min = DataRate::KilobitsPerSec(codec.spatialLayers[i].minBitrate),
            .max = DataRate::KilobitsPerSec(codec.spatialLayers[i].maxBitrate)};
      }
    }
  } else if (codec.codecType == VideoCodecType::kVideoCodecVP9) {
    for (size_t i = 0; i < codec.VP9().numberOfSpatialLayers; ++i) {
      if (codec.spatialLayers[i].width * codec.spatialLayers[i].height ==
          pixels) {
        return {
            .min = DataRate::KilobitsPerSec(codec.spatialLayers[i].minBitrate),
            .max = DataRate::KilobitsPerSec(codec.spatialLayers[i].maxBitrate)};
      }
    }
  } else {
    for (int i = 0; i < codec.numberOfSimulcastStreams; ++i) {
      if (codec.simulcastStream[i].width * codec.simulcastStream[i].height ==
          pixels) {
        return {
            .min =
                DataRate::KilobitsPerSec(codec.simulcastStream[i].minBitrate),
            .max =
                DataRate::KilobitsPerSec(codec.simulcastStream[i].maxBitrate)};
      }
    }
  }
  ADD_FAILURE();
  return BitrateLimits();
}

bool SupportsSpatialLayers(const std::string& payload_name) {
  return payload_name == "VP9" || payload_name == "AV1";
}

}  // namespace

class ResolutionBitrateLimitsWithScalabilityModeTest : public test::CallTest {};

class ResolutionBitrateLimitsTest
    : public test::CallTest,
      public ::testing::WithParamInterface<std::string> {
 public:
  ResolutionBitrateLimitsTest() : payload_name_(GetParam()) {}

  const std::string payload_name_;
};

INSTANTIATE_TEST_SUITE_P(PayloadName,
                         ResolutionBitrateLimitsTest,
                         ::testing::Values("VP8", "VP9"),
                         [](const ::testing::TestParamInfo<std::string>& info) {
                           return info.param;
                         });

class InitEncodeTest : public test::EndToEndTest,
                       public test::FrameGeneratorCapturer::SinkWantsObserver,
                       public test::FakeEncoder {
 public:
  struct Bitrate {
    const std::optional<DataRate> min;
    const std::optional<DataRate> max;
  };
  struct TestConfig {
    const bool active;
    const Bitrate bitrate;
    const std::optional<ScalabilityMode> scalability_mode;
  };
  struct Expectation {
    const uint32_t pixels = 0;
    const Bitrate eq_bitrate;
    const Bitrate ne_bitrate;
  };

  InitEncodeTest(const Environment& env,
                 const std::string& payload_name,
                 const std::vector<TestConfig>& configs,
                 const std::vector<Expectation>& expectations)
      : EndToEndTest(test::VideoTestConstants::kDefaultTimeout),
        FakeEncoder(env),
        encoder_factory_(this),
        payload_name_(payload_name),
        configs_(configs),
        expectations_(expectations),
        encoder_info_override_(env.field_trials()) {}

  void OnFrameGeneratorCapturerCreated(
      test::FrameGeneratorCapturer* frame_generator_capturer) override {
    frame_generator_capturer->SetSinkWantsObserver(this);
    // Set initial resolution.
    frame_generator_capturer->ChangeResolution(1280, 720);
  }

  void OnSinkWantsChanged(VideoSinkInterface<VideoFrame>* sink,
                          const VideoSinkWants& wants) override {}

  size_t GetNumVideoStreams() const override {
    return SupportsSpatialLayers(payload_name_) ? 1 : configs_.size();
  }

  void ModifyVideoConfigs(
      VideoSendStream::Config* send_config,
      std::vector<VideoReceiveStreamInterface::Config>* receive_configs,
      VideoEncoderConfig* encoder_config) override {
    send_config->encoder_settings.encoder_factory = &encoder_factory_;
    send_config->rtp.payload_name = payload_name_;
    send_config->rtp.payload_type =
        test::VideoTestConstants::kVideoSendPayloadType;
    const VideoCodecType codec_type = PayloadStringToCodecType(payload_name_);
    encoder_config->codec_type = codec_type;
    encoder_config->video_stream_factory = nullptr;
    encoder_config->max_bitrate_bps = -1;
    if (configs_.size() == 1 && configs_[0].bitrate.max)
      encoder_config->max_bitrate_bps = configs_[0].bitrate.max->bps();
    if (SupportsSpatialLayers(payload_name_)) {
      // Simulcast layers indicates which spatial layers are active.
      encoder_config->simulcast_layers.resize(configs_.size());
    }
    double scale_factor = 1.0;
    for (int i = configs_.size() - 1; i >= 0; --i) {
      VideoStream& stream = encoder_config->simulcast_layers[i];
      stream.active = configs_[i].active;
      stream.scalability_mode = configs_[i].scalability_mode;
      if (configs_[i].bitrate.min)
        stream.min_bitrate_bps = configs_[i].bitrate.min->bps();
      if (configs_[i].bitrate.max)
        stream.max_bitrate_bps = configs_[i].bitrate.max->bps();
      stream.scale_resolution_down_by = scale_factor;
      scale_factor *= SupportsSpatialLayers(payload_name_) ? 1.0 : 2.0;
    }
    SetEncoderSpecific(encoder_config, codec_type, configs_.size());
  }

  int32_t InitEncode(const VideoCodec* codec,
                     const Settings& settings) override {
    for (const auto& expected : expectations_) {
      BitrateLimits limits = GetLayerBitrateLimits(expected.pixels, *codec);
      if (expected.eq_bitrate.min)
        EXPECT_EQ(*expected.eq_bitrate.min, limits.min);
      if (expected.eq_bitrate.max)
        EXPECT_EQ(*expected.eq_bitrate.max, limits.max);
      EXPECT_NE(expected.ne_bitrate.min, limits.min);
      EXPECT_NE(expected.ne_bitrate.max, limits.max);
    }
    observation_complete_.Set();
    return 0;
  }

  VideoEncoder::EncoderInfo GetEncoderInfo() const override {
    EncoderInfo info = FakeEncoder::GetEncoderInfo();
    if (!encoder_info_override_.resolution_bitrate_limits().empty()) {
      info.resolution_bitrate_limits =
          encoder_info_override_.resolution_bitrate_limits();
    }
    return info;
  }

  void PerformTest() override {
    ASSERT_TRUE(Wait()) << "Timed out while waiting for InitEncode() call.";
  }

 private:
  test::VideoEncoderProxyFactory encoder_factory_;
  const std::string payload_name_;
  const std::vector<TestConfig> configs_;
  const std::vector<Expectation> expectations_;
  const LibvpxVp8EncoderInfoSettings encoder_info_override_;
};

TEST_P(ResolutionBitrateLimitsTest, LimitsApplied) {
  field_trials().Set("WebRTC-GetEncoderInfoOverride",
                     "frame_size_pixels:921600,"
                     "min_start_bitrate_bps:0,"
                     "min_bitrate_bps:32000,"
                     "max_bitrate_bps:3333000");

  InitEncodeTest test(
      env(), payload_name_, {{.active = true}},
      // Expectations:
      {{.pixels = 1280 * 720,
        .eq_bitrate = {.min = DataRate::KilobitsPerSec(32),
                       .max = DataRate::KilobitsPerSec(3333)}}});
  RunBaseTest(&test);
}

TEST_F(ResolutionBitrateLimitsWithScalabilityModeTest,
       OneStreamDefaultMaxBitrateAppliedForOneSpatialLayer) {
  InitEncodeTest test(
      env(), "VP9",
      {{.active = true,
        .bitrate = {.min = DataRate::KilobitsPerSec(30),
                    .max = DataRate::KilobitsPerSec(3000)},
        .scalability_mode = ScalabilityMode::kL1T1}},
      // Expectations:
      {{.pixels = 1280 * 720,
        .eq_bitrate = {.min = DataRate::KilobitsPerSec(30),
                       .max = DataRate::KilobitsPerSec(3000)}}});
  RunBaseTest(&test);
}

TEST_F(ResolutionBitrateLimitsWithScalabilityModeTest,
       OneStreamSvcMaxBitrateAppliedForTwoSpatialLayers) {
  InitEncodeTest test(
      env(), "VP9",
      {{.active = true,
        .bitrate = {.min = DataRate::KilobitsPerSec(30),
                    .max = DataRate::KilobitsPerSec(3000)},
        .scalability_mode = ScalabilityMode::kL2T1}},
      // Expectations:
      {{.pixels = 1280 * 720,
        .ne_bitrate = {.min = std::nullopt,
                       .max = DataRate::KilobitsPerSec(3000)}}});
  RunBaseTest(&test);
}
TEST_F(ResolutionBitrateLimitsWithScalabilityModeTest,
       OneStreamLimitsAppliedForOneSpatialLayer) {
  field_trials().Set("WebRTC-GetEncoderInfoOverride",
                     "frame_size_pixels:921600,"
                     "min_start_bitrate_bps:0,"
                     "min_bitrate_bps:32000,"
                     "max_bitrate_bps:3333000");

  InitEncodeTest test(
      env(), "VP9",
      {{.active = true, .scalability_mode = ScalabilityMode::kL1T1}},
      // Expectations:
      {{.pixels = 1280 * 720,
        .eq_bitrate = {.min = DataRate::KilobitsPerSec(32),
                       .max = DataRate::KilobitsPerSec(3333)}}});
  RunBaseTest(&test);
}

TEST_F(ResolutionBitrateLimitsWithScalabilityModeTest,
       OneStreamLimitsNotAppliedForMultipleSpatialLayers) {
  field_trials().Set("WebRTC-GetEncoderInfoOverride",
                     "frame_size_pixels:230400|921600,"
                     "min_start_bitrate_bps:0|0,"
                     "min_bitrate_bps:21000|32000,"
                     "max_bitrate_bps:2222000|3333000");

  InitEncodeTest test(
      env(), "VP9",
      {{.active = true, .scalability_mode = ScalabilityMode::kL2T1}},
      // Expectations:
      {{.pixels = 640 * 360,
        .ne_bitrate = {.min = DataRate::KilobitsPerSec(31),
                       .max = DataRate::KilobitsPerSec(2222)}},
       {.pixels = 1280 * 720,
        .ne_bitrate = {.min = DataRate::KilobitsPerSec(32),
                       .max = DataRate::KilobitsPerSec(3333)}}});
  RunBaseTest(&test);
}

TEST_P(ResolutionBitrateLimitsTest, EncodingsApplied) {
  InitEncodeTest test(
      env(), payload_name_,
      {{.active = true,
        .bitrate = {.min = DataRate::KilobitsPerSec(22),
                    .max = DataRate::KilobitsPerSec(3555)}}},
      // Expectations:
      {{.pixels = 1280 * 720,
        .eq_bitrate = {.min = DataRate::KilobitsPerSec(22),
                       .max = DataRate::KilobitsPerSec(3555)}}});
  RunBaseTest(&test);
}

TEST_P(ResolutionBitrateLimitsTest, IntersectionApplied) {
  field_trials().Set("WebRTC-GetEncoderInfoOverride",
                     "frame_size_pixels:921600,"
                     "min_start_bitrate_bps:0,"
                     "min_bitrate_bps:32000,"
                     "max_bitrate_bps:3333000");

  InitEncodeTest test(
      env(), payload_name_,
      {{.active = true,
        .bitrate = {.min = DataRate::KilobitsPerSec(22),
                    .max = DataRate::KilobitsPerSec(1555)}}},
      // Expectations:
      {{.pixels = 1280 * 720,
        .eq_bitrate = {.min = DataRate::KilobitsPerSec(32),
                       .max = DataRate::KilobitsPerSec(1555)}}});
  RunBaseTest(&test);
}

TEST_P(ResolutionBitrateLimitsTest, LimitsAppliedMiddleActive) {
  field_trials().Set("WebRTC-GetEncoderInfoOverride",
                     "frame_size_pixels:230400|921600,"
                     "min_start_bitrate_bps:0|0,"
                     "min_bitrate_bps:21000|32000,"
                     "max_bitrate_bps:2222000|3333000");

  InitEncodeTest test(
      env(), payload_name_,
      {{.active = false}, {.active = true}, {.active = false}},
      // Expectations:
      {{.pixels = 640 * 360,
        .eq_bitrate = {.min = DataRate::KilobitsPerSec(21),
                       .max = DataRate::KilobitsPerSec(2222)}}});
  RunBaseTest(&test);
}

TEST_P(ResolutionBitrateLimitsTest, EncodingMinMaxBitrateAppliedMiddleActive) {
  field_trials().Set("WebRTC-GetEncoderInfoOverride",
                     "frame_size_pixels:230400|921600,"
                     "min_start_bitrate_bps:0|0,"
                     "min_bitrate_bps:31000|32000,"
                     "max_bitrate_bps:1111000|3333000");

  InitEncodeTest test(
      env(), payload_name_,
      {{.active = false,
        .bitrate = {.min = DataRate::KilobitsPerSec(28),
                    .max = DataRate::KilobitsPerSec(1000)}},
       {.active = true,
        .bitrate = {.min = DataRate::KilobitsPerSec(28),
                    .max = DataRate::KilobitsPerSec(1555)}},
       {.active = false}},
      // Expectations:
      {{.pixels = 640 * 360,
        .eq_bitrate = {.min = DataRate::KilobitsPerSec(28),
                       .max = DataRate::KilobitsPerSec(1555)}}});
  RunBaseTest(&test);
}

TEST_P(ResolutionBitrateLimitsTest, MinBitrateNotAboveEncodingMax) {
  field_trials().Set("WebRTC-GetEncoderInfoOverride",
                     "frame_size_pixels:230400|921600,"
                     "min_start_bitrate_bps:0|0,"
                     "min_bitrate_bps:31000|32000,"
                     "max_bitrate_bps:1111000|3333000");

  InitEncodeTest test(
      env(), payload_name_,
      {{.active = false},
       {.active = true,
        .bitrate = {.min = std::nullopt, .max = DataRate::KilobitsPerSec(25)}},
       {.active = false}},
      // Expectations:
      {{.pixels = 640 * 360,
        .eq_bitrate = {.min = DataRate::KilobitsPerSec(25),
                       .max = DataRate::KilobitsPerSec(25)}}});
  RunBaseTest(&test);
}

TEST_P(ResolutionBitrateLimitsTest, MaxBitrateNotBelowEncodingMin) {
  field_trials().Set("WebRTC-GetEncoderInfoOverride",
                     "frame_size_pixels:230400|921600,"
                     "min_start_bitrate_bps:0|0,"
                     "min_bitrate_bps:21000|22000,"
                     "max_bitrate_bps:31000|32000");

  InitEncodeTest test(
      env(), payload_name_,
      {{.active = false,
        .bitrate = {.min = DataRate::KilobitsPerSec(50), .max = std::nullopt}},
       {.active = true,
        .bitrate = {.min = DataRate::KilobitsPerSec(50), .max = std::nullopt}},
       {.active = false}},
      // Expectations:
      {{.pixels = 640 * 360,
        .eq_bitrate = {.min = DataRate::KilobitsPerSec(50),
                       .max = DataRate::KilobitsPerSec(50)}}});
  RunBaseTest(&test);
}

TEST_P(ResolutionBitrateLimitsTest, DefaultLimitsAppliedMiddleActive) {
  const std::optional<VideoEncoder::ResolutionBitrateLimits>
      kDefaultSinglecastLimits360p =
          EncoderInfoSettings::GetDefaultSinglecastBitrateLimitsForResolution(
              PayloadStringToCodecType(payload_name_), 640 * 360);

  InitEncodeTest test(
      env(), payload_name_,
      {{.active = false}, {.active = true}, {.active = false}},
      // Expectations:
      {{.pixels = 640 * 360,
        .eq_bitrate = {.min = DataRate::BitsPerSec(
                           kDefaultSinglecastLimits360p->min_bitrate_bps),
                       .max = DataRate::BitsPerSec(
                           kDefaultSinglecastLimits360p->max_bitrate_bps)}}});
  RunBaseTest(&test);
}

TEST_F(ResolutionBitrateLimitsWithScalabilityModeTest,
       DefaultLimitsAppliedForOneSpatialLayer) {
  const std::optional<VideoEncoder::ResolutionBitrateLimits>
      kDefaultSinglecastLimits720p =
          EncoderInfoSettings::GetDefaultSinglecastBitrateLimitsForResolution(
              PayloadStringToCodecType("VP9"), 1280 * 720);

  InitEncodeTest test(
      env(), "VP9",
      {{.active = true, .scalability_mode = ScalabilityMode::kL1T3},
       {.active = false}},
      // Expectations:
      {{.pixels = 1280 * 720,
        .eq_bitrate = {.min = DataRate::BitsPerSec(
                           kDefaultSinglecastLimits720p->min_bitrate_bps),
                       .max = DataRate::BitsPerSec(
                           kDefaultSinglecastLimits720p->max_bitrate_bps)}}});
  RunBaseTest(&test);
}

TEST_P(ResolutionBitrateLimitsTest, LimitsAppliedHighestActive) {
  field_trials().Set("WebRTC-GetEncoderInfoOverride",
                     "frame_size_pixels:230400|921600,"
                     "min_start_bitrate_bps:0|0,"
                     "min_bitrate_bps:31000|32000,"
                     "max_bitrate_bps:2222000|3333000");

  InitEncodeTest test(
      env(), payload_name_,
      {{.active = false}, {.active = false}, {.active = true}},
      // Expectations:
      {{.pixels = 1280 * 720,
        .eq_bitrate = {.min = DataRate::KilobitsPerSec(32),
                       .max = DataRate::KilobitsPerSec(3333)}}});
  RunBaseTest(&test);
}

TEST_P(ResolutionBitrateLimitsTest, EncodingMinMaxBitrateAppliedHighestActive) {
  field_trials().Set("WebRTC-GetEncoderInfoOverride",
                     "frame_size_pixels:230400|921600,"
                     "min_start_bitrate_bps:0|0,"
                     "min_bitrate_bps:31000|32000,"
                     "max_bitrate_bps:555000|1111000");

  InitEncodeTest test(
      env(), payload_name_,
      {{.active = false,
        .bitrate = {.min = DataRate::KilobitsPerSec(28),
                    .max = DataRate::KilobitsPerSec(500)}},
       {.active = false,
        .bitrate = {.min = DataRate::KilobitsPerSec(28),
                    .max = DataRate::KilobitsPerSec(1000)}},
       {.active = true,
        .bitrate = {.min = DataRate::KilobitsPerSec(28),
                    .max = DataRate::KilobitsPerSec(1555)}}},
      // Expectations:
      {{.pixels = 1280 * 720,
        .eq_bitrate = {.min = DataRate::KilobitsPerSec(28),
                       .max = DataRate::KilobitsPerSec(1555)}}});
  RunBaseTest(&test);
}

TEST_P(ResolutionBitrateLimitsTest, LimitsNotAppliedLowestActive) {
  field_trials().Set("WebRTC-GetEncoderInfoOverride",
                     "frame_size_pixels:230400|921600,"
                     "min_start_bitrate_bps:0|0,"
                     "min_bitrate_bps:31000|32000,"
                     "max_bitrate_bps:2222000|3333000");

  InitEncodeTest test(
      env(), payload_name_, {{.active = true}, {.active = false}},
      // Expectations:
      {{.pixels = 640 * 360,
        .ne_bitrate = {.min = DataRate::KilobitsPerSec(31),
                       .max = DataRate::KilobitsPerSec(2222)}},
       {.pixels = 1280 * 720,
        .ne_bitrate = {.min = DataRate::KilobitsPerSec(32),
                       .max = DataRate::KilobitsPerSec(3333)}}});
  RunBaseTest(&test);
}

TEST_F(ResolutionBitrateLimitsWithScalabilityModeTest,
       LimitsAppliedForVp9OneSpatialLayer) {
  field_trials().Set("WebRTC-GetEncoderInfoOverride",
                     "frame_size_pixels:230400|921600,"
                     "min_start_bitrate_bps:0|0,"
                     "min_bitrate_bps:31000|32000,"
                     "max_bitrate_bps:2222000|3333000");

  InitEncodeTest test(
      env(), "VP9",
      {{.active = true, .scalability_mode = ScalabilityMode::kL1T1},
       {.active = false}},
      // Expectations:
      {{.pixels = 1280 * 720,
        .eq_bitrate = {.min = DataRate::KilobitsPerSec(32),
                       .max = DataRate::KilobitsPerSec(3333)}}});
  RunBaseTest(&test);
}

TEST_F(ResolutionBitrateLimitsWithScalabilityModeTest,
       LimitsNotAppliedForVp9MultipleSpatialLayers) {
  field_trials().Set("WebRTC-GetEncoderInfoOverride",
                     "frame_size_pixels:230400|921600,"
                     "min_start_bitrate_bps:0|0,"
                     "min_bitrate_bps:31000|32000,"
                     "max_bitrate_bps:2222000|3333000");

  InitEncodeTest test(
      env(), "VP9",
      {{.active = true, .scalability_mode = ScalabilityMode::kL2T1},
       {.active = false}},
      // Expectations:
      {{.pixels = 640 * 360,
        .ne_bitrate = {.min = DataRate::KilobitsPerSec(31),
                       .max = DataRate::KilobitsPerSec(2222)}},
       {.pixels = 1280 * 720,
        .ne_bitrate = {.min = DataRate::KilobitsPerSec(32),
                       .max = DataRate::KilobitsPerSec(3333)}}});
  RunBaseTest(&test);
}

TEST_F(ResolutionBitrateLimitsWithScalabilityModeTest,
       OneStreamLimitsAppliedForAv1OneSpatialLayer) {
  field_trials().Set("WebRTC-GetEncoderInfoOverride",
                     "frame_size_pixels:921600,"
                     "min_start_bitrate_bps:0,"
                     "min_bitrate_bps:32000,"
                     "max_bitrate_bps:133000");

  InitEncodeTest test(
      env(), "AV1",
      {{.active = true, .scalability_mode = ScalabilityMode::kL1T1}},
      // Expectations:
      {{.pixels = 1280 * 720,
        .eq_bitrate = {.min = DataRate::KilobitsPerSec(32),
                       .max = DataRate::KilobitsPerSec(133)}}});
  RunBaseTest(&test);
}

TEST_F(ResolutionBitrateLimitsWithScalabilityModeTest,
       LimitsAppliedForAv1SingleSpatialLayer) {
  field_trials().Set("WebRTC-GetEncoderInfoOverride",
                     "frame_size_pixels:230400|921600,"
                     "min_start_bitrate_bps:0|0,"
                     "min_bitrate_bps:25000|80000,"
                     "max_bitrate_bps:400000|1200000");

  InitEncodeTest test(
      env(), "AV1",
      {{.active = true, .scalability_mode = ScalabilityMode::kL1T1},
       {.active = false}},
      // Expectations:
      {{.pixels = 1280 * 720,
        .eq_bitrate = {.min = DataRate::KilobitsPerSec(80),
                       .max = DataRate::KilobitsPerSec(1200)}}});
  RunBaseTest(&test);
}

TEST_F(ResolutionBitrateLimitsWithScalabilityModeTest,
       EncodingMinMaxBitrateAppliedForAv1SingleSpatialLayer) {
  field_trials().Set("WebRTC-GetEncoderInfoOverride",
                     "frame_size_pixels:921600,"
                     "min_start_bitrate_bps:0,"
                     "min_bitrate_bps:32000,"
                     "max_bitrate_bps:99000");

  InitEncodeTest test(env(), "AV1",
                      {{.active = true,
                        .bitrate = {.min = DataRate::KilobitsPerSec(28),
                                    .max = DataRate::KilobitsPerSec(100)},
                        .scalability_mode = ScalabilityMode::kL1T1},
                       {.active = false}},
                      // Expectations:
                      {{.pixels = 1280 * 720,
                        .eq_bitrate = {.min = DataRate::KilobitsPerSec(28),
                                       .max = DataRate::KilobitsPerSec(100)}}});
  RunBaseTest(&test);
}

TEST_F(ResolutionBitrateLimitsWithScalabilityModeTest,
       LimitsNotAppliedForAv1MultipleSpatialLayers) {
  field_trials().Set("WebRTC-GetEncoderInfoOverride",
                     "frame_size_pixels:230400|921600,"
                     "min_start_bitrate_bps:0|0,"
                     "min_bitrate_bps:20000|25000,"
                     "max_bitrate_bps:900000|1333000");

  InitEncodeTest test(
      env(), "AV1",
      {{.active = true, .scalability_mode = ScalabilityMode::kL2T1},
       {.active = false}},
      // Expectations:
      {{.pixels = 640 * 360,
        .ne_bitrate = {.min = DataRate::KilobitsPerSec(20),
                       .max = DataRate::KilobitsPerSec(900)}},
       {.pixels = 1280 * 720,
        .ne_bitrate = {.min = DataRate::KilobitsPerSec(25),
                       .max = DataRate::KilobitsPerSec(1333)}}});
  RunBaseTest(&test);
}

TEST_P(ResolutionBitrateLimitsTest, LimitsNotAppliedSimulcast) {
  field_trials().Set("WebRTC-GetEncoderInfoOverride",
                     "frame_size_pixels:230400|921600,"
                     "min_start_bitrate_bps:0|0,"
                     "min_bitrate_bps:31000|32000,"
                     "max_bitrate_bps:2222000|3333000");

  InitEncodeTest test(
      env(), payload_name_, {{.active = true}, {.active = true}},
      // Expectations:
      {{.pixels = 640 * 360,
        .ne_bitrate = {.min = DataRate::KilobitsPerSec(31),
                       .max = DataRate::KilobitsPerSec(2222)}},
       {.pixels = 1280 * 720,
        .ne_bitrate = {.min = DataRate::KilobitsPerSec(32),
                       .max = DataRate::KilobitsPerSec(3333)}}});
  RunBaseTest(&test);
}

}  // namespace test
}  // namespace webrtc
