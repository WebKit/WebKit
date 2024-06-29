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
#include <string>

#include "api/media_stream_interface.h"
#include "api/test/create_network_emulation_manager.h"
#include "api/test/create_peer_connection_quality_test_frame_generator.h"
#include "api/test/create_peerconnection_quality_test_fixture.h"
#include "api/test/metrics/global_metrics_logger_and_exporter.h"
#include "api/test/network_emulation_manager.h"
#include "api/test/pclf/media_configuration.h"
#include "api/test/pclf/media_quality_test_params.h"
#include "api/test/pclf/peer_configurer.h"
#include "api/test/peerconnection_quality_test_fixture.h"
#include "system_wrappers/include/field_trial.h"
#include "test/field_trial.h"
#include "test/gtest.h"
#include "test/network/simulated_network.h"
#include "test/pc/e2e/analyzer/audio/default_audio_quality_analyzer.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_shared_objects.h"
#include "test/pc/e2e/stats_based_network_quality_metrics_reporter.h"
#include "test/testsupport/file_utils.h"

#if defined(WEBRTC_MAC) || defined(WEBRTC_IOS)
#include "modules/video_coding/codecs/test/objc_codec_factory_helper.h"  // nogncheck
#endif

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

class PeerConnectionE2EQualityTestSmokeTest : public ::testing::Test {
 public:
  void SetUp() override {
    network_emulation_ = CreateNetworkEmulationManager();
    auto video_quality_analyzer = std::make_unique<DefaultVideoQualityAnalyzer>(
        network_emulation_->time_controller()->GetClock(),
        test::GetGlobalMetricsLogger());
    video_quality_analyzer_ = video_quality_analyzer.get();
    fixture_ = CreatePeerConnectionE2EQualityTestFixture(
        testing::UnitTest::GetInstance()->current_test_info()->name(),
        *network_emulation_->time_controller(),
        /*audio_quality_analyzer=*/nullptr, std::move(video_quality_analyzer));
  }

  std::pair<EmulatedNetworkManagerInterface*, EmulatedNetworkManagerInterface*>
  CreateNetwork() {
    EmulatedNetworkNode* alice_node = network_emulation_->CreateEmulatedNode(
        std::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
    EmulatedNetworkNode* bob_node = network_emulation_->CreateEmulatedNode(
        std::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));

    EmulatedEndpoint* alice_endpoint =
        network_emulation_->CreateEndpoint(EmulatedEndpointConfig());
    EmulatedEndpoint* bob_endpoint =
        network_emulation_->CreateEndpoint(EmulatedEndpointConfig());

    network_emulation_->CreateRoute(alice_endpoint, {alice_node}, bob_endpoint);
    network_emulation_->CreateRoute(bob_endpoint, {bob_node}, alice_endpoint);

    EmulatedNetworkManagerInterface* alice_network =
        network_emulation_->CreateEmulatedNetworkManagerInterface(
            {alice_endpoint});
    EmulatedNetworkManagerInterface* bob_network =
        network_emulation_->CreateEmulatedNetworkManagerInterface(
            {bob_endpoint});

    return std::make_pair(alice_network, bob_network);
  }

  void AddPeer(EmulatedNetworkManagerInterface* network,
               rtc::FunctionView<void(PeerConfigurer*)> update_configurer) {
    auto configurer =
        std::make_unique<PeerConfigurer>(network->network_dependencies());
    update_configurer(configurer.get());
    fixture_->AddPeer(std::move(configurer));
  }

  void RunAndCheckEachVideoStreamReceivedFrames(const RunParams& run_params) {
    fixture_->Run(run_params);

    EXPECT_GE(fixture_->GetRealTestDuration(), run_params.run_duration);
    VideoStreamsInfo known_streams = video_quality_analyzer_->GetKnownStreams();
    for (const StatsKey& stream_key : known_streams.GetStatsKeys()) {
      FrameCounters stream_conters =
          video_quality_analyzer_->GetPerStreamCounters().at(stream_key);
      // On some devices the pipeline can be too slow, so we actually can't
      // force real constraints here. Lets just check, that at least 1
      // frame passed whole pipeline.
      int64_t expected_min_fps = run_params.run_duration.seconds() * 15;
      EXPECT_GE(stream_conters.captured, expected_min_fps)
          << stream_key.ToString();
      EXPECT_GE(stream_conters.pre_encoded, 1) << stream_key.ToString();
      EXPECT_GE(stream_conters.encoded, 1) << stream_key.ToString();
      EXPECT_GE(stream_conters.received, 1) << stream_key.ToString();
      EXPECT_GE(stream_conters.decoded, 1) << stream_key.ToString();
      EXPECT_GE(stream_conters.rendered, 1) << stream_key.ToString();
    }
  }

