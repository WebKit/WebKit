/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <utility>

#include "webrtc/common_audio/mocks/mock_smoothing_filter.h"
#include "webrtc/modules/audio_coding/audio_network_adaptor/fec_controller.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::_;

namespace {

// The test uses the following settings:
//
// packet-loss ^   |  |
//             |  A| C|   FEC
//             |    \  \   ON
//             | FEC \ D\_______
//             | OFF B\_________
//             |-----------------> bandwidth
//
// A : (kDisablingBandwidthLow, kDisablingPacketLossAtLowBw)
// B : (kDisablingBandwidthHigh, kDisablingPacketLossAtHighBw)
// C : (kEnablingBandwidthLow, kEnablingPacketLossAtLowBw)
// D : (kEnablingBandwidthHigh, kEnablingPacketLossAtHighBw)

constexpr int kDisablingBandwidthLow = 15000;
constexpr float kDisablingPacketLossAtLowBw = 0.08f;
constexpr int kDisablingBandwidthHigh = 64000;
constexpr float kDisablingPacketLossAtHighBw = 0.01f;
constexpr int kEnablingBandwidthLow = 17000;
constexpr float kEnablingPacketLossAtLowBw = 0.1f;
constexpr int kEnablingBandwidthHigh = 64000;
constexpr float kEnablingPacketLossAtHighBw = 0.05f;

struct FecControllerStates {
  std::unique_ptr<FecController> controller;
  MockSmoothingFilter* packet_loss_smoother;
};

FecControllerStates CreateFecController(bool initial_fec_enabled) {
  FecControllerStates states;
  std::unique_ptr<MockSmoothingFilter> mock_smoothing_filter(
      new NiceMock<MockSmoothingFilter>());
  states.packet_loss_smoother = mock_smoothing_filter.get();
  using Threshold = FecController::Config::Threshold;
  states.controller.reset(new FecController(
      FecController::Config(
          initial_fec_enabled,
          Threshold(kEnablingBandwidthLow, kEnablingPacketLossAtLowBw,
                    kEnablingBandwidthHigh, kEnablingPacketLossAtHighBw),
          Threshold(kDisablingBandwidthLow, kDisablingPacketLossAtLowBw,
                    kDisablingBandwidthHigh, kDisablingPacketLossAtHighBw),
          0, nullptr),
      std::move(mock_smoothing_filter)));
  return states;
}

void UpdateNetworkMetrics(FecControllerStates* states,
                          const rtc::Optional<int>& uplink_bandwidth_bps,
                          const rtc::Optional<float>& uplink_packet_loss) {
  // UpdateNetworkMetrics can accept multiple network metric updates at once.
  // However, currently, the most used case is to update one metric at a time.
  // To reflect this fact, we separate the calls.
  if (uplink_bandwidth_bps) {
    Controller::NetworkMetrics network_metrics;
    network_metrics.uplink_bandwidth_bps = uplink_bandwidth_bps;
    states->controller->UpdateNetworkMetrics(network_metrics);
  }
  if (uplink_packet_loss) {
    Controller::NetworkMetrics network_metrics;
    network_metrics.uplink_packet_loss_fraction = uplink_packet_loss;
    EXPECT_CALL(*states->packet_loss_smoother, AddSample(*uplink_packet_loss));
    states->controller->UpdateNetworkMetrics(network_metrics);
    // This is called during CheckDecision().
    EXPECT_CALL(*states->packet_loss_smoother, GetAverage())
        .WillOnce(Return(rtc::Optional<float>(*uplink_packet_loss)));
  }
}

// Checks that the FEC decision and |uplink_packet_loss_fraction| given by
// |states->controller->MakeDecision| matches |expected_enable_fec| and
// |expected_uplink_packet_loss_fraction|, respectively.
void CheckDecision(FecControllerStates* states,
                   bool expected_enable_fec,
                   float expected_uplink_packet_loss_fraction) {
  AudioNetworkAdaptor::EncoderRuntimeConfig config;
  states->controller->MakeDecision(&config);
  EXPECT_EQ(rtc::Optional<bool>(expected_enable_fec), config.enable_fec);
  EXPECT_EQ(rtc::Optional<float>(expected_uplink_packet_loss_fraction),
            config.uplink_packet_loss_fraction);
}

}  // namespace

