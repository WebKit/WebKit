/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/scenario/stats_collection.h"

#include "test/gtest.h"
#include "test/scenario/scenario.h"

namespace webrtc {
namespace test {
namespace {
void CreateAnalyzedStream(Scenario* s,
                          NetworkSimulationConfig network_config,
                          VideoQualityAnalyzer* analyzer,
                          CallStatsCollectors* collectors) {
  VideoStreamConfig config;
  config.encoder.codec = VideoStreamConfig::Encoder::Codec::kVideoCodecVP8;
  config.encoder.implementation =
      VideoStreamConfig::Encoder::Implementation::kSoftware;
  config.hooks.frame_pair_handlers = {analyzer->Handler()};
  auto* caller = s->CreateClient("caller", CallClientConfig());
  auto* callee = s->CreateClient("callee", CallClientConfig());
  auto route =
      s->CreateRoutes(caller, {s->CreateSimulationNode(network_config)}, callee,
                      {s->CreateSimulationNode(NetworkSimulationConfig())});
  VideoStreamPair* video = s->CreateVideoStream(route->forward(), config);
  auto* audio = s->CreateAudioStream(route->forward(), AudioStreamConfig());
  s->Every(TimeDelta::Seconds(1), [=] {
    collectors->call.AddStats(caller->GetStats());

    VideoSendStream::Stats send_stats;
    caller->SendTask([&]() { send_stats = video->send()->GetStats(); });
    collectors->video_send.AddStats(send_stats, s->Now());

    AudioReceiveStream::Stats receive_stats;
    caller->SendTask([&]() { receive_stats = audio->receive()->GetStats(); });
    collectors->audio_receive.AddStats(receive_stats);

    // Querying the video stats from within the expected runtime environment
    // (i.e. the TQ that belongs to the CallClient, not the Scenario TQ that
    // we're currently on).
    VideoReceiveStream::Stats video_receive_stats;
    auto* video_stream = video->receive();
    callee->SendTask([&video_stream, &video_receive_stats]() {
      video_receive_stats = video_stream->GetStats();
    });
    collectors->video_receive.AddStats(video_receive_stats);
  });
}
}  // namespace

TEST(ScenarioAnalyzerTest, PsnrIsHighWhenNetworkIsGood) {
  VideoQualityAnalyzer analyzer;
  CallStatsCollectors stats;
  {
    Scenario s;
    NetworkSimulationConfig good_network;
    good_network.bandwidth = DataRate::KilobitsPerSec(1000);
    CreateAnalyzedStream(&s, good_network, &analyzer, &stats);
    s.RunFor(TimeDelta::Seconds(3));
  }
  // This is a change detecting test, the targets are based on previous runs and
  // might change due to changes in configuration and encoder etc. The main
  // purpose is to show how the stats can be used. To avoid being overly
  // sensistive to change, the ranges are chosen to be quite large.
  EXPECT_NEAR(analyzer.stats().psnr_with_freeze.Mean(), 43, 10);
  EXPECT_NEAR(stats.call.stats().target_rate.Mean().kbps(), 700, 300);
  EXPECT_NEAR(stats.video_send.stats().media_bitrate.Mean().kbps(), 500, 200);
  EXPECT_NEAR(stats.video_receive.stats().resolution.Mean(), 180, 10);
  EXPECT_NEAR(stats.audio_receive.stats().jitter_buffer.Mean().ms(), 40, 20);
}

TEST(ScenarioAnalyzerTest, PsnrIsLowWhenNetworkIsBad) {
  VideoQualityAnalyzer analyzer;
  CallStatsCollectors stats;
  {
    Scenario s;
    NetworkSimulationConfig bad_network;
    bad_network.bandwidth = DataRate::KilobitsPerSec(100);
    bad_network.loss_rate = 0.02;
    CreateAnalyzedStream(&s, bad_network, &analyzer, &stats);
    s.RunFor(TimeDelta::Seconds(3));
  }
  // This is a change detecting test, the targets are based on previous runs and
  // might change due to changes in configuration and encoder etc.
  EXPECT_NEAR(analyzer.stats().psnr_with_freeze.Mean(), 20, 10);
  EXPECT_NEAR(stats.call.stats().target_rate.Mean().kbps(), 75, 50);
  EXPECT_NEAR(stats.video_send.stats().media_bitrate.Mean().kbps(), 100, 50);
  EXPECT_NEAR(stats.video_receive.stats().resolution.Mean(), 180, 10);
  EXPECT_NEAR(stats.audio_receive.stats().jitter_buffer.Mean().ms(), 250, 200);
}

TEST(ScenarioAnalyzerTest, CountsCapturedButNotRendered) {
  VideoQualityAnalyzer analyzer;
  CallStatsCollectors stats;
  {
    Scenario s;
    NetworkSimulationConfig long_delays;
    long_delays.delay = TimeDelta::Seconds(5);
    CreateAnalyzedStream(&s, long_delays, &analyzer, &stats);
    // Enough time to send frames but not enough to deliver.
    s.RunFor(TimeDelta::Millis(100));
  }
  EXPECT_GE(analyzer.stats().capture.count, 1);
  EXPECT_EQ(analyzer.stats().render.count, 0);
}
}  // namespace test
}  // namespace webrtc
