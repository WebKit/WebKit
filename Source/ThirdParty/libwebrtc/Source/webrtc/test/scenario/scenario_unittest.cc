/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/scenario/scenario.h"

#include <atomic>

#include "api/test/network_emulation/create_cross_traffic.h"
#include "api/test/network_emulation/cross_traffic.h"
#include "test/field_trial.h"
#include "test/gtest.h"
#include "test/logging/memory_log_writer.h"
#include "test/scenario/stats_collection.h"

namespace webrtc {
namespace test {
TEST(ScenarioTest, StartsAndStopsWithoutErrors) {
  std::atomic<bool> packet_received(false);
  std::atomic<bool> bitrate_changed(false);
  Scenario s;
  CallClientConfig call_client_config;
  call_client_config.transport.rates.start_rate = DataRate::KilobitsPerSec(300);
  auto* alice = s.CreateClient("alice", call_client_config);
  auto* bob = s.CreateClient("bob", call_client_config);
  NetworkSimulationConfig network_config;
  auto alice_net = s.CreateSimulationNode(network_config);
  auto bob_net = s.CreateSimulationNode(network_config);
  auto route = s.CreateRoutes(alice, {alice_net}, bob, {bob_net});

  VideoStreamConfig video_stream_config;
  s.CreateVideoStream(route->forward(), video_stream_config);
  s.CreateVideoStream(route->reverse(), video_stream_config);

  AudioStreamConfig audio_stream_config;
  audio_stream_config.encoder.min_rate = DataRate::KilobitsPerSec(6);
  audio_stream_config.encoder.max_rate = DataRate::KilobitsPerSec(64);
  audio_stream_config.encoder.allocate_bitrate = true;
  audio_stream_config.stream.in_bandwidth_estimation = false;
  s.CreateAudioStream(route->forward(), audio_stream_config);
  s.CreateAudioStream(route->reverse(), audio_stream_config);

  RandomWalkConfig cross_traffic_config;
  s.net()->StartCrossTraffic(CreateRandomWalkCrossTraffic(
      s.net()->CreateCrossTrafficRoute({alice_net}), cross_traffic_config));

  s.NetworkDelayedAction({alice_net, bob_net}, 100,
                         [&packet_received] { packet_received = true; });
  s.Every(TimeDelta::Millis(10), [alice, bob, &bitrate_changed] {
    if (alice->GetStats().send_bandwidth_bps != 300000 &&
        bob->GetStats().send_bandwidth_bps != 300000)
      bitrate_changed = true;
  });
  s.RunUntil(TimeDelta::Seconds(2), TimeDelta::Millis(5),
             [&bitrate_changed, &packet_received] {
               return packet_received && bitrate_changed;
             });
  EXPECT_TRUE(packet_received);
  EXPECT_TRUE(bitrate_changed);
}
namespace {
void SetupVideoCall(Scenario& s, VideoQualityAnalyzer* analyzer) {
  CallClientConfig call_config;
  auto* alice = s.CreateClient("alice", call_config);
  auto* bob = s.CreateClient("bob", call_config);
  NetworkSimulationConfig network_config;
  network_config.bandwidth = DataRate::KilobitsPerSec(1000);
  network_config.delay = TimeDelta::Millis(50);
  auto alice_net = s.CreateSimulationNode(network_config);
  auto bob_net = s.CreateSimulationNode(network_config);
  auto route = s.CreateRoutes(alice, {alice_net}, bob, {bob_net});
  VideoStreamConfig video;
  if (analyzer) {
    video.source.capture = VideoStreamConfig::Source::Capture::kVideoFile;
    video.source.video_file.name = "foreman_cif";
    video.source.video_file.width = 352;
    video.source.video_file.height = 288;
    video.source.framerate = 30;
    video.encoder.codec = VideoStreamConfig::Encoder::Codec::kVideoCodecVP8;
    video.encoder.implementation =
        VideoStreamConfig::Encoder::Implementation::kSoftware;
    video.hooks.frame_pair_handlers = {analyzer->Handler()};
  }
  s.CreateVideoStream(route->forward(), video);
  s.CreateAudioStream(route->forward(), AudioStreamConfig());
}
}  // namespace

TEST(ScenarioTest, SimTimeEncoding) {
  VideoQualityAnalyzerConfig analyzer_config;
  analyzer_config.psnr_coverage = 0.1;
  VideoQualityAnalyzer analyzer(analyzer_config);
  {
    Scenario s("scenario/encode_sim", false);
    SetupVideoCall(s, &analyzer);
    s.RunFor(TimeDelta::Seconds(2));
  }
  // Regression tests based on previous runs.
  EXPECT_EQ(analyzer.stats().lost_count, 0);
  EXPECT_NEAR(analyzer.stats().psnr_with_freeze.Mean(), 38, 5);
}

// TODO(bugs.webrtc.org/10515): Remove this when performance has been improved.
#if defined(WEBRTC_IOS) && defined(WEBRTC_ARCH_ARM64) && !defined(NDEBUG)
#define MAYBE_RealTimeEncoding DISABLED_RealTimeEncoding
#else
#define MAYBE_RealTimeEncoding RealTimeEncoding
#endif
TEST(ScenarioTest, MAYBE_RealTimeEncoding) {
  VideoQualityAnalyzerConfig analyzer_config;
  analyzer_config.psnr_coverage = 0.1;
  VideoQualityAnalyzer analyzer(analyzer_config);
  {
    Scenario s("scenario/encode_real", true);
    SetupVideoCall(s, &analyzer);
    s.RunFor(TimeDelta::Seconds(2));
  }
  // Regression tests based on previous runs.
  EXPECT_LT(analyzer.stats().lost_count, 2);
  // This far below expected but ensures that we get something.
  EXPECT_GT(analyzer.stats().psnr_with_freeze.Mean(), 10);
}

TEST(ScenarioTest, SimTimeFakeing) {
  Scenario s("scenario/encode_sim", false);
  SetupVideoCall(s, nullptr);
  s.RunFor(TimeDelta::Seconds(2));
}

TEST(ScenarioTest, WritesToRtcEventLog) {
  MemoryLogStorage storage;
  {
    Scenario s(storage.CreateFactory(), false);
    SetupVideoCall(s, nullptr);
    s.RunFor(TimeDelta::Seconds(1));
  }
  auto logs = storage.logs();
  // We expect that a rtc event log has been created and that it has some data.
  EXPECT_GE(storage.logs().at("alice.rtc.dat").size(), 1u);
}

TEST(ScenarioTest,
     RetransmitsVideoPacketsInAudioAndVideoCallWithSendSideBweAndLoss) {
  // Make sure audio packets are included in transport feedback.
  test::ScopedFieldTrials override_field_trials(
      "WebRTC-Audio-ABWENoTWCC/Disabled/");

  Scenario s;
  CallClientConfig call_client_config;
  call_client_config.transport.rates.start_rate = DataRate::KilobitsPerSec(300);
  auto* alice = s.CreateClient("alice", call_client_config);
  auto* bob = s.CreateClient("bob", call_client_config);
  NetworkSimulationConfig network_config;
  // Add some loss and delay.
  network_config.delay = TimeDelta::Millis(200);
  network_config.loss_rate = 0.05;
  auto alice_net = s.CreateSimulationNode(network_config);
  auto bob_net = s.CreateSimulationNode(network_config);
  auto route = s.CreateRoutes(alice, {alice_net}, bob, {bob_net});

  // First add an audio stream, then a video stream.
  // Needed to make sure audio RTP module is selected first when sending
  // transport feedback message.
  AudioStreamConfig audio_stream_config;
  audio_stream_config.encoder.min_rate = DataRate::KilobitsPerSec(6);
  audio_stream_config.encoder.max_rate = DataRate::KilobitsPerSec(64);
  audio_stream_config.encoder.allocate_bitrate = true;
  audio_stream_config.stream.in_bandwidth_estimation = true;
  s.CreateAudioStream(route->forward(), audio_stream_config);
  s.CreateAudioStream(route->reverse(), audio_stream_config);

  VideoStreamConfig video_stream_config;
  auto video = s.CreateVideoStream(route->forward(), video_stream_config);
  s.CreateVideoStream(route->reverse(), video_stream_config);

  // Run for 10 seconds.
  s.RunFor(TimeDelta::Seconds(10));
  // Make sure retransmissions have happened.
  int retransmit_packets = 0;

  VideoSendStream::Stats stats;
  alice->SendTask([&]() { stats = video->send()->GetStats(); });

  for (const auto& substream : stats.substreams) {
    retransmit_packets += substream.second.rtp_stats.retransmitted.packets;
  }
  EXPECT_GT(retransmit_packets, 0);
}

}  // namespace test
}  // namespace webrtc