TEST(FecControllerTest, OutputInitValueWhenUplinkBandwidthUnknown) {
  constexpr bool kInitialFecEnabled = true;
  auto states = CreateFecController(kInitialFecEnabled);
  // Let uplink packet loss fraction be so low that would cause FEC to turn off
  // if uplink bandwidth was known.
  UpdateNetworkMetrics(&states, rtc::Optional<int>(),
                       rtc::Optional<float>(kDisablingPacketLossAtHighBw));
  CheckDecision(&states, kInitialFecEnabled, kDisablingPacketLossAtHighBw);
}

TEST(FecControllerTest, OutputInitValueWhenUplinkPacketLossFractionUnknown) {
  constexpr bool kInitialFecEnabled = true;
  auto states = CreateFecController(kInitialFecEnabled);
  // Let uplink bandwidth be so low that would cause FEC to turn off if uplink
  // bandwidth packet loss fraction was known.
  UpdateNetworkMetrics(&states, rtc::Optional<int>(kDisablingBandwidthLow - 1),
                       rtc::Optional<float>());
  CheckDecision(&states, kInitialFecEnabled, 0.0);
}

TEST(FecControllerTest, EnableFecForHighBandwidth) {
  auto states = CreateFecController(false);
  UpdateNetworkMetrics(&states, rtc::Optional<int>(kEnablingBandwidthHigh),
                       rtc::Optional<float>(kEnablingPacketLossAtHighBw));
  CheckDecision(&states, true, kEnablingPacketLossAtHighBw);
}

TEST(FecControllerTest, UpdateMultipleNetworkMetricsAtOnce) {
  // This test is similar to EnableFecForHighBandwidth. But instead of
  // using ::UpdateNetworkMetrics(...), which calls
  // FecController::UpdateNetworkMetrics(...) multiple times, we
  // we call it only once. This is to verify that
  // FecController::UpdateNetworkMetrics(...) can handle multiple
  // network updates at once. This is, however, not a common use case in current
  // audio_network_adaptor_impl.cc.
  auto states = CreateFecController(false);
  Controller::NetworkMetrics network_metrics;
  network_metrics.uplink_bandwidth_bps =
      rtc::Optional<int>(kEnablingBandwidthHigh);
  network_metrics.uplink_packet_loss_fraction =
      rtc::Optional<float>(kEnablingPacketLossAtHighBw);
  EXPECT_CALL(*states.packet_loss_smoother, GetAverage())
      .WillOnce(Return(rtc::Optional<float>(kEnablingPacketLossAtHighBw)));
  states.controller->UpdateNetworkMetrics(network_metrics);
  CheckDecision(&states, true, kEnablingPacketLossAtHighBw);
}

TEST(FecControllerTest, MaintainFecOffForHighBandwidth) {
  auto states = CreateFecController(false);
  constexpr float kPacketLoss = kEnablingPacketLossAtHighBw * 0.99f;
  UpdateNetworkMetrics(&states, rtc::Optional<int>(kEnablingBandwidthHigh),
                       rtc::Optional<float>(kPacketLoss));
  CheckDecision(&states, false, kPacketLoss);
}

TEST(FecControllerTest, EnableFecForMediumBandwidth) {
  auto states = CreateFecController(false);
  constexpr float kPacketLoss =
      (kEnablingPacketLossAtLowBw + kEnablingPacketLossAtHighBw) / 2.0;
  UpdateNetworkMetrics(
      &states,
      rtc::Optional<int>((kEnablingBandwidthHigh + kEnablingBandwidthLow) / 2),
      rtc::Optional<float>(kPacketLoss));
  CheckDecision(&states, true, kPacketLoss);
}

TEST(FecControllerTest, MaintainFecOffForMediumBandwidth) {
  auto states = CreateFecController(false);
  constexpr float kPacketLoss =
      kEnablingPacketLossAtLowBw * 0.49f + kEnablingPacketLossAtHighBw * 0.51f;
  UpdateNetworkMetrics(
      &states,
      rtc::Optional<int>((kEnablingBandwidthHigh + kEnablingBandwidthLow) / 2),
      rtc::Optional<float>(kPacketLoss));
  CheckDecision(&states, false, kPacketLoss);
}

TEST(FecControllerTest, EnableFecForLowBandwidth) {
  auto states = CreateFecController(false);
  UpdateNetworkMetrics(&states, rtc::Optional<int>(kEnablingBandwidthLow),
                       rtc::Optional<float>(kEnablingPacketLossAtLowBw));
  CheckDecision(&states, true, kEnablingPacketLossAtLowBw);
}

