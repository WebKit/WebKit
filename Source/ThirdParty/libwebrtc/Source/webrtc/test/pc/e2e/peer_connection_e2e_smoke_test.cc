/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdint>
#include <memory>

#include "api/test/create_network_emulation_manager.h"
#include "api/test/create_peerconnection_quality_test_fixture.h"
#include "api/test/network_emulation_manager.h"
#include "api/test/peerconnection_quality_test_fixture.h"
#include "call/simulated_network.h"
#include "system_wrappers/include/field_trial.h"
#include "test/field_trial.h"
#include "test/gtest.h"
#include "test/pc/e2e/analyzer/audio/default_audio_quality_analyzer.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer.h"
#include "test/pc/e2e/network_quality_metrics_reporter.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

class PeerConnectionE2EQualityTestSmokeTest : public ::testing::Test {
 public:
  using PeerConfigurer = PeerConnectionE2EQualityTestFixture::PeerConfigurer;
  using RunParams = PeerConnectionE2EQualityTestFixture::RunParams;
  using VideoConfig = PeerConnectionE2EQualityTestFixture::VideoConfig;
  using VideoCodecConfig =
      PeerConnectionE2EQualityTestFixture::VideoCodecConfig;
  using AudioConfig = PeerConnectionE2EQualityTestFixture::AudioConfig;
  using ScreenShareConfig =
      PeerConnectionE2EQualityTestFixture::ScreenShareConfig;
  using ScrollingParams = PeerConnectionE2EQualityTestFixture::ScrollingParams;
  using VideoSimulcastConfig =
      PeerConnectionE2EQualityTestFixture::VideoSimulcastConfig;
  using EchoEmulationConfig =
      PeerConnectionE2EQualityTestFixture::EchoEmulationConfig;

  void RunTest(const std::string& test_case_name,
               const RunParams& run_params,
               rtc::FunctionView<void(PeerConfigurer*)> alice_configurer,
               rtc::FunctionView<void(PeerConfigurer*)> bob_configurer) {
    // Setup emulated network
    std::unique_ptr<NetworkEmulationManager> network_emulation_manager =
        CreateNetworkEmulationManager();

    auto alice_network_behavior =
        std::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig());
    SimulatedNetwork* alice_network_behavior_ptr = alice_network_behavior.get();
    EmulatedNetworkNode* alice_node =
        network_emulation_manager->CreateEmulatedNode(
            std::move(alice_network_behavior));
    EmulatedNetworkNode* bob_node =
        network_emulation_manager->CreateEmulatedNode(
            std::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
    auto* alice_endpoint =
        network_emulation_manager->CreateEndpoint(EmulatedEndpointConfig());
    EmulatedEndpoint* bob_endpoint =
        network_emulation_manager->CreateEndpoint(EmulatedEndpointConfig());
    network_emulation_manager->CreateRoute(alice_endpoint, {alice_node},
                                           bob_endpoint);
    network_emulation_manager->CreateRoute(bob_endpoint, {bob_node},
                                           alice_endpoint);

    // Create analyzers.
    std::unique_ptr<VideoQualityAnalyzerInterface> video_quality_analyzer =
        std::make_unique<DefaultVideoQualityAnalyzer>();
    // This is only done for the sake of smoke testing. In general there should
    // be no need to explicitly pull data from analyzers after the run.
    auto* video_analyzer_ptr =
        static_cast<DefaultVideoQualityAnalyzer*>(video_quality_analyzer.get());

    auto fixture = CreatePeerConnectionE2EQualityTestFixture(
        test_case_name, /*audio_quality_analyzer=*/nullptr,
        std::move(video_quality_analyzer));
    fixture->ExecuteAt(TimeDelta::Seconds(2),
                       [alice_network_behavior_ptr](TimeDelta) {
                         BuiltInNetworkBehaviorConfig config;
                         config.loss_percent = 5;
                         alice_network_behavior_ptr->SetConfig(config);
                       });

    // Setup components. We need to provide rtc::NetworkManager compatible with
    // emulated network layer.
    EmulatedNetworkManagerInterface* alice_network =
        network_emulation_manager->CreateEmulatedNetworkManagerInterface(
            {alice_endpoint});
    EmulatedNetworkManagerInterface* bob_network =
        network_emulation_manager->CreateEmulatedNetworkManagerInterface(
            {bob_endpoint});

    fixture->AddPeer(alice_network->network_thread(),
                     alice_network->network_manager(), alice_configurer);
    fixture->AddPeer(bob_network->network_thread(),
                     bob_network->network_manager(), bob_configurer);
    fixture->AddQualityMetricsReporter(
        std::make_unique<NetworkQualityMetricsReporter>(alice_network,
                                                        bob_network));

    fixture->Run(run_params);

    EXPECT_GE(fixture->GetRealTestDuration(), run_params.run_duration);
    for (auto stream_label : video_analyzer_ptr->GetKnownVideoStreams()) {
      FrameCounters stream_conters =
          video_analyzer_ptr->GetPerStreamCounters().at(stream_label);
      // On some devices the pipeline can be too slow, so we actually can't
      // force real constraints here. Lets just check, that at least 1
      // frame passed whole pipeline.
      int64_t expected_min_fps = run_params.run_duration.seconds() * 30;
      EXPECT_GE(stream_conters.captured, expected_min_fps);
      EXPECT_GE(stream_conters.pre_encoded, 1);
      EXPECT_GE(stream_conters.encoded, 1);
      EXPECT_GE(stream_conters.received, 1);
      EXPECT_GE(stream_conters.decoded, 1);
      EXPECT_GE(stream_conters.rendered, 1);
    }
  }
};

}  // namespace