  NetworkEmulationManager* network_emulation() {
    return network_emulation_.get();
  }

  PeerConnectionE2EQualityTestFixture* fixture() { return fixture_.get(); }

 private:
  std::unique_ptr<NetworkEmulationManager> network_emulation_;
  DefaultVideoQualityAnalyzer* video_quality_analyzer_;
  std::unique_ptr<PeerConnectionE2EQualityTestFixture> fixture_;
};

// IOS debug builds can be quite slow, disabling to avoid issues with timeouts.
#if defined(WEBRTC_IOS) && defined(WEBRTC_ARCH_ARM64) && !defined(NDEBUG)
#define MAYBE_Smoke DISABLED_Smoke
#else
#define MAYBE_Smoke Smoke
#endif
TEST_F(PeerConnectionE2EQualityTestSmokeTest, MAYBE_Smoke) {
  std::pair<EmulatedNetworkManagerInterface*, EmulatedNetworkManagerInterface*>
      network_links = CreateNetwork();
  AddPeer(network_links.first, [](PeerConfigurer* alice) {
    VideoConfig video(160, 120, 15);
    video.stream_label = "alice-video";
    video.sync_group = "alice-media";
    alice->AddVideoConfig(std::move(video));

    AudioConfig audio;
    audio.stream_label = "alice-audio";
    audio.input_file_name =
        test::ResourcePath("pc_quality_smoke_test_alice_source", "wav");
    audio.sampling_frequency_in_hz = 48000;
    audio.sync_group = "alice-media";
    alice->SetAudioConfig(std::move(audio));
    alice->SetVideoCodecs(
        {VideoCodecConfig(cricket::kVp9CodecName, {{"profile-id", "0"}})});

    alice->SetUseFlexFEC(true);
    alice->SetUseUlpFEC(true);
    alice->SetVideoEncoderBitrateMultiplier(1.1);
  });
  AddPeer(network_links.second, [](PeerConfigurer* charlie) {
    charlie->SetName("charlie");
    VideoConfig video(160, 120, 15);
    video.stream_label = "charlie-video";
    video.temporal_layers_count = 2;
    charlie->AddVideoConfig(std::move(video));

    AudioConfig audio;
    audio.stream_label = "charlie-audio";
    audio.input_file_name =
        test::ResourcePath("pc_quality_smoke_test_bob_source", "wav");
    charlie->SetAudioConfig(std::move(audio));
    charlie->SetVideoCodecs(
        {VideoCodecConfig(cricket::kVp9CodecName, {{"profile-id", "0"}})});

    charlie->SetUseFlexFEC(true);
    charlie->SetUseUlpFEC(true);
    charlie->SetVideoEncoderBitrateMultiplier(1.1);
  });
  fixture()->AddQualityMetricsReporter(
      std::make_unique<StatsBasedNetworkQualityMetricsReporter>(
          std::map<std::string, std::vector<EmulatedEndpoint*>>(
              {{"alice", network_links.first->endpoints()},
               {"charlie", network_links.second->endpoints()}}),
          network_emulation(), test::GetGlobalMetricsLogger()));
  RunParams run_params(TimeDelta::Seconds(2));
  run_params.enable_flex_fec_support = true;
  RunAndCheckEachVideoStreamReceivedFrames(run_params);
}