TEST(FecControllerTest, MaintainFecOffForLowBandwidth) {
  auto states = CreateFecController(false);
  constexpr float kPacketLoss = kEnablingPacketLossAtLowBw * 0.99f;
  UpdateNetworkMetrics(&states, rtc::Optional<int>(kEnablingBandwidthLow),
                       rtc::Optional<float>(kPacketLoss));
  CheckDecision(&states, false, kPacketLoss);
}

TEST(FecControllerTest, MaintainFecOffForVeryLowBandwidth) {
  auto states = CreateFecController(false);
  // Below |kEnablingBandwidthLow|, no packet loss fraction can cause FEC to
  // turn on.
  UpdateNetworkMetrics(&states, rtc::Optional<int>(kEnablingBandwidthLow - 1),
                       rtc::Optional<float>(1.0));
  CheckDecision(&states, false, 1.0);
}

TEST(FecControllerTest, DisableFecForHighBandwidth) {
  auto states = CreateFecController(true);
  UpdateNetworkMetrics(&states, rtc::Optional<int>(kDisablingBandwidthHigh),
                       rtc::Optional<float>(kDisablingPacketLossAtHighBw));
  CheckDecision(&states, false, kDisablingPacketLossAtHighBw);
}

TEST(FecControllerTest, MaintainFecOnForHighBandwidth) {
  auto states = CreateFecController(true);
  constexpr float kPacketLoss = kDisablingPacketLossAtHighBw * 1.01f;
  UpdateNetworkMetrics(&states, rtc::Optional<int>(kDisablingBandwidthHigh),
                       rtc::Optional<float>(kPacketLoss));
  CheckDecision(&states, true, kPacketLoss);
}

TEST(FecControllerTest, DisableFecOnMediumBandwidth) {
  auto states = CreateFecController(true);
  constexpr float kPacketLoss =
      (kDisablingPacketLossAtLowBw + kDisablingPacketLossAtHighBw) / 2.0f;
  UpdateNetworkMetrics(
      &states, rtc::Optional<int>(
                   (kDisablingBandwidthHigh + kDisablingBandwidthLow) / 2),
      rtc::Optional<float>(kPacketLoss));
  CheckDecision(&states, false, kPacketLoss);
}

TEST(FecControllerTest, MaintainFecOnForMediumBandwidth) {
  auto states = CreateFecController(true);
  constexpr float kPacketLoss = kDisablingPacketLossAtLowBw * 0.51f +
                                kDisablingPacketLossAtHighBw * 0.49f;
  UpdateNetworkMetrics(
      &states,
      rtc::Optional<int>((kEnablingBandwidthHigh + kDisablingBandwidthLow) / 2),
      rtc::Optional<float>(kPacketLoss));
  CheckDecision(&states, true, kPacketLoss);
}

TEST(FecControllerTest, DisableFecForLowBandwidth) {
  auto states = CreateFecController(true);
  UpdateNetworkMetrics(&states, rtc::Optional<int>(kDisablingBandwidthLow),
                       rtc::Optional<float>(kDisablingPacketLossAtLowBw));
  CheckDecision(&states, false, kDisablingPacketLossAtLowBw);
}

TEST(FecControllerTest, DisableFecForVeryLowBandwidth) {
  auto states = CreateFecController(true);
  // Below |kEnablingBandwidthLow|, any packet loss fraction can cause FEC to
  // turn off.
  UpdateNetworkMetrics(&states, rtc::Optional<int>(kDisablingBandwidthLow - 1),
                       rtc::Optional<float>(1.0));
  CheckDecision(&states, false, 1.0);
}

TEST(FecControllerTest, CheckBehaviorOnChangingNetworkMetrics) {
  // In this test, we let the network metrics to traverse from 1 to 5.
  // packet-loss ^ 1 |  |
  //             |   | 2|
  //             |    \  \ 3
  //             |     \4 \_______
  //             |      \_________
  //             |---------5-------> bandwidth

  auto states = CreateFecController(true);
  UpdateNetworkMetrics(&states, rtc::Optional<int>(kDisablingBandwidthLow - 1),
                       rtc::Optional<float>(1.0));
  CheckDecision(&states, false, 1.0);

  UpdateNetworkMetrics(
      &states, rtc::Optional<int>(kEnablingBandwidthLow),
      rtc::Optional<float>(kEnablingPacketLossAtLowBw * 0.99f));
  CheckDecision(&states, false, kEnablingPacketLossAtLowBw * 0.99f);

  UpdateNetworkMetrics(&states, rtc::Optional<int>(kEnablingBandwidthHigh),
                       rtc::Optional<float>(kEnablingPacketLossAtHighBw));
  CheckDecision(&states, true, kEnablingPacketLossAtHighBw);

  UpdateNetworkMetrics(
      &states, rtc::Optional<int>(kDisablingBandwidthHigh),
      rtc::Optional<float>(kDisablingPacketLossAtHighBw * 1.01f));
  CheckDecision(&states, true, kDisablingPacketLossAtHighBw * 1.01f);

  UpdateNetworkMetrics(&states, rtc::Optional<int>(kDisablingBandwidthHigh + 1),
                       rtc::Optional<float>(0.0));
  CheckDecision(&states, false, 0.0);
}

