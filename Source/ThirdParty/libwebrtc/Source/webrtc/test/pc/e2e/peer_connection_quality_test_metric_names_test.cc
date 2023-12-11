/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <map>
#include <memory>
#include <string>

#include "api/test/create_network_emulation_manager.h"
#include "api/test/create_peer_connection_quality_test_frame_generator.h"
#include "api/test/metrics/metrics_logger.h"
#include "api/test/metrics/stdout_metrics_exporter.h"
#include "api/test/network_emulation_manager.h"
#include "api/test/pclf/media_configuration.h"
#include "api/test/pclf/media_quality_test_params.h"
#include "api/test/pclf/peer_configurer.h"
#include "api/test/peerconnection_quality_test_fixture.h"
#include "api/units/time_delta.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/pc/e2e/metric_metadata_keys.h"
#include "test/pc/e2e/network_quality_metrics_reporter.h"
#include "test/pc/e2e/peer_connection_quality_test.h"
#include "test/pc/e2e/stats_based_network_quality_metrics_reporter.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

using ::testing::IsSupersetOf;
using ::testing::UnorderedElementsAre;

using ::webrtc::test::DefaultMetricsLogger;
using ::webrtc::test::ImprovementDirection;
using ::webrtc::test::Metric;
using ::webrtc::test::MetricsExporter;
using ::webrtc::test::StdoutMetricsExporter;
using ::webrtc::test::Unit;
using ::webrtc::webrtc_pc_e2e::PeerConfigurer;

// Adds a peer with some audio and video (the client should not care about
// details about audio and video configs).
void AddDefaultAudioVideoPeer(
    absl::string_view peer_name,
    absl::string_view audio_stream_label,
    absl::string_view video_stream_label,
    const PeerNetworkDependencies& network_dependencies,
    PeerConnectionE2EQualityTestFixture& fixture) {
  AudioConfig audio{std::string(audio_stream_label)};
  audio.sync_group = std::string(peer_name);
  VideoConfig video(std::string(video_stream_label), 320, 180, 15);
  video.sync_group = std::string(peer_name);
  auto peer = std::make_unique<PeerConfigurer>(network_dependencies);
  peer->SetName(peer_name);
  peer->SetAudioConfig(std::move(audio));
  peer->AddVideoConfig(std::move(video));
  peer->SetVideoCodecs({VideoCodecConfig(cricket::kVp8CodecName)});
  fixture.AddPeer(std::move(peer));
}

// Metric fields to assert on
struct MetricValidationInfo {
  std::string test_case;
  std::string name;
  Unit unit;
  ImprovementDirection improvement_direction;
  std::map<std::string, std::string> metadata;
};

bool operator==(const MetricValidationInfo& a, const MetricValidationInfo& b) {
  return a.name == b.name && a.test_case == b.test_case && a.unit == b.unit &&
         a.improvement_direction == b.improvement_direction &&
         a.metadata == b.metadata;
}

std::ostream& operator<<(std::ostream& os, const MetricValidationInfo& m) {
  os << "{ test_case=" << m.test_case << "; name=" << m.name
     << "; unit=" << test::ToString(m.unit)
     << "; improvement_direction=" << test::ToString(m.improvement_direction)
     << "; metadata={ ";
  for (const auto& [key, value] : m.metadata) {
    os << "{ key=" << key << "; value=" << value << " }";
  }
  os << " }}";
  return os;
}

std::vector<MetricValidationInfo> ToValidationInfo(
    const std::vector<Metric>& metrics) {
  std::vector<MetricValidationInfo> out;
  for (const Metric& m : metrics) {
    out.push_back(
        MetricValidationInfo{.test_case = m.test_case,
                             .name = m.name,
                             .unit = m.unit,
                             .improvement_direction = m.improvement_direction,
                             .metadata = m.metric_metadata});
  }
  return out;
}

