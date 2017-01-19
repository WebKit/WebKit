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

#include "webrtc/modules/audio_coding/audio_network_adaptor/fec_controller.h"
#include "webrtc/modules/audio_coding/audio_network_adaptor/mock/mock_smoothing_filter.h"
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
  MockSmoothingFilter* packet_loss_smoothed;
};

FecControllerStates CreateFecController(bool initial_fec_enabled) {
  FecControllerStates states;
  std::unique_ptr<MockSmoothingFilter> mock_smoothing_filter(
      new NiceMock<MockSmoothingFilter>());
  states.packet_loss_smoothed = mock_smoothing_filter.get();
  EXPECT_CALL(*states.packet_loss_smoothed, Die());
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

// Checks that the FEC decision given by |states->controller->MakeDecision|
// matches |expected_enable_fec|. It also checks that
// |uplink_packet_loss_fraction| returned by |states->controller->MakeDecision|
// matches |uplink_packet_loss|.
void CheckDecision(FecControllerStates* states,
                   const rtc::Optional<int>& uplink_bandwidth_bps,
                   const rtc::Optional<float>& uplink_packet_loss,
                   bool expected_enable_fec) {
  Controller::NetworkMetrics metrics;
  metrics.uplink_bandwidth_bps = uplink_bandwidth_bps;
  metrics.uplink_packet_loss_fraction = uplink_packet_loss;

  if (uplink_packet_loss) {
    // Check that smoothing filter is updated.
    EXPECT_CALL(*states->packet_loss_smoothed, AddSample(*uplink_packet_loss));
  }

  EXPECT_CALL(*states->packet_loss_smoothed, GetAverage())
      .WillRepeatedly(Return(uplink_packet_loss));

  AudioNetworkAdaptor::EncoderRuntimeConfig config;
  states->controller->MakeDecision(metrics, &config);
  EXPECT_EQ(rtc::Optional<bool>(expected_enable_fec), config.enable_fec);

  // Check that |config.uplink_packet_loss_fraction| is properly filled.
  EXPECT_EQ(uplink_packet_loss ? uplink_packet_loss : rtc::Optional<float>(0.0),
            config.uplink_packet_loss_fraction);
}

}  // namespace

TEST(FecControllerTest, OutputInitValueWhenUplinkBandwidthUnknown) {
  constexpr bool kInitialFecEnabled = true;
  auto states = CreateFecController(kInitialFecEnabled);
  // Let uplink packet loss fraction be so low that would cause FEC to turn off
  // if uplink bandwidth was known.
  CheckDecision(&states, rtc::Optional<int>(),
                rtc::Optional<float>(kDisablingPacketLossAtHighBw),
                kInitialFecEnabled);
}

TEST(FecControllerTest, OutputInitValueWhenUplinkPacketLossFractionUnknown) {
  constexpr bool kInitialFecEnabled = true;
  auto states = CreateFecController(kInitialFecEnabled);
  // Let uplink bandwidth be so low that would cause FEC to turn off if uplink
  // bandwidth packet loss fraction was known.
  CheckDecision(&states, rtc::Optional<int>(kDisablingBandwidthLow - 1),
                rtc::Optional<float>(), kInitialFecEnabled);
}

TEST(FecControllerTest, EnableFecForHighBandwidth) {
  auto states = CreateFecController(false);
  CheckDecision(&states, rtc::Optional<int>(kEnablingBandwidthHigh),
                rtc::Optional<float>(kEnablingPacketLossAtHighBw), true);
}

TEST(FecControllerTest, MaintainFecOffForHighBandwidth) {
  auto states = CreateFecController(false);
  CheckDecision(&states, rtc::Optional<int>(kEnablingBandwidthHigh),
                rtc::Optional<float>(kEnablingPacketLossAtHighBw * 0.99f),
                false);
}

TEST(FecControllerTest, EnableFecForMediumBandwidth) {
  auto states = CreateFecController(false);
  CheckDecision(
      &states,
      rtc::Optional<int>((kEnablingBandwidthHigh + kEnablingBandwidthLow) / 2),
      rtc::Optional<float>(
          (kEnablingPacketLossAtLowBw + kEnablingPacketLossAtHighBw) / 2.0),
      true);
}

TEST(FecControllerTest, MaintainFecOffForMediumBandwidth) {
  auto states = CreateFecController(false);
  CheckDecision(
      &states,
      rtc::Optional<int>((kEnablingBandwidthHigh + kEnablingBandwidthLow) / 2),
      rtc::Optional<float>(kEnablingPacketLossAtLowBw * 0.49f +
                           kEnablingPacketLossAtHighBw * 0.51f),
      false);
}

TEST(FecControllerTest, EnableFecForLowBandwidth) {
  auto states = CreateFecController(false);
  CheckDecision(&states, rtc::Optional<int>(kEnablingBandwidthLow),
                rtc::Optional<float>(kEnablingPacketLossAtLowBw), true);
}

TEST(FecControllerTest, MaintainFecOffForLowBandwidth) {
  auto states = CreateFecController(false);
  CheckDecision(&states, rtc::Optional<int>(kEnablingBandwidthLow),
                rtc::Optional<float>(kEnablingPacketLossAtLowBw * 0.99f),
                false);
}