#if defined(WEBRTC_MAC) || defined(WEBRTC_IOS)
TEST_F(PeerConnectionE2EQualityTestSmokeTest, SmokeH264) {
  std::pair<EmulatedNetworkManagerInterface*, EmulatedNetworkManagerInterface*>
      network_links = CreateNetwork();

  AddPeer(network_links.first, [](PeerConfigurer* alice) {
    VideoConfig video(160, 120, 15);
    video.stream_label = "alice-video";
    video.sync_group = "alice-media";
    alice->AddVideoConfig(std::move(video));

    AudioConfig audio;
    audio.stream_label = "alice-audio";
    audio.input_file_name =
        test::ResourcePath("pc_quality_smoke_test_alice_source", "wav");
    audio.sampling_frequency_in_hz = 48000;
    audio.sync_group = "alice-media";
    alice->SetAudioConfig(std::move(audio));
    alice->SetVideoCodecs({VideoCodecConfig(cricket::kH264CodecName)});
    alice->SetVideoEncoderFactory(webrtc::test::CreateObjCEncoderFactory());
    alice->SetVideoDecoderFactory(webrtc::test::CreateObjCDecoderFactory());
  });
  AddPeer(network_links.second, [](PeerConfigurer* charlie) {
    charlie->SetName("charlie");
    VideoConfig video(160, 120, 15);
    video.stream_label = "charlie-video";
    video.temporal_layers_count = 2;
    charlie->AddVideoConfig(std::move(video));

    AudioConfig audio;
    audio.stream_label = "charlie-audio";
    audio.input_file_name =
        test::ResourcePath("pc_quality_smoke_test_bob_source", "wav");
    charlie->SetAudioConfig(std::move(audio));
    charlie->SetVideoCodecs({VideoCodecConfig(cricket::kH264CodecName)});
    charlie->SetVideoEncoderFactory(webrtc::test::CreateObjCEncoderFactory());
    charlie->SetVideoDecoderFactory(webrtc::test::CreateObjCDecoderFactory());
  });

  fixture()->AddQualityMetricsReporter(
      std::make_unique<StatsBasedNetworkQualityMetricsReporter>(
          std::map<std::string, std::vector<EmulatedEndpoint*>>(
              {{"alice", network_links.first->endpoints()},
               {"charlie", network_links.second->endpoints()}}),
          network_emulation(), test::GetGlobalMetricsLogger()));
  RunParams run_params(TimeDelta::Seconds(2));
  run_params.enable_flex_fec_support = true;
  RunAndCheckEachVideoStreamReceivedFrames(run_params);
}
#endif

// IOS debug builds can be quite slow, disabling to avoid issues with timeouts.
#if defined(WEBRTC_IOS) && defined(WEBRTC_ARCH_ARM64) && !defined(NDEBUG)
#define MAYBE_ChangeNetworkConditions DISABLED_ChangeNetworkConditions
#else
#define MAYBE_ChangeNetworkConditions ChangeNetworkConditions
#endif
TEST_F(PeerConnectionE2EQualityTestSmokeTest, MAYBE_ChangeNetworkConditions) {
  NetworkEmulationManager::SimulatedNetworkNode alice_node =
      network_emulation()
          ->NodeBuilder()
          .config(BuiltInNetworkBehaviorConfig())
          .Build();
  NetworkEmulationManager::SimulatedNetworkNode bob_node =
      network_emulation()
          ->NodeBuilder()
          .config(BuiltInNetworkBehaviorConfig())
          .Build();

  EmulatedEndpoint* alice_endpoint =
      network_emulation()->CreateEndpoint(EmulatedEndpointConfig());
  EmulatedEndpoint* bob_endpoint =
      network_emulation()->CreateEndpoint(EmulatedEndpointConfig());

  network_emulation()->CreateRoute(alice_endpoint, {alice_node.node},
                                   bob_endpoint);
  network_emulation()->CreateRoute(bob_endpoint, {bob_node.node},
                                   alice_endpoint);

  EmulatedNetworkManagerInterface* alice_network =
      network_emulation()->CreateEmulatedNetworkManagerInterface(
          {alice_endpoint});
  EmulatedNetworkManagerInterface* bob_network =
      network_emulation()->CreateEmulatedNetworkManagerInterface(
          {bob_endpoint});

  AddPeer(alice_network, [](PeerConfigurer* alice) {
    VideoConfig video(160, 120, 15);
    video.stream_label = "alice-video";
    video.sync_group = "alice-media";
    alice->AddVideoConfig(std::move(video));
    alice->SetVideoCodecs(
        {VideoCodecConfig(cricket::kVp9CodecName, {{"profile-id", "0"}})});

    alice->SetUseFlexFEC(true);
    alice->SetUseUlpFEC(true);
    alice->SetVideoEncoderBitrateMultiplier(1.1);
  });
  AddPeer(bob_network, [](PeerConfigurer* bob) {
    bob->SetVideoCodecs(
        {VideoCodecConfig(cricket::kVp9CodecName, {{"profile-id", "0"}})});

    bob->SetUseFlexFEC(true);
    bob->SetUseUlpFEC(true);
    bob->SetVideoEncoderBitrateMultiplier(1.1);
  });
  fixture()->AddQualityMetricsReporter(
      std::make_unique<StatsBasedNetworkQualityMetricsReporter>(
          std::map<std::string, std::vector<EmulatedEndpoint*>>(
              {{"alice", alice_network->endpoints()},
               {"bob", bob_network->endpoints()}}),
          network_emulation(), test::GetGlobalMetricsLogger()));

  fixture()->ExecuteAt(TimeDelta::Seconds(1), [alice_node](TimeDelta) {
    BuiltInNetworkBehaviorConfig config;
    config.loss_percent = 5;
    alice_node.simulation->SetConfig(config);
  });

  RunParams run_params(TimeDelta::Seconds(2));
  run_params.enable_flex_fec_support = true;
  RunAndCheckEachVideoStreamReceivedFrames(run_params);
}

