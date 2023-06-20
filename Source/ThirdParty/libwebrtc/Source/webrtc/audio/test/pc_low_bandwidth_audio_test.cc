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
#include "absl/strings/string_view.h"
#include "api/test/create_network_emulation_manager.h"
#include "api/test/create_peerconnection_quality_test_fixture.h"
#include "api/test/metrics/chrome_perf_dashboard_metrics_exporter.h"
#include "api/test/metrics/global_metrics_logger_and_exporter.h"
#include "api/test/metrics/metrics_exporter.h"
#include "api/test/metrics/stdout_metrics_exporter.h"
#include "api/test/network_emulation_manager.h"
#include "api/test/pclf/media_configuration.h"
#include "api/test/pclf/media_quality_test_params.h"
#include "api/test/pclf/peer_configurer.h"
#include "api/test/peerconnection_quality_test_fixture.h"
#include "api/test/simulated_network.h"
#include "api/test/time_controller.h"
#include "call/simulated_network.h"
#include "test/gtest.h"
#include "test/pc/e2e/network_quality_metrics_reporter.h"
#include "test/testsupport/file_utils.h"

ABSL_DECLARE_FLAG(std::string, test_case_prefix);
ABSL_DECLARE_FLAG(int, sample_rate_hz);
ABSL_DECLARE_FLAG(bool, quick);

namespace webrtc {
namespace test {

using ::webrtc::webrtc_pc_e2e::AudioConfig;
using ::webrtc::webrtc_pc_e2e::PeerConfigurer;
using ::webrtc::webrtc_pc_e2e::RunParams;

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

std::unique_ptr<webrtc_pc_e2e::PeerConnectionE2EQualityTestFixture>
CreateTestFixture(absl::string_view test_case_name,
                  TimeController& time_controller,
                  std::pair<EmulatedNetworkManagerInterface*,
                            EmulatedNetworkManagerInterface*> network_links,
                  rtc::FunctionView<void(PeerConfigurer*)> alice_configurer,
                  rtc::FunctionView<void(PeerConfigurer*)> bob_configurer) {
  auto fixture = webrtc_pc_e2e::CreatePeerConnectionE2EQualityTestFixture(
      std::string(test_case_name), time_controller,
      /*audio_quality_analyzer=*/nullptr,
      /*video_quality_analyzer=*/nullptr);
  auto alice = std::make_unique<PeerConfigurer>(
      network_links.first->network_dependencies());
  auto bob = std::make_unique<PeerConfigurer>(
      network_links.second->network_dependencies());
  alice_configurer(alice.get());
  bob_configurer(bob.get());
  fixture->AddPeer(std::move(alice));
  fixture->AddPeer(std::move(bob));
  fixture->AddQualityMetricsReporter(
      std::make_unique<webrtc_pc_e2e::NetworkQualityMetricsReporter>(
          network_links.first, network_links.second,
          test::GetGlobalMetricsLogger()));
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
  std::vector<std::unique_ptr<MetricsExporter>> exporters;
  exporters.push_back(std::make_unique<StdoutMetricsExporter>());
  exporters.push_back(std::make_unique<ChromePerfDashboardMetricsExporter>(
      perf_results_output_file));
  EXPECT_TRUE(
      ExportPerfMetric(*GetGlobalMetricsLogger(), std::move(exporters)));

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
      network_emulation_manager->CreateEndpointPairWithTwoWayRoutes(
          BuiltInNetworkBehaviorConfig()),
      [](PeerConfigurer* alice) {
        AudioConfig audio;
        audio.stream_label = "alice-audio";
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
      network_emulation_manager->CreateEndpointPairWithTwoWayRoutes(config),
      [](PeerConfigurer* alice) {
        AudioConfig audio;
        audio.stream_label = "alice-audio";
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