TEST(FecControllerTest, CheckBehaviorOnSpecialCurves) {
  // We test a special configuration, where the points to define the FEC
  // enabling/disabling curves are placed like the following, otherwise the test
  // is the same as CheckBehaviorOnChangingNetworkMetrics.
  //
  // packet-loss ^   |  |
  //             |   | C|
  //             |   |  |
  //             |   | D|_______
  //             |  A|___B______
  //             |-----------------> bandwidth

  constexpr int kEnablingBandwidthHigh = kEnablingBandwidthLow;
  constexpr float kDisablingPacketLossAtLowBw = kDisablingPacketLossAtHighBw;
  FecControllerStates states;
  std::unique_ptr<MockSmoothingFilter> mock_smoothing_filter(
      new NiceMock<MockSmoothingFilter>());
  states.packet_loss_smoother = mock_smoothing_filter.get();
  using Threshold = FecController::Config::Threshold;
  states.controller.reset(new FecController(
      FecController::Config(
          true, Threshold(kEnablingBandwidthLow, kEnablingPacketLossAtLowBw,
                          kEnablingBandwidthHigh, kEnablingPacketLossAtHighBw),
          Threshold(kDisablingBandwidthLow, kDisablingPacketLossAtLowBw,
                    kDisablingBandwidthHigh, kDisablingPacketLossAtHighBw),
          0, nullptr),
      std::move(mock_smoothing_filter)));

  UpdateNetworkMetrics(&states, rtc::Optional<int>(kDisablingBandwidthLow - 1),
                       rtc::Optional<float>(1.0));
  CheckDecision(&states, false, 1.0);

  UpdateNetworkMetrics(
      &states, rtc::Optional<int>(kEnablingBandwidthLow),
      rtc::Optional<float>(kEnablingPacketLossAtHighBw * 0.99f));
  CheckDecision(&states, false, kEnablingPacketLossAtHighBw * 0.99f);

  UpdateNetworkMetrics(&states, rtc::Optional<int>(kEnablingBandwidthHigh),
                       rtc::Optional<float>(kEnablingPacketLossAtHighBw));
  CheckDecision(&states, true, kEnablingPacketLossAtHighBw);

  UpdateNetworkMetrics(
      &states, rtc::Optional<int>(kDisablingBandwidthHigh),
      rtc::Optional<float>(kDisablingPacketLossAtHighBw * 1.01f));
  CheckDecision(&states, true, kDisablingPacketLossAtHighBw * 1.01f);

  UpdateNetworkMetrics(&states, rtc::Optional<int>(kDisablingBandwidthHigh + 1),
                       rtc::Optional<float>(0.0));
  CheckDecision(&states, false, 0.0);
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST(FecControllerDeathTest, InvalidConfig) {
  FecControllerStates states;
  std::unique_ptr<MockSmoothingFilter> mock_smoothing_filter(
      new NiceMock<MockSmoothingFilter>());
  states.packet_loss_smoother = mock_smoothing_filter.get();
  using Threshold = FecController::Config::Threshold;
  EXPECT_DEATH(
      states.controller.reset(new FecController(
          FecController::Config(
              true,
              Threshold(kDisablingBandwidthLow - 1, kEnablingPacketLossAtLowBw,
                        kEnablingBandwidthHigh, kEnablingPacketLossAtHighBw),
              Threshold(kDisablingBandwidthLow, kDisablingPacketLossAtLowBw,
                        kDisablingBandwidthHigh, kDisablingPacketLossAtHighBw),
              0, nullptr),
          std::move(mock_smoothing_filter))),
      "Check failed");
}
#endif

}  // namespace webrtc
