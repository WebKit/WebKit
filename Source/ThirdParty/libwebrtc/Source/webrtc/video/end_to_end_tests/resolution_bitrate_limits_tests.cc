/*
 *  Copyright 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>

#include "media/engine/webrtc_video_engine.h"
#include "rtc_base/experiments/encoder_info_settings.h"
#include "test/call_test.h"
#include "test/fake_encoder.h"
#include "test/field_trial.h"
#include "test/gtest.h"
#include "test/video_encoder_proxy_factory.h"

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
        rtc::make_ref_counted<VideoEncoderConfig::Vp9EncoderSpecificSettings>(
            vp9);
  }
}

SpatialLayer GetLayer(int pixels, const VideoCodec& codec) {
  if (codec.codecType == VideoCodecType::kVideoCodecVP9) {
    for (size_t i = 0; i < codec.VP9().numberOfSpatialLayers; ++i) {
      if (codec.spatialLayers[i].width * codec.spatialLayers[i].height ==
          pixels) {
        return codec.spatialLayers[i];
      }
    }
  } else {
    for (int i = 0; i < codec.numberOfSimulcastStreams; ++i) {
      if (codec.simulcastStream[i].width * codec.simulcastStream[i].height ==
          pixels) {
        return codec.simulcastStream[i];
      }
    }
  }
  ADD_FAILURE();
  return SpatialLayer();
}

}  // namespace

class ResolutionBitrateLimitsTest
    : public test::CallTest,
      public ::testing::WithParamInterface<std::string> {
 public:
  ResolutionBitrateLimitsTest() : payload_name_(GetParam()) {}

  const std::string payload_name_;
};

INSTANTIATE_TEST_SUITE_P(PayloadName,
                         ResolutionBitrateLimitsTest,
                         ::testing::Values("VP8", "VP9"));

class InitEncodeTest : public test::EndToEndTest,
                       public test::FrameGeneratorCapturer::SinkWantsObserver,
                       public test::FakeEncoder {
 public:
  struct Bitrate {
    const absl::optional<uint32_t> min;
    const absl::optional<uint32_t> max;
  };
  struct TestConfig {
    const bool active;
    const Bitrate bitrate_bps;
  };
  struct Expectation {
    const uint32_t pixels = 0;
    const Bitrate eq_bitrate_bps;
    const Bitrate ne_bitrate_bps;
  };

  InitEncodeTest(const std::string& payload_name,
                 const std::vector<TestConfig>& configs,
                 const std::vector<Expectation>& expectations)
      : EndToEndTest(test::CallTest::kDefaultTimeoutMs),
        FakeEncoder(Clock::GetRealTimeClock()),
        encoder_factory_(this),
        payload_name_(payload_name),
        configs_(configs),
        expectations_(expectations) {}

  void OnFrameGeneratorCapturerCreated(
      test::FrameGeneratorCapturer* frame_generator_capturer) override {
    frame_generator_capturer->SetSinkWantsObserver(this);
    // Set initial resolution.
    frame_generator_capturer->ChangeResolution(1280, 720);
  }

  void OnSinkWantsChanged(rtc::VideoSinkInterface<VideoFrame>* sink,
                          const rtc::VideoSinkWants& wants) override {}

  size_t GetNumVideoStreams() const override {
    return (payload_name_ == "VP9") ? 1 : configs_.size();
  }

  void ModifyVideoConfigs(
      VideoSendStream::Config* send_config,
      std::vector<VideoReceiveStream::Config>* receive_configs,
      VideoEncoderConfig* encoder_config) override {
    send_config->encoder_settings.encoder_factory = &encoder_factory_;
    send_config->rtp.payload_name = payload_name_;
    send_config->rtp.payload_type = test::CallTest::kVideoSendPayloadType;
    const VideoCodecType codec_type = PayloadStringToCodecType(payload_name_);
    encoder_config->codec_type = codec_type;
    encoder_config->video_stream_factory =
        rtc::make_ref_counted<cricket::EncoderStreamFactory>(
            payload_name_, /*max qp*/ 0, /*screencast*/ false,
            /*screenshare enabled*/ false);
    encoder_config->max_bitrate_bps = -1;
    if (configs_.size() == 1 && configs_[0].bitrate_bps.max)
      encoder_config->max_bitrate_bps = *configs_[0].bitrate_bps.max;
    if (payload_name_ == "VP9") {
      // Simulcast layers indicates which spatial layers are active.
      encoder_config->simulcast_layers.resize(configs_.size());
    }
    double scale_factor = 1.0;
    for (int i = configs_.size() - 1; i >= 0; --i) {
      VideoStream& stream = encoder_config->simulcast_layers[i];
      stream.active = configs_[i].active;
      if (configs_[i].bitrate_bps.min)
        stream.min_bitrate_bps = *configs_[i].bitrate_bps.min;
      if (configs_[i].bitrate_bps.max)
        stream.max_bitrate_bps = *configs_[i].bitrate_bps.max;
      stream.scale_resolution_down_by = scale_factor;
      scale_factor *= (payload_name_ == "VP9") ? 1.0 : 2.0;
    }
    SetEncoderSpecific(encoder_config, codec_type, configs_.size());
  }

  int32_t InitEncode(const VideoCodec* codec,
                     const Settings& settings) override {
    for (const auto& expected : expectations_) {
      SpatialLayer layer = GetLayer(expected.pixels, *codec);
      if (expected.eq_bitrate_bps.min)
        EXPECT_EQ(*expected.eq_bitrate_bps.min, layer.minBitrate * 1000);
      if (expected.eq_bitrate_bps.max)
        EXPECT_EQ(*expected.eq_bitrate_bps.max, layer.maxBitrate * 1000);
      EXPECT_NE(expected.ne_bitrate_bps.min, layer.minBitrate * 1000);
      EXPECT_NE(expected.ne_bitrate_bps.max, layer.maxBitrate * 1000);
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
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-GetEncoderInfoOverride/"
      "frame_size_pixels:921600,"
      "min_start_bitrate_bps:0,"
      "min_bitrate_bps:32000,"
      "max_bitrate_bps:3333000/");

  InitEncodeTest test(
      payload_name_,
      {{/*active=*/true, /*bitrate_bps=*/{absl::nullopt, absl::nullopt}}},
      // Expectations:
      {{1280 * 720,
        /*eq_bitrate_bps=*/{32000, 3333000},
        /*ne_bitrate_bps=*/{absl::nullopt, absl::nullopt}}});
  RunBaseTest(&test);
}

