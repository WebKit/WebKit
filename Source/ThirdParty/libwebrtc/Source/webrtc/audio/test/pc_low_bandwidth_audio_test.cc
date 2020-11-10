/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "api/test/create_network_emulation_manager.h"
#include "api/test/create_peerconnection_quality_test_fixture.h"
#include "api/test/network_emulation_manager.h"
#include "api/test/peerconnection_quality_test_fixture.h"
#include "api/test/simulated_network.h"
#include "api/test/time_controller.h"
#include "call/simulated_network.h"
#include "test/gtest.h"
#include "test/pc/e2e/network_quality_metrics_reporter.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/perf_test.h"

ABSL_DECLARE_FLAG(std::string, test_case_prefix);
ABSL_DECLARE_FLAG(int, sample_rate_hz);
ABSL_DECLARE_FLAG(bool, quick);

namespace webrtc {
namespace test {

using PeerConfigurer =
    webrtc_pc_e2e::PeerConnectionE2EQualityTestFixture::PeerConfigurer;
using RunParams = webrtc_pc_e2e::PeerConnectionE2EQualityTestFixture::RunParams;
using AudioConfig =
    webrtc_pc_e2e::PeerConnectionE2EQualityTestFixture::AudioConfig;

namespace {

constexpr int kTestDurationMs = 5400;
constexpr int kQuickTestDurationMs = 100;

std::string GetMetricTestCaseName() {
  const ::testing::TestInfo* const test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();
  std::string test_case_prefix(absl::GetFlag(FLAGS_test_case_prefix));
  if (test_case_prefix.empty()) {
    return test_info->name();
  }
  return test_case_prefix + "_" + test_info->name();
}

std::pair<EmulatedNetworkManagerInterface*, EmulatedNetworkManagerInterface*>
CreateTwoNetworkLinks(NetworkEmulationManager* emulation,
                      const BuiltInNetworkBehaviorConfig& config) {
  auto* alice_node = emulation->CreateEmulatedNode(config);
  auto* bob_node = emulation->CreateEmulatedNode(config);

  auto* alice_endpoint = emulation->CreateEndpoint(EmulatedEndpointConfig());
  auto* bob_endpoint = emulation->CreateEndpoint(EmulatedEndpointConfig());

  emulation->CreateRoute(alice_endpoint, {alice_node}, bob_endpoint);
  emulation->CreateRoute(bob_endpoint, {bob_node}, alice_endpoint);

  return {
      emulation->CreateEmulatedNetworkManagerInterface({alice_endpoint}),
      emulation->CreateEmulatedNetworkManagerInterface({bob_endpoint}),
  };
}

std::unique_ptr<webrtc_pc_e2e::PeerConnectionE2EQualityTestFixture>
CreateTestFixture(const std::string& test_case_name,
                  TimeController& time_controller,
                  std::pair<EmulatedNetworkManagerInterface*,
                            EmulatedNetworkManagerInterface*> network_links,
                  rtc::FunctionView<void(PeerConfigurer*)> alice_configurer,
                  rtc::FunctionView<void(PeerConfigurer*)> bob_configurer) {
  auto fixture = webrtc_pc_e2e::CreatePeerConnectionE2EQualityTestFixture(
      test_case_name, time_controller, /*audio_quality_analyzer=*/nullptr,
      /*video_quality_analyzer=*/nullptr);
  fixture->AddPeer(network_links.first->network_thread(),
                   network_links.first->network_manager(), alice_configurer);
  fixture->AddPeer(network_links.second->network_thread(),
                   network_links.second->network_manager(), bob_configurer);
  fixture->AddQualityMetricsReporter(
      std::make_unique<webrtc_pc_e2e::NetworkQualityMetricsReporter>(
          network_links.first, network_links.second));
  return fixture;
}

std::string FileSampleRateSuffix() {
  return std::to_string(absl::GetFlag(FLAGS_sample_rate_hz) / 1000);
}

std::string AudioInputFile() {
  return test::ResourcePath("voice_engine/audio_tiny" + FileSampleRateSuffix(),
                            "wav");
}

std::string AudioOutputFile() {
  const ::testing::TestInfo* const test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();
  return webrtc::test::OutputPath() + "PCLowBandwidth_" + test_info->name() +
         "_" + FileSampleRateSuffix() + ".wav";
}

std::string PerfResultsOutputFile() {
  return webrtc::test::OutputPath() + "PCLowBandwidth_perf_" +
         FileSampleRateSuffix() + ".pb";
}

void LogTestResults() {
  std::string perf_results_output_file = PerfResultsOutputFile();
  EXPECT_TRUE(webrtc::test::WritePerfResults(perf_results_output_file));

  const ::testing::TestInfo* const test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();

  // Output information about the input and output audio files so that further
  // processing can be done by an external process.
  printf("TEST %s %s %s %s\n", test_info->name(), AudioInputFile().c_str(),
         AudioOutputFile().c_str(), perf_results_output_file.c_str());
}

}  // namespace

TEST(PCLowBandwidthAudioTest, PCGoodNetworkHighBitrate) {
  std::unique_ptr<NetworkEmulationManager> network_emulation_manager =
      CreateNetworkEmulationManager();
  auto fixture = CreateTestFixture(
      GetMetricTestCaseName(), *network_emulation_manager->time_controller(),
      CreateTwoNetworkLinks(network_emulation_manager.get(),
                            BuiltInNetworkBehaviorConfig()),
      [](PeerConfigurer* alice) {
        AudioConfig audio;
        audio.stream_label = "alice-audio";
        audio.mode = AudioConfig::Mode::kFile;
        audio.input_file_name = AudioInputFile();
        audio.output_dump_file_name = AudioOutputFile();
        audio.sampling_frequency_in_hz = absl::GetFlag(FLAGS_sample_rate_hz);
        alice->SetAudioConfig(std::move(audio));
      },
      [](PeerConfigurer* bob) {});
  fixture->Run(RunParams(TimeDelta::Millis(
      absl::GetFlag(FLAGS_quick) ? kQuickTestDurationMs : kTestDurationMs)));
  LogTestResults();
}

TEST(PCLowBandwidthAudioTest, PC40kbpsNetwork) {
  std::unique_ptr<NetworkEmulationManager> network_emulation_manager =
      CreateNetworkEmulationManager();
  BuiltInNetworkBehaviorConfig config;
  config.link_capacity_kbps = 40;
  config.queue_length_packets = 1500;
  config.queue_delay_ms = 400;
  config.loss_percent = 1;
  auto fixture = CreateTestFixture(
      GetMetricTestCaseName(), *network_emulation_manager->time_controller(),
      CreateTwoNetworkLinks(network_emulation_manager.get(), config),
      [](PeerConfigurer* alice) {
        AudioConfig audio;
        audio.stream_label = "alice-audio";
        audio.mode = AudioConfig::Mode::kFile;
        audio.input_file_name = AudioInputFile();
        audio.output_dump_file_name = AudioOutputFile();
        audio.sampling_frequency_in_hz = absl::GetFlag(FLAGS_sample_rate_hz);
        alice->SetAudioConfig(std::move(audio));
      },
      [](PeerConfigurer* bob) {});
  fixture->Run(RunParams(TimeDelta::Millis(
      absl::GetFlag(FLAGS_quick) ? kQuickTestDurationMs : kTestDurationMs)));
  LogTestResults();
}

}  // namespace test
}  // namespace webrtc