// IOS debug builds can be quite slow, disabling to avoid issues with timeouts.
#if defined(WEBRTC_IOS) && defined(WEBRTC_ARCH_ARM64) && !defined(NDEBUG)
#define MAYBE_Screenshare DISABLED_Screenshare
#else
#define MAYBE_Screenshare Screenshare
#endif
TEST_F(PeerConnectionE2EQualityTestSmokeTest, MAYBE_Screenshare) {
  std::pair<EmulatedNetworkManagerInterface*, EmulatedNetworkManagerInterface*>
      network_links = CreateNetwork();
  AddPeer(network_links.first, [](PeerConfigurer* alice) {
    VideoConfig screenshare(320, 180, 30);
    screenshare.stream_label = "alice-screenshare";
    screenshare.content_hint = VideoTrackInterface::ContentHint::kText;
    ScreenShareConfig screen_share_config =
        ScreenShareConfig(TimeDelta::Seconds(2));
    screen_share_config.scrolling_params =
        ScrollingParams{.duration = TimeDelta::Millis(1800)};
    auto screen_share_frame_generator =
        CreateScreenShareFrameGenerator(screenshare, screen_share_config);
    alice->AddVideoConfig(std::move(screenshare),
                          std::move(screen_share_frame_generator));
  });
  AddPeer(network_links.second, [](PeerConfigurer* bob) {});
  RunAndCheckEachVideoStreamReceivedFrames(RunParams(TimeDelta::Seconds(2)));
}

// IOS debug builds can be quite slow, disabling to avoid issues with timeouts.
#if defined(WEBRTC_IOS) && defined(WEBRTC_ARCH_ARM64) && !defined(NDEBUG)
#define MAYBE_Echo DISABLED_Echo
#else
#define MAYBE_Echo Echo
#endif
TEST_F(PeerConnectionE2EQualityTestSmokeTest, MAYBE_Echo) {
  std::pair<EmulatedNetworkManagerInterface*, EmulatedNetworkManagerInterface*>
      network_links = CreateNetwork();
  AddPeer(network_links.first, [](PeerConfigurer* alice) {
    AudioConfig audio;
    audio.stream_label = "alice-audio";
    audio.input_file_name =
        test::ResourcePath("pc_quality_smoke_test_alice_source", "wav");
    audio.sampling_frequency_in_hz = 48000;
    alice->SetAudioConfig(std::move(audio));
  });
  AddPeer(network_links.second, [](PeerConfigurer* bob) {
    AudioConfig audio;
    audio.stream_label = "bob-audio";
    audio.input_file_name =
        test::ResourcePath("pc_quality_smoke_test_bob_source", "wav");
    bob->SetAudioConfig(std::move(audio));
  });
  RunParams run_params(TimeDelta::Seconds(2));
  run_params.echo_emulation_config = EchoEmulationConfig();
  RunAndCheckEachVideoStreamReceivedFrames(run_params);
}

// IOS debug builds can be quite slow, disabling to avoid issues with timeouts.
#if defined(WEBRTC_IOS) && defined(WEBRTC_ARCH_ARM64) && !defined(NDEBUG)
#define MAYBE_Simulcast DISABLED_Simulcast
#else
#define MAYBE_Simulcast Simulcast
#endif
TEST_F(PeerConnectionE2EQualityTestSmokeTest, MAYBE_Simulcast) {
  std::pair<EmulatedNetworkManagerInterface*, EmulatedNetworkManagerInterface*>
      network_links = CreateNetwork();
  AddPeer(network_links.first, [](PeerConfigurer* alice) {
    VideoConfig simulcast(1280, 720, 15);
    simulcast.stream_label = "alice-simulcast";
    simulcast.simulcast_config = VideoSimulcastConfig(2);
    simulcast.emulated_sfu_config = EmulatedSFUConfig(0);
    alice->AddVideoConfig(std::move(simulcast));

    AudioConfig audio;
    audio.stream_label = "alice-audio";
    audio.input_file_name =
        test::ResourcePath("pc_quality_smoke_test_alice_source", "wav");
    alice->SetAudioConfig(std::move(audio));
  });
  AddPeer(network_links.second, [](PeerConfigurer* bob) {});
  RunParams run_params(TimeDelta::Seconds(2));
  RunAndCheckEachVideoStreamReceivedFrames(run_params);
}

