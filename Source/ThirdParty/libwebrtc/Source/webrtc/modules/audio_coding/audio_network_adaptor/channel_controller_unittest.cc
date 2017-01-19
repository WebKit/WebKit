/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "webrtc/modules/audio_coding/audio_network_adaptor/channel_controller.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

namespace {

constexpr int kNumChannels = 2;
constexpr int kChannel1To2BandwidthBps = 31000;
constexpr int kChannel2To1BandwidthBps = 29000;
constexpr int kMediumBandwidthBps =
    (kChannel1To2BandwidthBps + kChannel2To1BandwidthBps) / 2;

std::unique_ptr<ChannelController> CreateChannelController(int init_channels) {
  std::unique_ptr<ChannelController> controller(
      new ChannelController(ChannelController::Config(
          kNumChannels, init_channels, kChannel1To2BandwidthBps,
          kChannel2To1BandwidthBps)));
  return controller;
}

void CheckDecision(ChannelController* controller,
                   const Controller::NetworkMetrics& metrics,
                   size_t expected_num_channels) {
  AudioNetworkAdaptor::EncoderRuntimeConfig config;
  controller->MakeDecision(metrics, &config);
  EXPECT_EQ(rtc::Optional<size_t>(expected_num_channels), config.num_channels);
}

}  // namespace

TEST(ChannelControllerTest, OutputInitValueWhenUplinkBandwidthUnknown) {
  constexpr int kInitChannels = 2;
  auto controller = CreateChannelController(kInitChannels);
  Controller::NetworkMetrics metrics;
  CheckDecision(controller.get(), metrics, kInitChannels);
}

TEST(ChannelControllerTest, SwitchTo2ChannelsOnHighUplinkBandwidth) {
  constexpr int kInitChannels = 1;
  auto controller = CreateChannelController(kInitChannels);
  Controller::NetworkMetrics metrics;

  // Use high bandwidth to check output switch to 2.
  metrics.uplink_bandwidth_bps = rtc::Optional<int>(kChannel1To2BandwidthBps);
  CheckDecision(controller.get(), metrics, 2);
}

TEST(ChannelControllerTest, SwitchTo1ChannelOnLowUplinkBandwidth) {
  constexpr int kInitChannels = 2;
  auto controller = CreateChannelController(kInitChannels);
  Controller::NetworkMetrics metrics;

  // Use low bandwidth to check output switch to 1.
  metrics.uplink_bandwidth_bps = rtc::Optional<int>(kChannel2To1BandwidthBps);
  CheckDecision(controller.get(), metrics, 1);
}

TEST(ChannelControllerTest, Maintain1ChannelOnMediumUplinkBandwidth) {
  constexpr int kInitChannels = 1;
  auto controller = CreateChannelController(kInitChannels);
  Controller::NetworkMetrics metrics;

  // Use between-thresholds bandwidth to check output remains at 1.
  metrics.uplink_bandwidth_bps = rtc::Optional<int>(kMediumBandwidthBps);
  CheckDecision(controller.get(), metrics, 1);
}

TEST(ChannelControllerTest, Maintain2ChannelsOnMediumUplinkBandwidth) {
  constexpr int kInitChannels = 2;
  auto controller = CreateChannelController(kInitChannels);
  Controller::NetworkMetrics metrics;

  // Use between-thresholds bandwidth to check output remains at 2.
  metrics.uplink_bandwidth_bps = rtc::Optional<int>(kMediumBandwidthBps);
  CheckDecision(controller.get(), metrics, 2);
}

TEST(ChannelControllerTest, CheckBehaviorOnChangingUplinkBandwidth) {
  constexpr int kInitChannels = 1;
  auto controller = CreateChannelController(kInitChannels);
  Controller::NetworkMetrics metrics;

  // Use between-thresholds bandwidth to check output remains at 1.
  metrics.uplink_bandwidth_bps = rtc::Optional<int>(kMediumBandwidthBps);
  CheckDecision(controller.get(), metrics, 1);

  // Use high bandwidth to check output switch to 2.
  metrics.uplink_bandwidth_bps = rtc::Optional<int>(kChannel1To2BandwidthBps);
  CheckDecision(controller.get(), metrics, 2);

  // Use between-thresholds bandwidth to check output remains at 2.
  metrics.uplink_bandwidth_bps = rtc::Optional<int>(kMediumBandwidthBps);
  CheckDecision(controller.get(), metrics, 2);

  // Use low bandwidth to check output switch to 1.
  metrics.uplink_bandwidth_bps = rtc::Optional<int>(kChannel2To1BandwidthBps);
  CheckDecision(controller.get(), metrics, 1);
}

}  // namespace webrtc