// IOS debug builds can be quite slow, disabling to avoid issues with timeouts.
#if defined(WEBRTC_IOS) && defined(WEBRTC_ARCH_ARM64) && !defined(NDEBUG)
#define MAYBE_Smoke DISABLED_Smoke
#else
#define MAYBE_Smoke Smoke
#endif
TEST_F(PeerConnectionE2EQualityTestSmokeTest, MAYBE_Smoke) {
  RunParams run_params(TimeDelta::Seconds(2));
  run_params.video_codecs = {
      VideoCodecConfig(cricket::kVp9CodecName, {{"profile-id", "0"}})};
  run_params.use_flex_fec = true;
  run_params.use_ulp_fec = true;
  run_params.video_encoder_bitrate_multiplier = 1.1;
  test::ScopedFieldTrials field_trials(
      std::string(field_trial::GetFieldTrialString()) +
      "WebRTC-UseStandardBytesStats/Enabled/");
  RunTest(
      "smoke", run_params,
      [](PeerConfigurer* alice) {
        VideoConfig video(640, 360, 30);
        video.stream_label = "alice-video";
        video.sync_group = "alice-media";
        alice->AddVideoConfig(std::move(video));

        AudioConfig audio;
        audio.stream_label = "alice-audio";
        audio.mode = AudioConfig::Mode::kFile;
        audio.input_file_name =
            test::ResourcePath("pc_quality_smoke_test_alice_source", "wav");
        audio.sampling_frequency_in_hz = 48000;
        audio.sync_group = "alice-media";
        alice->SetAudioConfig(std::move(audio));
      },
      [](PeerConfigurer* bob) {
        VideoConfig video(640, 360, 30);
        video.stream_label = "bob-video";
        video.temporal_layers_count = 2;
        bob->AddVideoConfig(std::move(video));

        VideoConfig screenshare(640, 360, 30);
        screenshare.stream_label = "bob-screenshare";
        screenshare.screen_share_config =
            ScreenShareConfig(TimeDelta::Seconds(2));
        screenshare.screen_share_config->scrolling_params = ScrollingParams(
            TimeDelta::Millis(1800), kDefaultSlidesWidth, kDefaultSlidesHeight);
        bob->AddVideoConfig(screenshare);

        AudioConfig audio;
        audio.stream_label = "bob-audio";
        audio.mode = AudioConfig::Mode::kFile;
        audio.input_file_name =
            test::ResourcePath("pc_quality_smoke_test_bob_source", "wav");
        bob->SetAudioConfig(std::move(audio));
      });
}

// IOS debug builds can be quite slow, disabling to avoid issues with timeouts.
#if defined(WEBRTC_IOS) && defined(WEBRTC_ARCH_ARM64) && !defined(NDEBUG)
#define MAYBE_Echo DISABLED_Echo
#else
#define MAYBE_Echo Echo
#endif
TEST_F(PeerConnectionE2EQualityTestSmokeTest, MAYBE_Echo) {
  RunParams run_params(TimeDelta::Seconds(2));
  run_params.echo_emulation_config = EchoEmulationConfig();
  RunTest(
      "smoke", run_params,
      [](PeerConfigurer* alice) {
        AudioConfig audio;
        audio.stream_label = "alice-audio";
        audio.mode = AudioConfig::Mode::kFile;
        audio.input_file_name =
            test::ResourcePath("pc_quality_smoke_test_alice_source", "wav");
        audio.sampling_frequency_in_hz = 48000;
        alice->SetAudioConfig(std::move(audio));
      },
      [](PeerConfigurer* bob) {
        AudioConfig audio;
        audio.stream_label = "bob-audio";
        audio.mode = AudioConfig::Mode::kFile;
        audio.input_file_name =
            test::ResourcePath("pc_quality_smoke_test_bob_source", "wav");
        bob->SetAudioConfig(std::move(audio));
      });
}