TEST(PeerConnectionE2EQualityTestMetricNamesTest,
     ExportedMetricsHasCorrectNamesAndAnnotation) {
  std::unique_ptr<NetworkEmulationManager> network_emulation =
      CreateNetworkEmulationManager(TimeMode::kSimulated);
  DefaultMetricsLogger metrics_logger(
      network_emulation->time_controller()->GetClock());
  PeerConnectionE2EQualityTest fixture(
      "test_case", *network_emulation->time_controller(),
      /*audio_quality_analyzer=*/nullptr, /*video_quality_analyzer=*/nullptr,
      &metrics_logger);

  EmulatedEndpoint* alice_endpoint =
      network_emulation->CreateEndpoint(EmulatedEndpointConfig());
  EmulatedEndpoint* bob_endpoint =
      network_emulation->CreateEndpoint(EmulatedEndpointConfig());

  network_emulation->CreateRoute(
      alice_endpoint, {network_emulation->CreateUnconstrainedEmulatedNode()},
      bob_endpoint);
  network_emulation->CreateRoute(
      bob_endpoint, {network_emulation->CreateUnconstrainedEmulatedNode()},
      alice_endpoint);

  EmulatedNetworkManagerInterface* alice_network =
      network_emulation->CreateEmulatedNetworkManagerInterface(
          {alice_endpoint});
  EmulatedNetworkManagerInterface* bob_network =
      network_emulation->CreateEmulatedNetworkManagerInterface({bob_endpoint});

  AddDefaultAudioVideoPeer("alice", "alice_audio", "alice_video",
                           alice_network->network_dependencies(), fixture);
  AddDefaultAudioVideoPeer("bob", "bob_audio", "bob_video",
                           bob_network->network_dependencies(), fixture);
  fixture.AddQualityMetricsReporter(
      std::make_unique<StatsBasedNetworkQualityMetricsReporter>(
          std::map<std::string, std::vector<EmulatedEndpoint*>>(
              {{"alice", alice_network->endpoints()},
               {"bob", bob_network->endpoints()}}),
          network_emulation.get(), &metrics_logger));

  // Run for at least 7 seconds, so AV-sync metrics will be collected.
  fixture.Run(RunParams(TimeDelta::Seconds(7)));

  std::vector<MetricValidationInfo> metrics =
      ToValidationInfo(metrics_logger.GetCollectedMetrics());
  EXPECT_THAT(
      metrics,
      UnorderedElementsAre(
          // Metrics from PeerConnectionE2EQualityTest
          MetricValidationInfo{
              .test_case = "test_case",
              .name = "alice_connected",
              .unit = Unit::kUnitless,
              .improvement_direction = ImprovementDirection::kBiggerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case",
              .name = "bob_connected",
              .unit = Unit::kUnitless,
              .improvement_direction = ImprovementDirection::kBiggerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},

          // Metrics from DefaultAudioQualityAnalyzer
          MetricValidationInfo{
              .test_case = "test_case/alice_audio",
              .name = "expand_rate",
              .unit = Unit::kUnitless,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata = {{MetricMetadataKey::kAudioStreamMetadataKey,
                            "alice_audio"},
                           {MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kReceiverMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice_audio",
              .name = "accelerate_rate",
              .unit = Unit::kUnitless,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata = {{MetricMetadataKey::kAudioStreamMetadataKey,
                            "alice_audio"},
                           {MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kReceiverMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice_audio",
              .name = "preemptive_rate",
              .unit = Unit::kUnitless,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata = {{MetricMetadataKey::kAudioStreamMetadataKey,
                            "alice_audio"},
                           {MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kReceiverMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice_audio",
              .name = "speech_expand_rate",
              .unit = Unit::kUnitless,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata = {{MetricMetadataKey::kAudioStreamMetadataKey,
                            "alice_audio"},
                           {MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kReceiverMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice_audio",
              .name = "average_jitter_buffer_delay_ms",
              .unit = Unit::kMilliseconds,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kAudioStreamMetadataKey,
                            "alice_audio"},
                           {MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kReceiverMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice_audio",
              .name = "preferred_buffer_size_ms",
              .unit = Unit::kMilliseconds,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kAudioStreamMetadataKey,
                            "alice_audio"},
                           {MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kReceiverMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob_audio",
              .name = "expand_rate",
              .unit = Unit::kUnitless,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata = {{MetricMetadataKey::kAudioStreamMetadataKey,
                            "bob_audio"},
                           {MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kReceiverMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob_audio",
              .name = "accelerate_rate",
              .unit = Unit::kUnitless,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata = {{MetricMetadataKey::kAudioStreamMetadataKey,
                            "bob_audio"},
                           {MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kReceiverMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob_audio",
              .name = "preemptive_rate",
              .unit = Unit::kUnitless,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata = {{MetricMetadataKey::kAudioStreamMetadataKey,
                            "bob_audio"},
                           {MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kReceiverMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob_audio",
              .name = "speech_expand_rate",
              .unit = Unit::kUnitless,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata = {{MetricMetadataKey::kAudioStreamMetadataKey,
                            "bob_audio"},
                           {MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kReceiverMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob_audio",
              .name = "average_jitter_buffer_delay_ms",
              .unit = Unit::kMilliseconds,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kAudioStreamMetadataKey,
                            "bob_audio"},
                           {MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kReceiverMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob_audio",
              .name = "preferred_buffer_size_ms",
              .unit = Unit::kMilliseconds,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kAudioStreamMetadataKey,
                            "bob_audio"},
                           {MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kReceiverMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},

          // Metrics from DefaultVideoQualityAnalyzer
          MetricValidationInfo{
              .test_case = "test_case/alice_video",
              .name = "psnr_dB",
              .unit = Unit::kUnitless,
              .improvement_direction = ImprovementDirection::kBiggerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "alice_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "alice"},
                           {MetricMetadataKey::kReceiverMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice_video",
              .name = "ssim",
              .unit = Unit::kUnitless,
              .improvement_direction = ImprovementDirection::kBiggerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "alice_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "alice"},
                           {MetricMetadataKey::kReceiverMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice_video",
              .name = "transport_time",
              .unit = Unit::kMilliseconds,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "alice_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "alice"},
                           {MetricMetadataKey::kReceiverMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice_video",
              .name = "total_delay_incl_transport",
              .unit = Unit::kMilliseconds,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "alice_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "alice"},
                           {MetricMetadataKey::kReceiverMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice_video",
              .name = "time_between_rendered_frames",
              .unit = Unit::kMilliseconds,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "alice_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "alice"},
                           {MetricMetadataKey::kReceiverMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice_video",
              .name = "harmonic_framerate",
              .unit = Unit::kHertz,
              .improvement_direction = ImprovementDirection::kBiggerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "alice_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "alice"},
                           {MetricMetadataKey::kReceiverMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice_video",
              .name = "encode_frame_rate",
              .unit = Unit::kHertz,
              .improvement_direction = ImprovementDirection::kBiggerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "alice_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "alice"},
                           {MetricMetadataKey::kReceiverMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice_video",
              .name = "encode_time",
              .unit = Unit::kMilliseconds,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "alice_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "alice"},
                           {MetricMetadataKey::kReceiverMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice_video",
              .name = "time_between_freezes",
              .unit = Unit::kMilliseconds,
              .improvement_direction = ImprovementDirection::kBiggerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "alice_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "alice"},
                           {MetricMetadataKey::kReceiverMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice_video",
              .name = "freeze_time_ms",
              .unit = Unit::kMilliseconds,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "alice_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "alice"},
                           {MetricMetadataKey::kReceiverMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice_video",
              .name = "pixels_per_frame",
              .unit = Unit::kCount,
              .improvement_direction = ImprovementDirection::kBiggerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "alice_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "alice"},
                           {MetricMetadataKey::kReceiverMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice_video",
              .name = "min_psnr_dB",
              .unit = Unit::kUnitless,
              .improvement_direction = ImprovementDirection::kBiggerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "alice_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "alice"},
                           {MetricMetadataKey::kReceiverMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice_video",
              .name = "decode_time",
              .unit = Unit::kMilliseconds,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "alice_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "alice"},
                           {MetricMetadataKey::kReceiverMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice_video",
              .name = "receive_to_render_time",
              .unit = Unit::kMilliseconds,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "alice_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "alice"},
                           {MetricMetadataKey::kReceiverMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice_video",
              .name = "dropped_frames",
              .unit = Unit::kCount,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "alice_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "alice"},
                           {MetricMetadataKey::kReceiverMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice_video",
              .name = "frames_in_flight",
              .unit = Unit::kCount,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "alice_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "alice"},
                           {MetricMetadataKey::kReceiverMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice_video",
              .name = "rendered_frames",
              .unit = Unit::kCount,
              .improvement_direction = ImprovementDirection::kBiggerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "alice_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "alice"},
                           {MetricMetadataKey::kReceiverMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice_video",
              .name = "max_skipped",
              .unit = Unit::kCount,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "alice_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "alice"},
                           {MetricMetadataKey::kReceiverMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice_video",
              .name = "target_encode_bitrate",
              .unit = Unit::kKilobitsPerSecond,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "alice_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "alice"},
                           {MetricMetadataKey::kReceiverMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice_video",
              .name = "qp_sl0",
              .unit = Unit::kUnitless,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "alice_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "alice"},
                           {MetricMetadataKey::kReceiverMetadataKey, "bob"},
                           {MetricMetadataKey::kSpatialLayerMetadataKey, "0"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice_video",
              .name = "actual_encode_bitrate",
              .unit = Unit::kKilobitsPerSecond,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "alice_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "alice"},
                           {MetricMetadataKey::kReceiverMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob_video",
              .name = "psnr_dB",
              .unit = Unit::kUnitless,
              .improvement_direction = ImprovementDirection::kBiggerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "bob_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "bob"},
                           {MetricMetadataKey::kReceiverMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob_video",
              .name = "ssim",
              .unit = Unit::kUnitless,
              .improvement_direction = ImprovementDirection::kBiggerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "bob_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "bob"},
                           {MetricMetadataKey::kReceiverMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob_video",
              .name = "transport_time",
              .unit = Unit::kMilliseconds,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "bob_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "bob"},
                           {MetricMetadataKey::kReceiverMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob_video",
              .name = "total_delay_incl_transport",
              .unit = Unit::kMilliseconds,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "bob_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "bob"},
                           {MetricMetadataKey::kReceiverMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob_video",
              .name = "time_between_rendered_frames",
              .unit = Unit::kMilliseconds,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "bob_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "bob"},
                           {MetricMetadataKey::kReceiverMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob_video",
              .name = "harmonic_framerate",
              .unit = Unit::kHertz,
              .improvement_direction = ImprovementDirection::kBiggerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "bob_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "bob"},
                           {MetricMetadataKey::kReceiverMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob_video",
              .name = "encode_frame_rate",
              .unit = Unit::kHertz,
              .improvement_direction = ImprovementDirection::kBiggerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "bob_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "bob"},
                           {MetricMetadataKey::kReceiverMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob_video",
              .name = "encode_time",
              .unit = Unit::kMilliseconds,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "bob_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "bob"},
                           {MetricMetadataKey::kReceiverMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob_video",
              .name = "time_between_freezes",
              .unit = Unit::kMilliseconds,
              .improvement_direction = ImprovementDirection::kBiggerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "bob_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "bob"},
                           {MetricMetadataKey::kReceiverMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob_video",
              .name = "freeze_time_ms",
              .unit = Unit::kMilliseconds,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "bob_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "bob"},
                           {MetricMetadataKey::kReceiverMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob_video",
              .name = "pixels_per_frame",
              .unit = Unit::kCount,
              .improvement_direction = ImprovementDirection::kBiggerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "bob_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "bob"},
                           {MetricMetadataKey::kReceiverMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob_video",
              .name = "min_psnr_dB",
              .unit = Unit::kUnitless,
              .improvement_direction = ImprovementDirection::kBiggerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "bob_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "bob"},
                           {MetricMetadataKey::kReceiverMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob_video",
              .name = "decode_time",
              .unit = Unit::kMilliseconds,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "bob_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "bob"},
                           {MetricMetadataKey::kReceiverMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob_video",
              .name = "receive_to_render_time",
              .unit = Unit::kMilliseconds,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "bob_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "bob"},
                           {MetricMetadataKey::kReceiverMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob_video",
              .name = "dropped_frames",
              .unit = Unit::kCount,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "bob_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "bob"},
                           {MetricMetadataKey::kReceiverMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob_video",
              .name = "frames_in_flight",
              .unit = Unit::kCount,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "bob_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "bob"},
                           {MetricMetadataKey::kReceiverMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob_video",
              .name = "rendered_frames",
              .unit = Unit::kCount,
              .improvement_direction = ImprovementDirection::kBiggerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "bob_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "bob"},
                           {MetricMetadataKey::kReceiverMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob_video",
              .name = "max_skipped",
              .unit = Unit::kCount,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "bob_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "bob"},
                           {MetricMetadataKey::kReceiverMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob_video",
              .name = "target_encode_bitrate",
              .unit = Unit::kKilobitsPerSecond,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "bob_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "bob"},
                           {MetricMetadataKey::kReceiverMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob_video",
              .name = "actual_encode_bitrate",
              .unit = Unit::kKilobitsPerSecond,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "bob_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "bob"},
                           {MetricMetadataKey::kReceiverMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob_video",
              .name = "qp_sl0",
              .unit = Unit::kUnitless,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kVideoStreamMetadataKey,
                            "bob_video"},
                           {MetricMetadataKey::kSenderMetadataKey, "bob"},
                           {MetricMetadataKey::kReceiverMetadataKey, "alice"},
                           {MetricMetadataKey::kSpatialLayerMetadataKey, "0"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case",
              .name = "cpu_usage_%",
              .unit = Unit::kUnitless,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata = {{MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},

          // Metrics from StatsBasedNetworkQualityMetricsReporter
          MetricValidationInfo{
              .test_case = "test_case/alice",
              .name = "bytes_discarded_no_receiver",
              .unit = Unit::kBytes,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice",
              .name = "packets_discarded_no_receiver",
              .unit = Unit::kUnitless,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice",
              .name = "payload_bytes_received",
              .unit = Unit::kBytes,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice",
              .name = "payload_bytes_sent",
              .unit = Unit::kBytes,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice",
              .name = "bytes_sent",
              .unit = Unit::kBytes,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice",
              .name = "packets_sent",
              .unit = Unit::kUnitless,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice",
              .name = "average_send_rate",
              .unit = Unit::kKilobitsPerSecond,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice",
              .name = "bytes_received",
              .unit = Unit::kBytes,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice",
              .name = "packets_received",
              .unit = Unit::kUnitless,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice",
              .name = "average_receive_rate",
              .unit = Unit::kKilobitsPerSecond,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice",
              .name = "sent_packets_loss",
              .unit = Unit::kUnitless,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob",
              .name = "bytes_discarded_no_receiver",
              .unit = Unit::kBytes,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob",
              .name = "packets_discarded_no_receiver",
              .unit = Unit::kUnitless,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob",
              .name = "payload_bytes_received",
              .unit = Unit::kBytes,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob",
              .name = "payload_bytes_sent",
              .unit = Unit::kBytes,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob",
              .name = "bytes_sent",
              .unit = Unit::kBytes,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob",
              .name = "packets_sent",
              .unit = Unit::kUnitless,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob",
              .name = "average_send_rate",
              .unit = Unit::kKilobitsPerSecond,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob",
              .name = "bytes_received",
              .unit = Unit::kBytes,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob",
              .name = "packets_received",
              .unit = Unit::kUnitless,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob",
              .name = "average_receive_rate",
              .unit = Unit::kKilobitsPerSecond,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob",
              .name = "sent_packets_loss",
              .unit = Unit::kUnitless,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},

          // Metrics from VideoQualityMetricsReporter
          MetricValidationInfo{
              .test_case = "test_case/alice",
              .name = "available_send_bandwidth",
              .unit = Unit::kKilobitsPerSecond,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice",
              .name = "transmission_bitrate",
              .unit = Unit::kKilobitsPerSecond,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice",
              .name = "retransmission_bitrate",
              .unit = Unit::kKilobitsPerSecond,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "alice"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob",
              .name = "available_send_bandwidth",
              .unit = Unit::kKilobitsPerSecond,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob",
              .name = "transmission_bitrate",
              .unit = Unit::kKilobitsPerSecond,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob",
              .name = "retransmission_bitrate",
              .unit = Unit::kKilobitsPerSecond,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},

          // Metrics from CrossMediaMetricsReporter
          MetricValidationInfo{
              .test_case = "test_case/alice_alice_audio",
              .name = "audio_ahead_ms",
              .unit = Unit::kMilliseconds,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata =
                  {{MetricMetadataKey::kAudioStreamMetadataKey, "alice_audio"},
                   {MetricMetadataKey::kPeerMetadataKey, "bob"},
                   {MetricMetadataKey::kPeerSyncGroupMetadataKey, "alice"},
                   {MetricMetadataKey::kReceiverMetadataKey, "bob"},
                   {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                    "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice_alice_video",
              .name = "video_ahead_ms",
              .unit = Unit::kMilliseconds,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata =
                  {{MetricMetadataKey::kAudioStreamMetadataKey, "alice_video"},
                   {MetricMetadataKey::kPeerMetadataKey, "bob"},
                   {MetricMetadataKey::kPeerSyncGroupMetadataKey, "alice"},
                   {MetricMetadataKey::kReceiverMetadataKey, "bob"},
                   {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                    "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob_bob_audio",
              .name = "audio_ahead_ms",
              .unit = Unit::kMilliseconds,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata =
                  {{MetricMetadataKey::kAudioStreamMetadataKey, "bob_audio"},
                   {MetricMetadataKey::kPeerMetadataKey, "alice"},
                   {MetricMetadataKey::kPeerSyncGroupMetadataKey, "bob"},
                   {MetricMetadataKey::kReceiverMetadataKey, "alice"},
                   {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                    "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob_bob_video",
              .name = "video_ahead_ms",
              .unit = Unit::kMilliseconds,
              .improvement_direction = ImprovementDirection::kSmallerIsBetter,
              .metadata =
                  {{MetricMetadataKey::kAudioStreamMetadataKey, "bob_video"},
                   {MetricMetadataKey::kPeerMetadataKey, "alice"},
                   {MetricMetadataKey::kPeerSyncGroupMetadataKey, "bob"},
                   {MetricMetadataKey::kReceiverMetadataKey, "alice"},
                   {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                    "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/alice_audio",
              .name = "energy",
              .unit = Unit::kUnitless,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {{MetricMetadataKey::kAudioStreamMetadataKey,
                            "alice_audio"},
                           {MetricMetadataKey::kPeerMetadataKey, "bob"},
                           {MetricMetadataKey::kReceiverMetadataKey, "bob"},
                           {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                            "test_case"}}},
          MetricValidationInfo{
              .test_case = "test_case/bob_audio",
              .name = "energy",
              .unit = Unit::kUnitless,
              .improvement_direction = ImprovementDirection::kNeitherIsBetter,
              .metadata = {
                  {MetricMetadataKey::kAudioStreamMetadataKey, "bob_audio"},
                  {MetricMetadataKey::kPeerMetadataKey, "alice"},
                  {MetricMetadataKey::kReceiverMetadataKey, "alice"},
                  {MetricMetadataKey::kExperimentalTestNameMetadataKey,
                   "test_case"}}}));
}

TEST(PeerConnectionE2EQualityTestMetricNamesTest,
     ExportedNetworkMetricsHaveCustomNetworkLabelIfSet) {
  std::unique_ptr<NetworkEmulationManager> network_emulation =
      CreateNetworkEmulationManager(TimeMode::kSimulated);
  DefaultMetricsLogger metrics_logger(
      network_emulation->time_controller()->GetClock());
  PeerConnectionE2EQualityTest fixture(
      "test_case", *network_emulation->time_controller(),
      /*audio_quality_analyzer=*/nullptr, /*video_quality_analyzer=*/nullptr,
      &metrics_logger);

  EmulatedEndpoint* alice_endpoint =
      network_emulation->CreateEndpoint(EmulatedEndpointConfig());
  EmulatedEndpoint* bob_endpoint =
      network_emulation->CreateEndpoint(EmulatedEndpointConfig());

  network_emulation->CreateRoute(
      alice_endpoint, {network_emulation->CreateUnconstrainedEmulatedNode()},
      bob_endpoint);
  network_emulation->CreateRoute(
      bob_endpoint, {network_emulation->CreateUnconstrainedEmulatedNode()},
      alice_endpoint);

  EmulatedNetworkManagerInterface* alice_network =
      network_emulation->CreateEmulatedNetworkManagerInterface(
          {alice_endpoint});
  EmulatedNetworkManagerInterface* bob_network =
      network_emulation->CreateEmulatedNetworkManagerInterface({bob_endpoint});

  AddDefaultAudioVideoPeer("alice", "alice_audio", "alice_video",
                           alice_network->network_dependencies(), fixture);
  AddDefaultAudioVideoPeer("bob", "bob_audio", "bob_video",
                           bob_network->network_dependencies(), fixture);
  std::string kAliceNetworkLabel = "alice_label";
  std::string kBobNetworkLabel = "bob_label";
  fixture.AddQualityMetricsReporter(
      std::make_unique<NetworkQualityMetricsReporter>(
          kAliceNetworkLabel, alice_network, kBobNetworkLabel, bob_network,
          &metrics_logger));

  fixture.Run(RunParams(TimeDelta::Seconds(1)));

  std::vector<MetricValidationInfo> metrics =
      ToValidationInfo(metrics_logger.GetCollectedMetrics());

  EXPECT_THAT(metrics,
              IsSupersetOf(
                  // Metrics from PeerConnectionE2EQualityTest
                  std::vector<MetricValidationInfo>{
                      MetricValidationInfo{
                          .test_case = "test_case/" + kAliceNetworkLabel,
                          .name = "bytes_discarded_no_receiver",
                          .unit = Unit::kBytes,
                          .improvement_direction =
                              ImprovementDirection::kNeitherIsBetter,
                      },
                      MetricValidationInfo{
                          .test_case = "test_case/" + kAliceNetworkLabel,
                          .name = "packets_discarded_no_receiver",
                          .unit = Unit::kUnitless,
                          .improvement_direction =
                              ImprovementDirection::kNeitherIsBetter,
                      },
                      MetricValidationInfo{
                          .test_case = "test_case/" + kAliceNetworkLabel,
                          .name = "bytes_sent",
                          .unit = Unit::kBytes,
                          .improvement_direction =
                              ImprovementDirection::kNeitherIsBetter,
                      },
                      MetricValidationInfo{
                          .test_case = "test_case/" + kAliceNetworkLabel,
                          .name = "packets_sent",
                          .unit = Unit::kUnitless,
                          .improvement_direction =
                              ImprovementDirection::kNeitherIsBetter,
                      },
                      MetricValidationInfo{
                          .test_case = "test_case/" + kAliceNetworkLabel,
                          .name = "average_send_rate",
                          .unit = Unit::kKilobitsPerSecond,
                          .improvement_direction =
                              ImprovementDirection::kNeitherIsBetter,
                      },
                      MetricValidationInfo{
                          .test_case = "test_case/" + kAliceNetworkLabel,
                          .name = "bytes_received",
                          .unit = Unit::kBytes,
                          .improvement_direction =
                              ImprovementDirection::kNeitherIsBetter,
                      },
                      MetricValidationInfo{
                          .test_case = "test_case/" + kAliceNetworkLabel,
                          .name = "packets_received",
                          .unit = Unit::kUnitless,
                          .improvement_direction =
                              ImprovementDirection::kNeitherIsBetter,
                      },
                      MetricValidationInfo{
                          .test_case = "test_case/" + kAliceNetworkLabel,
                          .name = "average_receive_rate",
                          .unit = Unit::kKilobitsPerSecond,
                          .improvement_direction =
                              ImprovementDirection::kNeitherIsBetter,
                      },
                      MetricValidationInfo{
                          .test_case = "test_case/" + kAliceNetworkLabel,
                          .name = "sent_packets_loss",
                          .unit = Unit::kUnitless,
                          .improvement_direction =
                              ImprovementDirection::kNeitherIsBetter,
                      },
                      MetricValidationInfo{
                          .test_case = "test_case/" + kBobNetworkLabel,
                          .name = "bytes_discarded_no_receiver",
                          .unit = Unit::kBytes,
                          .improvement_direction =
                              ImprovementDirection::kNeitherIsBetter,
                      },
                      MetricValidationInfo{
                          .test_case = "test_case/" + kBobNetworkLabel,
                          .name = "packets_discarded_no_receiver",
                          .unit = Unit::kUnitless,
                          .improvement_direction =
                              ImprovementDirection::kNeitherIsBetter,
                      },
                      MetricValidationInfo{
                          .test_case = "test_case/" + kBobNetworkLabel,
                          .name = "bytes_sent",
                          .unit = Unit::kBytes,
                          .improvement_direction =
                              ImprovementDirection::kNeitherIsBetter,
                      },
                      MetricValidationInfo{
                          .test_case = "test_case/" + kBobNetworkLabel,
                          .name = "packets_sent",
                          .unit = Unit::kUnitless,
                          .improvement_direction =
                              ImprovementDirection::kNeitherIsBetter,
                      },
                      MetricValidationInfo{
                          .test_case = "test_case/" + kBobNetworkLabel,
                          .name = "average_send_rate",
                          .unit = Unit::kKilobitsPerSecond,
                          .improvement_direction =
                              ImprovementDirection::kNeitherIsBetter,
                      },
                      MetricValidationInfo{
                          .test_case = "test_case/" + kBobNetworkLabel,
                          .name = "bytes_received",
                          .unit = Unit::kBytes,
                          .improvement_direction =
                              ImprovementDirection::kNeitherIsBetter,
                      },
                      MetricValidationInfo{
                          .test_case = "test_case/" + kBobNetworkLabel,
                          .name = "packets_received",
                          .unit = Unit::kUnitless,
                          .improvement_direction =
                              ImprovementDirection::kNeitherIsBetter,
                      },
                      MetricValidationInfo{
                          .test_case = "test_case/" + kBobNetworkLabel,
                          .name = "average_receive_rate",
                          .unit = Unit::kKilobitsPerSecond,
                          .improvement_direction =
                              ImprovementDirection::kNeitherIsBetter,
                      },
                      MetricValidationInfo{
                          .test_case = "test_case/" + kBobNetworkLabel,
                          .name = "sent_packets_loss",
                          .unit = Unit::kUnitless,
                          .improvement_direction =
                              ImprovementDirection::kNeitherIsBetter,
                      }}));
}

}  // namespace
}  // namespace webrtc_pc_e2e
}  // namespace webrtc