TEST(FecControllerTest, MaintainFecOffForVeryLowBandwidth) {
  auto states = CreateFecController(false);
  // Below |kEnablingBandwidthLow|, no packet loss fraction can cause FEC to
  // turn on.
  CheckDecision(&states, rtc::Optional<int>(kEnablingBandwidthLow - 1),
                rtc::Optional<float>(1.0), false);
}

TEST(FecControllerTest, DisableFecForHighBandwidth) {
  auto states = CreateFecController(true);
  CheckDecision(&states, rtc::Optional<int>(kDisablingBandwidthHigh),
                rtc::Optional<float>(kDisablingPacketLossAtHighBw), false);
}

TEST(FecControllerTest, MaintainFecOnForHighBandwidth) {
  auto states = CreateFecController(true);
  CheckDecision(&states, rtc::Optional<int>(kDisablingBandwidthHigh),
                rtc::Optional<float>(kDisablingPacketLossAtHighBw * 1.01f),
                true);
}

TEST(FecControllerTest, DisableFecOnMediumBandwidth) {
  auto states = CreateFecController(true);
  CheckDecision(
      &states, rtc::Optional<int>(
                   (kDisablingBandwidthHigh + kDisablingBandwidthLow) / 2),
      rtc::Optional<float>(
          (kDisablingPacketLossAtLowBw + kDisablingPacketLossAtHighBw) / 2.0f),
      false);
}

TEST(FecControllerTest, MaintainFecOnForMediumBandwidth) {
  auto states = CreateFecController(true);
  CheckDecision(
      &states,
      rtc::Optional<int>((kEnablingBandwidthHigh + kDisablingBandwidthLow) / 2),
      rtc::Optional<float>(kDisablingPacketLossAtLowBw * 0.51f +
                           kDisablingPacketLossAtHighBw * 0.49f),
      true);
}

TEST(FecControllerTest, DisableFecForLowBandwidth) {
  auto states = CreateFecController(true);
  CheckDecision(&states, rtc::Optional<int>(kDisablingBandwidthLow),
                rtc::Optional<float>(kDisablingPacketLossAtLowBw), false);
}

TEST(FecControllerTest, DisableFecForVeryLowBandwidth) {
  auto states = CreateFecController(true);
  // Below |kEnablingBandwidthLow|, any packet loss fraction can cause FEC to
  // turn off.
  CheckDecision(&states, rtc::Optional<int>(kDisablingBandwidthLow - 1),
                rtc::Optional<float>(1.0), false);
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
  CheckDecision(&states, rtc::Optional<int>(kDisablingBandwidthLow - 1),
                rtc::Optional<float>(1.0), false);
  CheckDecision(&states, rtc::Optional<int>(kEnablingBandwidthLow),
                rtc::Optional<float>(kEnablingPacketLossAtLowBw * 0.99f),
                false);
  CheckDecision(&states, rtc::Optional<int>(kEnablingBandwidthHigh),
                rtc::Optional<float>(kEnablingPacketLossAtHighBw), true);
  CheckDecision(&states, rtc::Optional<int>(kDisablingBandwidthHigh),
                rtc::Optional<float>(kDisablingPacketLossAtHighBw * 1.01f),
                true);
  CheckDecision(&states, rtc::Optional<int>(kDisablingBandwidthHigh + 1),
                rtc::Optional<float>(0.0), false);
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
  states.packet_loss_smoothed = mock_smoothing_filter.get();
  EXPECT_CALL(*states.packet_loss_smoothed, Die());
  using Threshold = FecController::Config::Threshold;
  states.controller.reset(new FecController(
      FecController::Config(
          true, Threshold(kEnablingBandwidthLow, kEnablingPacketLossAtLowBw,
                          kEnablingBandwidthHigh, kEnablingPacketLossAtHighBw),
          Threshold(kDisablingBandwidthLow, kDisablingPacketLossAtLowBw,
                    kDisablingBandwidthHigh, kDisablingPacketLossAtHighBw),
          0, nullptr),
      std::move(mock_smoothing_filter)));

  CheckDecision(&states, rtc::Optional<int>(kDisablingBandwidthLow - 1),
                rtc::Optional<float>(1.0), false);
  CheckDecision(&states, rtc::Optional<int>(kEnablingBandwidthLow),
                rtc::Optional<float>(kEnablingPacketLossAtHighBw * 0.99f),
                false);
  CheckDecision(&states, rtc::Optional<int>(kEnablingBandwidthHigh),
                rtc::Optional<float>(kEnablingPacketLossAtHighBw), true);
  CheckDecision(&states, rtc::Optional<int>(kDisablingBandwidthHigh),
                rtc::Optional<float>(kDisablingPacketLossAtHighBw * 1.01f),
                true);
  CheckDecision(&states, rtc::Optional<int>(kDisablingBandwidthHigh + 1),
                rtc::Optional<float>(0.0), false);
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST(FecControllerDeathTest, InvalidConfig) {
  FecControllerStates states;
  std::unique_ptr<MockSmoothingFilter> mock_smoothing_filter(
      new NiceMock<MockSmoothingFilter>());
  states.packet_loss_smoothed = mock_smoothing_filter.get();
  EXPECT_CALL(*states.packet_loss_smoothed, Die());
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
