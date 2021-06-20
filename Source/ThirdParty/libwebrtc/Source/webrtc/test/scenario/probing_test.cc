/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/gtest.h"
#include "test/scenario/scenario.h"

namespace webrtc {
namespace test {

TEST(ProbingTest, InitialProbingRampsUpTargetRateWhenNetworkIsGood) {
  Scenario s;
  NetworkSimulationConfig good_network;
  good_network.bandwidth = DataRate::KilobitsPerSec(2000);

  VideoStreamConfig video_config;
  video_config.encoder.codec =
      VideoStreamConfig::Encoder::Codec::kVideoCodecVP8;
  CallClientConfig send_config;
  auto* caller = s.CreateClient("caller", send_config);
  auto* callee = s.CreateClient("callee", CallClientConfig());
  auto route =
      s.CreateRoutes(caller, {s.CreateSimulationNode(good_network)}, callee,
                     {s.CreateSimulationNode(NetworkSimulationConfig())});
  s.CreateVideoStream(route->forward(), video_config);

  s.RunFor(TimeDelta::Seconds(1));
  EXPECT_GE(DataRate::BitsPerSec(caller->GetStats().send_bandwidth_bps),
            3 * send_config.transport.rates.start_rate);
}

TEST(ProbingTest, MidCallProbingRampupTriggeredByUpdatedBitrateConstraints) {
  Scenario s;

  const DataRate kStartRate = DataRate::KilobitsPerSec(300);
  const DataRate kConstrainedRate = DataRate::KilobitsPerSec(100);
  const DataRate kHighRate = DataRate::KilobitsPerSec(2500);

  VideoStreamConfig video_config;
  video_config.encoder.codec =
      VideoStreamConfig::Encoder::Codec::kVideoCodecVP8;
  CallClientConfig send_call_config;
  send_call_config.transport.rates.start_rate = kStartRate;
  send_call_config.transport.rates.max_rate = kHighRate * 2;
  auto* caller = s.CreateClient("caller", send_call_config);
  auto* callee = s.CreateClient("callee", CallClientConfig());
  auto route = s.CreateRoutes(
      caller, {s.CreateSimulationNode(NetworkSimulationConfig())}, callee,
      {s.CreateSimulationNode(NetworkSimulationConfig())});
  s.CreateVideoStream(route->forward(), video_config);

  // Wait until initial probing rampup is done and then set a low max bitrate.
  s.RunFor(TimeDelta::Seconds(1));
  EXPECT_GE(DataRate::BitsPerSec(caller->GetStats().send_bandwidth_bps),
            5 * send_call_config.transport.rates.start_rate);
  BitrateConstraints bitrate_config;
  bitrate_config.max_bitrate_bps = kConstrainedRate.bps();
  caller->UpdateBitrateConstraints(bitrate_config);

  // Wait until the low send bitrate has taken effect, and then set a much
  // higher max bitrate.
  s.RunFor(TimeDelta::Seconds(2));
  EXPECT_LT(DataRate::BitsPerSec(caller->GetStats().send_bandwidth_bps),
            kConstrainedRate * 1.1);
  bitrate_config.max_bitrate_bps = 2 * kHighRate.bps();
  caller->UpdateBitrateConstraints(bitrate_config);

  // Check that the max send bitrate is reached quicker than would be possible
  // with simple AIMD rate control.
  s.RunFor(TimeDelta::Seconds(1));
  EXPECT_GE(DataRate::BitsPerSec(caller->GetStats().send_bandwidth_bps),
            kHighRate);
}

TEST(ProbingTest, ProbesRampsUpWhenVideoEncoderConfigChanges) {
  Scenario s;
  const DataRate kStartRate = DataRate::KilobitsPerSec(50);
  const DataRate kHdRate = DataRate::KilobitsPerSec(3250);

  // Set up 3-layer simulcast.
  VideoStreamConfig video_config;
  video_config.encoder.codec =
      VideoStreamConfig::Encoder::Codec::kVideoCodecVP8;
  video_config.encoder.layers.spatial = 3;
  video_config.source.generator.width = 1280;
  video_config.source.generator.height = 720;

  CallClientConfig send_call_config;
  send_call_config.transport.rates.start_rate = kStartRate;
  send_call_config.transport.rates.max_rate = kHdRate * 2;
  auto* caller = s.CreateClient("caller", send_call_config);
  auto* callee = s.CreateClient("callee", CallClientConfig());
  auto send_net =
      s.CreateMutableSimulationNode([&](NetworkSimulationConfig* c) {
        c->bandwidth = DataRate::KilobitsPerSec(200);
      });
  auto route =
      s.CreateRoutes(caller, {send_net->node()}, callee,
                     {s.CreateSimulationNode(NetworkSimulationConfig())});
  auto* video_stream = s.CreateVideoStream(route->forward(), video_config);

  // Only QVGA enabled initially. Run until initial probing is done and BWE
  // has settled.
  video_stream->send()->UpdateActiveLayers({true, false, false});
  s.RunFor(TimeDelta::Seconds(2));

  // Remove network constraints and run for a while more, BWE should be much
  // less than required HD rate.
  send_net->UpdateConfig([&](NetworkSimulationConfig* c) {
    c->bandwidth = DataRate::PlusInfinity();
  });
  s.RunFor(TimeDelta::Seconds(2));

  DataRate bandwidth =
      DataRate::BitsPerSec(caller->GetStats().send_bandwidth_bps);
  EXPECT_LT(bandwidth, kHdRate / 4);

  // Enable all layers, triggering a probe.
  video_stream->send()->UpdateActiveLayers({true, true, true});

  // Run for a short while and verify BWE has ramped up fast.
  s.RunFor(TimeDelta::Seconds(2));
  EXPECT_GT(DataRate::BitsPerSec(caller->GetStats().send_bandwidth_bps),
            kHdRate);
}

}  // namespace test
}  // namespace webrtc