TEST_P(ResolutionBitrateLimitsTest, EncodingsApplied) {
  InitEncodeTest test(payload_name_,
                      {{/*active=*/true, /*bitrate_bps=*/{22000, 3555000}}},
                      // Expectations:
                      {{1280 * 720,
                        /*eq_bitrate_bps=*/{22000, 3555000},
                        /*ne_bitrate_bps=*/{absl::nullopt, absl::nullopt}}});
  RunBaseTest(&test);
}

TEST_P(ResolutionBitrateLimitsTest, IntersectionApplied) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-GetEncoderInfoOverride/"
      "frame_size_pixels:921600,"
      "min_start_bitrate_bps:0,"
      "min_bitrate_bps:32000,"
      "max_bitrate_bps:3333000/");

  InitEncodeTest test(payload_name_,
                      {{/*active=*/true, /*bitrate_bps=*/{22000, 1555000}}},
                      // Expectations:
                      {{1280 * 720,
                        /*eq_bitrate_bps=*/{32000, 1555000},
                        /*ne_bitrate_bps=*/{absl::nullopt, absl::nullopt}}});
  RunBaseTest(&test);
}

TEST_P(ResolutionBitrateLimitsTest, LimitsAppliedMiddleActive) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-GetEncoderInfoOverride/"
      "frame_size_pixels:230400|921600,"
      "min_start_bitrate_bps:0|0,"
      "min_bitrate_bps:21000|32000,"
      "max_bitrate_bps:2222000|3333000/");

  InitEncodeTest test(
      payload_name_,
      {{/*active=*/false, /*bitrate_bps=*/{absl::nullopt, absl::nullopt}},
       {/*active=*/true, /*bitrate_bps=*/{absl::nullopt, absl::nullopt}},
       {/*active=*/false, /*bitrate_bps=*/{absl::nullopt, absl::nullopt}}},
      // Expectations:
      {{640 * 360,
        /*eq_bitrate_bps=*/{21000, 2222000},
        /*ne_bitrate_bps=*/{absl::nullopt, absl::nullopt}}});

  RunBaseTest(&test);
}

TEST_P(ResolutionBitrateLimitsTest, IntersectionAppliedMiddleActive) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-GetEncoderInfoOverride/"
      "frame_size_pixels:230400|921600,"
      "min_start_bitrate_bps:0|0,"
      "min_bitrate_bps:31000|32000,"
      "max_bitrate_bps:2222000|3333000/");

  InitEncodeTest test(
      payload_name_,
      {{/*active=*/false, /*bitrate_bps=*/{absl::nullopt, absl::nullopt}},
       {/*active=*/true, /*bitrate_bps=*/{30000, 1555000}},
       {/*active=*/false, /*bitrate_bps=*/{absl::nullopt, absl::nullopt}}},
      // Expectations:
      {{640 * 360,
        /*eq_bitrate_bps=*/{31000, 1555000},
        /*ne_bitrate_bps=*/{absl::nullopt, absl::nullopt}}});
  RunBaseTest(&test);
}