// IOS debug builds can be quite slow, disabling to avoid issues with timeouts.
#if defined(WEBRTC_IOS) && defined(WEBRTC_ARCH_ARM64) && !defined(NDEBUG)
#define MAYBE_Simulcast DISABLED_Simulcast
#else
#define MAYBE_Simulcast Simulcast
#endif
TEST_F(PeerConnectionE2EQualityTestSmokeTest, MAYBE_Simulcast) {
  RunParams run_params(TimeDelta::Seconds(2));
  run_params.video_codecs = {VideoCodecConfig(cricket::kVp8CodecName)};
  RunTest(
      "simulcast", run_params,
      [](PeerConfigurer* alice) {
        VideoConfig simulcast(1280, 720, 30);
        simulcast.stream_label = "alice-simulcast";
        simulcast.simulcast_config = VideoSimulcastConfig(3, 0);
        alice->AddVideoConfig(std::move(simulcast));

        AudioConfig audio;
        audio.stream_label = "alice-audio";
        audio.mode = AudioConfig::Mode::kFile;
        audio.input_file_name =
            test::ResourcePath("pc_quality_smoke_test_alice_source", "wav");
        alice->SetAudioConfig(std::move(audio));
      },
      [](PeerConfigurer* bob) {
        VideoConfig video(640, 360, 30);
        video.stream_label = "bob-video";
        bob->AddVideoConfig(std::move(video));

        AudioConfig audio;
        audio.stream_label = "bob-audio";
        audio.mode = AudioConfig::Mode::kFile;
        audio.input_file_name =
            test::ResourcePath("pc_quality_smoke_test_bob_source", "wav");
        bob->SetAudioConfig(std::move(audio));
      });
}

// IOS debug builds can be quite slow, disabling to avoid issues with timeouts.
#if defined(WEBRTC_IOS) && defined(WEBRTC_ARCH_ARM64) && !defined(NDEBUG)
#define MAYBE_Svc DISABLED_Svc
#else
#define MAYBE_Svc Svc
#endif
TEST_F(PeerConnectionE2EQualityTestSmokeTest, MAYBE_Svc) {
  RunParams run_params(TimeDelta::Seconds(2));
  run_params.video_codecs = {VideoCodecConfig(cricket::kVp9CodecName)};
  RunTest(
      "simulcast", run_params,
      [](PeerConfigurer* alice) {
        VideoConfig simulcast(1280, 720, 30);
        simulcast.stream_label = "alice-svc";
        // Because we have network with packets loss we can analyze only the
        // highest spatial layer in SVC mode.
        simulcast.simulcast_config = VideoSimulcastConfig(3, 2);
        alice->AddVideoConfig(std::move(simulcast));

        AudioConfig audio;
        audio.stream_label = "alice-audio";
        audio.mode = AudioConfig::Mode::kFile;
        audio.input_file_name =
            test::ResourcePath("pc_quality_smoke_test_alice_source", "wav");
        alice->SetAudioConfig(std::move(audio));
      },
      [](PeerConfigurer* bob) {
        VideoConfig video(640, 360, 30);
        video.stream_label = "bob-video";
        bob->AddVideoConfig(std::move(video));

        AudioConfig audio;
        audio.stream_label = "bob-audio";
        audio.mode = AudioConfig::Mode::kFile;
        audio.input_file_name =
            test::ResourcePath("pc_quality_smoke_test_bob_source", "wav");
        bob->SetAudioConfig(std::move(audio));
      });
}

// IOS debug builds can be quite slow, disabling to avoid issues with timeouts.
#if defined(WEBRTC_IOS) && defined(WEBRTC_ARCH_ARM64) && !defined(NDEBUG)
#define MAYBE_HighBitrate DISABLED_HighBitrate
#else
#define MAYBE_HighBitrate HighBitrate
#endif
TEST_F(PeerConnectionE2EQualityTestSmokeTest, MAYBE_HighBitrate) {
  RunParams run_params(TimeDelta::Seconds(2));
  run_params.video_codecs = {
      VideoCodecConfig(cricket::kVp9CodecName, {{"profile-id", "0"}})};

  RunTest(
      "smoke", run_params,
      [](PeerConfigurer* alice) {
        PeerConnectionInterface::BitrateParameters bitrate_params;
        bitrate_params.current_bitrate_bps = 3'000'000;
        bitrate_params.max_bitrate_bps = 3'000'000;
        alice->SetBitrateParameters(bitrate_params);
        VideoConfig video(800, 600, 30);
        video.stream_label = "alice-video";
        video.min_encode_bitrate_bps = 500'000;
        video.max_encode_bitrate_bps = 3'000'000;
        alice->AddVideoConfig(std::move(video));

        AudioConfig audio;
        audio.stream_label = "alice-audio";
        audio.mode = AudioConfig::Mode::kFile;
        audio.input_file_name =
            test::ResourcePath("pc_quality_smoke_test_alice_source", "wav");
        audio.sampling_frequency_in_hz = 48000;
        alice->SetAudioConfig(std::move(audio));
      },
      [](PeerConfigurer* bob) {
        PeerConnectionInterface::BitrateParameters bitrate_params;
        bitrate_params.current_bitrate_bps = 3'000'000;
        bitrate_params.max_bitrate_bps = 3'000'000;
        bob->SetBitrateParameters(bitrate_params);
        VideoConfig video(800, 600, 30);
        video.stream_label = "bob-video";
        video.min_encode_bitrate_bps = 500'000;
        video.max_encode_bitrate_bps = 3'000'000;
        bob->AddVideoConfig(std::move(video));

        AudioConfig audio;
        audio.stream_label = "bob-audio";
        audio.mode = AudioConfig::Mode::kFile;
        audio.input_file_name =
            test::ResourcePath("pc_quality_smoke_test_bob_source", "wav");
        bob->SetAudioConfig(std::move(audio));
      });
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
