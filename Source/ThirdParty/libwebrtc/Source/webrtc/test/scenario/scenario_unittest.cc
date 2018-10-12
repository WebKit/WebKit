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
#include "test/gtest.h"
namespace webrtc {
namespace test {
TEST(ScenarioTest, StartsAndStopsWithoutErrors) {
  Scenario s;
  CallClientConfig call_client_config;
  call_client_config.transport.rates.start_rate = DataRate::kbps(300);
  auto* alice = s.CreateClient("alice", call_client_config);
  auto* bob = s.CreateClient("bob", call_client_config);
  NetworkNodeConfig network_config;
  auto alice_net = s.CreateSimulationNode(network_config);
  auto bob_net = s.CreateSimulationNode(network_config);

  VideoStreamConfig video_stream_config;
  s.CreateVideoStream(alice, {alice_net}, bob, {bob_net}, video_stream_config);
  s.CreateVideoStream(bob, {bob_net}, alice, {alice_net}, video_stream_config);

  AudioStreamConfig audio_stream_config;
  s.CreateAudioStream(alice, {alice_net}, bob, {bob_net}, audio_stream_config);
  s.CreateAudioStream(bob, {bob_net}, alice, {alice_net}, audio_stream_config);

  CrossTrafficConfig cross_traffic_config;
  s.CreateCrossTraffic({alice_net}, cross_traffic_config);

  bool packet_received = false;
  s.NetworkDelayedAction({alice_net, bob_net}, 100,
                         [&packet_received] { packet_received = true; });
  bool bitrate_changed = false;
  s.Every(TimeDelta::ms(10), [alice, bob, &bitrate_changed] {
    if (alice->GetStats().send_bandwidth_bps != 300000 &&
        bob->GetStats().send_bandwidth_bps != 300000)
      bitrate_changed = true;
  });
  s.RunUntil(TimeDelta::seconds(2), TimeDelta::ms(5),
             [&bitrate_changed, &packet_received] {
               return packet_received && bitrate_changed;
             });
  EXPECT_TRUE(packet_received);
  EXPECT_TRUE(bitrate_changed);
}
}  // namespace test
}  // namespace webrtc