TEST_P(ResolutionBitrateLimitsTest, DefaultLimitsAppliedMiddleActive) {
  const absl::optional<VideoEncoder::ResolutionBitrateLimits>
      kDefaultSinglecastLimits360p =
          EncoderInfoSettings::GetDefaultSinglecastBitrateLimitsForResolution(
              PayloadStringToCodecType(payload_name_), 640 * 360);

  InitEncodeTest test(
      payload_name_,
      {{/*active=*/false, /*bitrate_bps=*/{absl::nullopt, absl::nullopt}},
       {/*active=*/true, /*bitrate_bps=*/{absl::nullopt, absl::nullopt}},
       {/*active=*/false, /*bitrate_bps=*/{absl::nullopt, absl::nullopt}}},
      // Expectations:
      {{640 * 360,
        /*eq_bitrate_bps=*/
        {kDefaultSinglecastLimits360p->min_bitrate_bps,
         kDefaultSinglecastLimits360p->max_bitrate_bps},
        /*ne_bitrate_bps=*/{absl::nullopt, absl::nullopt}}});

  RunBaseTest(&test);
}

TEST_P(ResolutionBitrateLimitsTest, LimitsAppliedHighestActive) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-GetEncoderInfoOverride/"
      "frame_size_pixels:230400|921600,"
      "min_start_bitrate_bps:0|0,"
      "min_bitrate_bps:31000|32000,"
      "max_bitrate_bps:2222000|3333000/");

  InitEncodeTest test(
      payload_name_,
      {{/*active=*/false, /*bitrate_bps=*/{absl::nullopt, absl::nullopt}},
       {/*active=*/false, /*bitrate_bps=*/{absl::nullopt, absl::nullopt}},
       {/*active=*/true, /*bitrate_bps=*/{absl::nullopt, absl::nullopt}}},
      // Expectations:
      {{1280 * 720,
        /*eq_bitrate_bps=*/{32000, 3333000},
        /*ne_bitrate_bps=*/{absl::nullopt, absl::nullopt}}});
  RunBaseTest(&test);
}

TEST_P(ResolutionBitrateLimitsTest, IntersectionAppliedHighestActive) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-GetEncoderInfoOverride/"
      "frame_size_pixels:230400|921600,"
      "min_start_bitrate_bps:0|0,"
      "min_bitrate_bps:31000|32000,"
      "max_bitrate_bps:2222000|3333000/");

  InitEncodeTest test(
      payload_name_,
      {{/*active=*/false, /*bitrate_bps=*/{absl::nullopt, absl::nullopt}},
       {/*active=*/false, /*bitrate_bps=*/{absl::nullopt, absl::nullopt}},
       {/*active=*/true, /*bitrate_bps=*/{30000, 1555000}}},
      // Expectations:
      {{1280 * 720,
        /*eq_bitrate_bps=*/{32000, 1555000},
        /*ne_bitrate_bps=*/{absl::nullopt, absl::nullopt}}});
  RunBaseTest(&test);
}

TEST_P(ResolutionBitrateLimitsTest, LimitsNotAppliedLowestActive) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-GetEncoderInfoOverride/"
      "frame_size_pixels:230400|921600,"
      "min_start_bitrate_bps:0|0,"
      "min_bitrate_bps:31000|32000,"
      "max_bitrate_bps:2222000|3333000/");

  InitEncodeTest test(
      payload_name_,
      {{/*active=*/true, /*bitrate_bps=*/{absl::nullopt, absl::nullopt}},
       {/*active=*/false, /*bitrate_bps=*/{absl::nullopt, absl::nullopt}}},
      // Expectations:
      {{640 * 360,
        /*eq_bitrate_bps=*/{absl::nullopt, absl::nullopt},
        /*ne_bitrate_bps=*/{31000, 2222000}},
       {1280 * 720,
        /*eq_bitrate_bps=*/{absl::nullopt, absl::nullopt},
        /*ne_bitrate_bps=*/{32000, 3333000}}});
  RunBaseTest(&test);
}

TEST_P(ResolutionBitrateLimitsTest, LimitsNotAppliedSimulcast) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-GetEncoderInfoOverride/"
      "frame_size_pixels:230400|921600,"
      "min_start_bitrate_bps:0|0,"
      "min_bitrate_bps:31000|32000,"
      "max_bitrate_bps:2222000|3333000/");

  InitEncodeTest test(
      payload_name_,
      {{/*active=*/true, /*bitrate_bps=*/{absl::nullopt, absl::nullopt}},
       {/*active=*/true, /*bitrate_bps=*/{absl::nullopt, absl::nullopt}}},
      // Expectations:
      {{640 * 360,
        /*eq_bitrate_bps=*/{absl::nullopt, absl::nullopt},
        /*ne_bitrate_bps=*/{31000, 2222000}},
       {1280 * 720,
        /*eq_bitrate_bps=*/{absl::nullopt, absl::nullopt},
        /*ne_bitrate_bps=*/{32000, 3333000}}});
  RunBaseTest(&test);
}

}  // namespace test
}  // namespace webrtc