// IOS debug builds can be quite slow, disabling to avoid issues with timeouts.
#if defined(WEBRTC_IOS) && defined(WEBRTC_ARCH_ARM64) && !defined(NDEBUG)
#define MAYBE_Svc DISABLED_Svc
#else
#define MAYBE_Svc Svc
#endif
TEST_F(PeerConnectionE2EQualityTestSmokeTest, MAYBE_Svc) {
  std::pair<EmulatedNetworkManagerInterface*, EmulatedNetworkManagerInterface*>
      network_links = CreateNetwork();
  AddPeer(network_links.first, [](PeerConfigurer* alice) {
    VideoConfig simulcast("alice-svc", 1280, 720, 15);
    // Because we have network with packets loss we can analyze only the
    // highest spatial layer in SVC mode.
    simulcast.simulcast_config = VideoSimulcastConfig(2);
    simulcast.emulated_sfu_config = EmulatedSFUConfig(1);
    alice->AddVideoConfig(std::move(simulcast));

    AudioConfig audio("alice-audio");
    audio.input_file_name =
        test::ResourcePath("pc_quality_smoke_test_alice_source", "wav");
    alice->SetAudioConfig(std::move(audio));
    alice->SetVideoCodecs({VideoCodecConfig(cricket::kVp9CodecName)});
  });
  AddPeer(network_links.second, [](PeerConfigurer* bob) {
    bob->SetVideoCodecs({VideoCodecConfig(cricket::kVp9CodecName)});
  });
  RunParams run_params(TimeDelta::Seconds(2));
  RunAndCheckEachVideoStreamReceivedFrames(run_params);
}

// IOS debug builds can be quite slow, disabling to avoid issues with timeouts.
#if defined(WEBRTC_IOS) && defined(WEBRTC_ARCH_ARM64) && !defined(NDEBUG)
#define MAYBE_HighBitrate DISABLED_HighBitrate
#else
#define MAYBE_HighBitrate HighBitrate
#endif
TEST_F(PeerConnectionE2EQualityTestSmokeTest, MAYBE_HighBitrate) {
  std::pair<EmulatedNetworkManagerInterface*, EmulatedNetworkManagerInterface*>
      network_links = CreateNetwork();
  AddPeer(network_links.first, [](PeerConfigurer* alice) {
    BitrateSettings bitrate_settings;
    bitrate_settings.start_bitrate_bps = 3'000'000;
    bitrate_settings.max_bitrate_bps = 3'000'000;
    alice->SetBitrateSettings(bitrate_settings);
    VideoConfig video(800, 600, 15);
    video.stream_label = "alice-video";
    RtpEncodingParameters encoding_parameters;
    encoding_parameters.min_bitrate_bps = 500'000;
    encoding_parameters.max_bitrate_bps = 3'000'000;
    video.encoding_params.push_back(std::move(encoding_parameters));
    alice->AddVideoConfig(std::move(video));

    AudioConfig audio;
    audio.stream_label = "alice-audio";
    audio.input_file_name =
        test::ResourcePath("pc_quality_smoke_test_alice_source", "wav");
    audio.sampling_frequency_in_hz = 48000;
    alice->SetAudioConfig(std::move(audio));
    alice->SetVideoCodecs(
        {VideoCodecConfig(cricket::kVp9CodecName, {{"profile-id", "0"}})});
  });
  AddPeer(network_links.second, [](PeerConfigurer* bob) {
    bob->SetVideoCodecs(
        {VideoCodecConfig(cricket::kVp9CodecName, {{"profile-id", "0"}})});
  });
  RunParams run_params(TimeDelta::Seconds(2));
  RunAndCheckEachVideoStreamReceivedFrames(run_params);
}

}  // namespace
}  // namespace webrtc_pc_e2e
}  // namespace webrtc
