/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <random>
#include <utility>

#include "modules/audio_coding/audio_network_adaptor/fec_controller_rplr_based.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

// The test uses the following settings:
//
// recoverable ^
// packet-loss |   |  |
//             |  A| C|   FEC
//             |    \  \   ON
//             | FEC \ D\_______
//             | OFF B\_________
//             |-----------------> bandwidth
//
// A : (kDisablingBandwidthLow, kDisablingRecoverablePacketLossAtLowBw)
// B : (kDisablingBandwidthHigh, kDisablingRecoverablePacketLossAtHighBw)
// C : (kEnablingBandwidthLow, kEnablingRecoverablePacketLossAtLowBw)
// D : (kEnablingBandwidthHigh, kEnablingRecoverablePacketLossAtHighBw)

constexpr int kDisablingBandwidthLow = 15000;
constexpr float kDisablingRecoverablePacketLossAtLowBw = 0.08f;
constexpr int kDisablingBandwidthHigh = 64000;
constexpr float kDisablingRecoverablePacketLossAtHighBw = 0.01f;
constexpr int kEnablingBandwidthLow = 17000;
constexpr float kEnablingRecoverablePacketLossAtLowBw = 0.1f;
constexpr int kEnablingBandwidthHigh = 64000;
constexpr float kEnablingRecoverablePacketLossAtHighBw = 0.05f;

constexpr float kEpsilon = 1e-5f;

absl::optional<float> GetRandomProbabilityOrUnknown() {
  std::random_device rd;
  std::mt19937 generator(rd());
  std::uniform_real_distribution<> distribution(0, 1);

  return (distribution(generator) < 0.2)
             ? absl::nullopt
             : absl::optional<float>(distribution(generator));
}

std::unique_ptr<FecControllerRplrBased> CreateFecControllerRplrBased(
    bool initial_fec_enabled) {
  return std::unique_ptr<FecControllerRplrBased>(
      new FecControllerRplrBased(FecControllerRplrBased::Config(
          initial_fec_enabled,
          ThresholdCurve(
              kEnablingBandwidthLow, kEnablingRecoverablePacketLossAtLowBw,
              kEnablingBandwidthHigh, kEnablingRecoverablePacketLossAtHighBw),
          ThresholdCurve(kDisablingBandwidthLow,
                         kDisablingRecoverablePacketLossAtLowBw,
                         kDisablingBandwidthHigh,
                         kDisablingRecoverablePacketLossAtHighBw))));
}

void UpdateNetworkMetrics(
    FecControllerRplrBased* controller,
    const absl::optional<int>& uplink_bandwidth_bps,
    const absl::optional<float>& uplink_packet_loss,
    const absl::optional<float>& uplink_recoveralbe_packet_loss) {
  // UpdateNetworkMetrics can accept multiple network metric updates at once.
  // However, currently, the most used case is to update one metric at a time.
  // To reflect this fact, we separate the calls.
  if (uplink_bandwidth_bps) {
    Controller::NetworkMetrics network_metrics;
    network_metrics.uplink_bandwidth_bps = uplink_bandwidth_bps;
    controller->UpdateNetworkMetrics(network_metrics);
  }
  if (uplink_packet_loss) {
    Controller::NetworkMetrics network_metrics;
    network_metrics.uplink_packet_loss_fraction = uplink_packet_loss;
    controller->UpdateNetworkMetrics(network_metrics);
  }
  if (uplink_recoveralbe_packet_loss) {
    Controller::NetworkMetrics network_metrics;
    network_metrics.uplink_recoverable_packet_loss_fraction =
        uplink_recoveralbe_packet_loss;
    controller->UpdateNetworkMetrics(network_metrics);
  }
}

void UpdateNetworkMetrics(
    FecControllerRplrBased* controller,
    const absl::optional<int>& uplink_bandwidth_bps,
    const absl::optional<float>& uplink_recoveralbe_packet_loss) {
  // FecControllerRplrBased doesn't currently use the PLR (general packet-loss
  // rate) at all. (This might be changed in the future.) The unit-tests will
  // use a random value (including unknown), to show this does not interfere.
  UpdateNetworkMetrics(controller, uplink_bandwidth_bps,
                       GetRandomProbabilityOrUnknown(),
                       uplink_recoveralbe_packet_loss);
}

// Checks that the FEC decision and |uplink_packet_loss_fraction| given by
// |states->controller->MakeDecision| matches |expected_enable_fec| and
// |expected_uplink_packet_loss_fraction|, respectively.
void CheckDecision(FecControllerRplrBased* controller,
                   bool expected_enable_fec,
                   float expected_uplink_packet_loss_fraction) {
  AudioEncoderRuntimeConfig config;
  controller->MakeDecision(&config);

  // Less compact than comparing optionals, but yields more readable errors.
  EXPECT_TRUE(config.enable_fec);
  if (config.enable_fec) {
    EXPECT_EQ(expected_enable_fec, *config.enable_fec);
  }
  EXPECT_TRUE(config.uplink_packet_loss_fraction);
  if (config.uplink_packet_loss_fraction) {
    EXPECT_EQ(expected_uplink_packet_loss_fraction,
              *config.uplink_packet_loss_fraction);
  }
}

}  // namespace

TEST(FecControllerRplrBasedTest, OutputInitValueBeforeAnyInputsAreReceived) {
  for (bool initial_fec_enabled : {false, true}) {
    auto controller = CreateFecControllerRplrBased(initial_fec_enabled);
    CheckDecision(controller.get(), initial_fec_enabled, 0);
  }
}

TEST(FecControllerRplrBasedTest, OutputInitValueWhenUplinkBandwidthUnknown) {
  // Regardless of the initial FEC state and the recoverable-packet-loss
  // rate, the initial FEC state is maintained as long as the BWE is unknown.
  for (bool initial_fec_enabled : {false, true}) {
    for (float recoverable_packet_loss :
         {kDisablingRecoverablePacketLossAtHighBw - kEpsilon,
          kDisablingRecoverablePacketLossAtHighBw,
          kDisablingRecoverablePacketLossAtHighBw + kEpsilon,
          kEnablingRecoverablePacketLossAtHighBw - kEpsilon,
          kEnablingRecoverablePacketLossAtHighBw,
          kEnablingRecoverablePacketLossAtHighBw + kEpsilon}) {
      auto controller = CreateFecControllerRplrBased(initial_fec_enabled);
      UpdateNetworkMetrics(controller.get(), absl::nullopt,
                           recoverable_packet_loss);
      CheckDecision(controller.get(), initial_fec_enabled,
                    recoverable_packet_loss);
    }
  }
}

TEST(FecControllerRplrBasedTest,
     OutputInitValueWhenUplinkRecoverablePacketLossFractionUnknown) {
  // Regardless of the initial FEC state and the BWE, the initial FEC state
  // is maintained as long as the recoverable-packet-loss rate is unknown.
  for (bool initial_fec_enabled : {false, true}) {
    for (int bandwidth : {kDisablingBandwidthLow - 1, kDisablingBandwidthLow,
                          kDisablingBandwidthLow + 1, kEnablingBandwidthLow - 1,
                          kEnablingBandwidthLow, kEnablingBandwidthLow + 1}) {
      auto controller = CreateFecControllerRplrBased(initial_fec_enabled);
      UpdateNetworkMetrics(controller.get(), bandwidth, absl::nullopt);
      CheckDecision(controller.get(), initial_fec_enabled, 0.0);
    }
  }
}

TEST(FecControllerRplrBasedTest, EnableFecForHighBandwidth) {
  auto controller = CreateFecControllerRplrBased(false);
  UpdateNetworkMetrics(controller.get(), kEnablingBandwidthHigh,
                       kEnablingRecoverablePacketLossAtHighBw);
  CheckDecision(controller.get(), true, kEnablingRecoverablePacketLossAtHighBw);
}

TEST(FecControllerRplrBasedTest, UpdateMultipleNetworkMetricsAtOnce) {
  // This test is similar to EnableFecForHighBandwidth. But instead of
  // using ::UpdateNetworkMetrics(...), which calls
  // FecControllerRplrBasedTest::UpdateNetworkMetrics(...) multiple times, we
  // we call it only once. This is to verify that
  // FecControllerRplrBasedTest::UpdateNetworkMetrics(...) can handle multiple
  // network updates at once. This is, however, not a common use case in current
  // audio_network_adaptor_impl.cc.
  auto controller = CreateFecControllerRplrBased(false);
  Controller::NetworkMetrics network_metrics;
  network_metrics.uplink_bandwidth_bps = kEnablingBandwidthHigh;
  network_metrics.uplink_packet_loss_fraction = GetRandomProbabilityOrUnknown();
  network_metrics.uplink_recoverable_packet_loss_fraction =
      kEnablingRecoverablePacketLossAtHighBw;
  controller->UpdateNetworkMetrics(network_metrics);
  CheckDecision(controller.get(), true, kEnablingRecoverablePacketLossAtHighBw);
}

TEST(FecControllerRplrBasedTest, MaintainFecOffForHighBandwidth) {
  auto controller = CreateFecControllerRplrBased(false);
  constexpr float kRecoverablePacketLoss =
      kEnablingRecoverablePacketLossAtHighBw * 0.99f;
  UpdateNetworkMetrics(controller.get(), kEnablingBandwidthHigh,
                       kRecoverablePacketLoss);
  CheckDecision(controller.get(), false, kRecoverablePacketLoss);
}

TEST(FecControllerRplrBasedTest, EnableFecForMediumBandwidth) {
  auto controller = CreateFecControllerRplrBased(false);
  constexpr float kRecoverablePacketLoss =
      (kEnablingRecoverablePacketLossAtLowBw +
       kEnablingRecoverablePacketLossAtHighBw) /
      2.0;
  UpdateNetworkMetrics(controller.get(),
                       (kEnablingBandwidthHigh + kEnablingBandwidthLow) / 2,
                       kRecoverablePacketLoss);
  CheckDecision(controller.get(), true, kRecoverablePacketLoss);
}

TEST(FecControllerRplrBasedTest, MaintainFecOffForMediumBandwidth) {
  auto controller = CreateFecControllerRplrBased(false);
  constexpr float kRecoverablePacketLoss =
      kEnablingRecoverablePacketLossAtLowBw * 0.49f +
      kEnablingRecoverablePacketLossAtHighBw * 0.51f;
  UpdateNetworkMetrics(controller.get(),
                       (kEnablingBandwidthHigh + kEnablingBandwidthLow) / 2,
                       kRecoverablePacketLoss);
  CheckDecision(controller.get(), false, kRecoverablePacketLoss);
}

TEST(FecControllerRplrBasedTest, EnableFecForLowBandwidth) {
  auto controller = CreateFecControllerRplrBased(false);
  UpdateNetworkMetrics(controller.get(), kEnablingBandwidthLow,
                       kEnablingRecoverablePacketLossAtLowBw);
  CheckDecision(controller.get(), true, kEnablingRecoverablePacketLossAtLowBw);
}

TEST(FecControllerRplrBasedTest, MaintainFecOffForLowBandwidth) {
  auto controller = CreateFecControllerRplrBased(false);
  constexpr float kRecoverablePacketLoss =
      kEnablingRecoverablePacketLossAtLowBw * 0.99f;
  UpdateNetworkMetrics(controller.get(), kEnablingBandwidthLow,
                       kRecoverablePacketLoss);
  CheckDecision(controller.get(), false, kRecoverablePacketLoss);
}

TEST(FecControllerRplrBasedTest, MaintainFecOffForVeryLowBandwidth) {
  auto controller = CreateFecControllerRplrBased(false);
  // Below |kEnablingBandwidthLow|, no recoverable packet loss fraction can
  // cause FEC to turn on.
  UpdateNetworkMetrics(controller.get(), kEnablingBandwidthLow - 1, 1.0);
  CheckDecision(controller.get(), false, 1.0);
}

TEST(FecControllerRplrBasedTest, DisableFecForHighBandwidth) {
  auto controller = CreateFecControllerRplrBased(true);
  constexpr float kRecoverablePacketLoss =
      kDisablingRecoverablePacketLossAtHighBw - kEpsilon;
  UpdateNetworkMetrics(controller.get(), kDisablingBandwidthHigh,
                       kRecoverablePacketLoss);
  CheckDecision(controller.get(), false, kRecoverablePacketLoss);
}

TEST(FecControllerRplrBasedTest, MaintainFecOnForHighBandwidth) {
  // Note: Disabling happens when the value is strictly below the threshold.
  auto controller = CreateFecControllerRplrBased(true);
  UpdateNetworkMetrics(controller.get(), kDisablingBandwidthHigh,
                       kDisablingRecoverablePacketLossAtHighBw);
  CheckDecision(controller.get(), true,
                kDisablingRecoverablePacketLossAtHighBw);
}

TEST(FecControllerRplrBasedTest, DisableFecOnMediumBandwidth) {
  auto controller = CreateFecControllerRplrBased(true);
  constexpr float kRecoverablePacketLoss =
      ((kDisablingRecoverablePacketLossAtLowBw +
        kDisablingRecoverablePacketLossAtHighBw) /
       2.0f) -
      kEpsilon;
  UpdateNetworkMetrics(controller.get(),
                       (kDisablingBandwidthHigh + kDisablingBandwidthLow) / 2,
                       kRecoverablePacketLoss);
  CheckDecision(controller.get(), false, kRecoverablePacketLoss);
}

TEST(FecControllerRplrBasedTest, MaintainFecOnForMediumBandwidth) {
  auto controller = CreateFecControllerRplrBased(true);
  constexpr float kRecoverablePacketLoss =
      kDisablingRecoverablePacketLossAtLowBw * 0.51f +
      kDisablingRecoverablePacketLossAtHighBw * 0.49f - kEpsilon;
  UpdateNetworkMetrics(controller.get(),
                       (kEnablingBandwidthHigh + kDisablingBandwidthLow) / 2,
                       kRecoverablePacketLoss);
  CheckDecision(controller.get(), true, kRecoverablePacketLoss);
}

TEST(FecControllerRplrBasedTest, DisableFecForLowBandwidth) {
  auto controller = CreateFecControllerRplrBased(true);
  constexpr float kRecoverablePacketLoss =
      kDisablingRecoverablePacketLossAtLowBw - kEpsilon;
  UpdateNetworkMetrics(controller.get(), kDisablingBandwidthLow,
                       kRecoverablePacketLoss);
  CheckDecision(controller.get(), false, kRecoverablePacketLoss);
}

TEST(FecControllerRplrBasedTest, DisableFecForVeryLowBandwidth) {
  auto controller = CreateFecControllerRplrBased(true);
  // Below |kEnablingBandwidthLow|, any recoverable packet loss fraction can
  // cause FEC to turn off.
  UpdateNetworkMetrics(controller.get(), kDisablingBandwidthLow - 1, 1.0);
  CheckDecision(controller.get(), false, 1.0);
}

TEST(FecControllerRplrBasedTest, CheckBehaviorOnChangingNetworkMetrics) {
  // In this test, we let the network metrics to traverse from 1 to 5.
  //
  // recoverable ^
  // packet-loss | 1 |  |
  //             |   | 2|
  //             |    \  \ 3
  //             |     \4 \_______
  //             |      \_________
  //             |---------5-------> bandwidth

  auto controller = CreateFecControllerRplrBased(true);
  UpdateNetworkMetrics(controller.get(), kDisablingBandwidthLow - 1, 1.0);
  CheckDecision(controller.get(), false, 1.0);

  UpdateNetworkMetrics(controller.get(), kEnablingBandwidthLow,
                       kEnablingRecoverablePacketLossAtLowBw * 0.99f);
  CheckDecision(controller.get(), false,
                kEnablingRecoverablePacketLossAtLowBw * 0.99f);

  UpdateNetworkMetrics(controller.get(), kEnablingBandwidthHigh,
                       kEnablingRecoverablePacketLossAtHighBw);
  CheckDecision(controller.get(), true, kEnablingRecoverablePacketLossAtHighBw);

  UpdateNetworkMetrics(controller.get(), kDisablingBandwidthHigh,
                       kDisablingRecoverablePacketLossAtHighBw);
  CheckDecision(controller.get(), true,
                kDisablingRecoverablePacketLossAtHighBw);

  UpdateNetworkMetrics(controller.get(), kDisablingBandwidthHigh + 1, 0.0);
  CheckDecision(controller.get(), false, 0.0);
}

TEST(FecControllerRplrBasedTest, CheckBehaviorOnSpecialCurves) {
  // We test a special configuration, where the points to define the FEC
  // enabling/disabling curves are placed like the following, otherwise the test
  // is the same as CheckBehaviorOnChangingNetworkMetrics.
  //
  // recoverable ^
  // packet-loss |   |  |
  //             |   | C|
  //             |   |  |
  //             |   | D|_______
  //             |  A|___B______
  //             |-----------------> bandwidth

  constexpr int kEnablingBandwidthHigh = kEnablingBandwidthLow;
  constexpr float kDisablingRecoverablePacketLossAtLowBw =
      kDisablingRecoverablePacketLossAtHighBw;
  FecControllerRplrBased controller(FecControllerRplrBased::Config(
      true,
      ThresholdCurve(
          kEnablingBandwidthLow, kEnablingRecoverablePacketLossAtLowBw,
          kEnablingBandwidthHigh, kEnablingRecoverablePacketLossAtHighBw),
      ThresholdCurve(
          kDisablingBandwidthLow, kDisablingRecoverablePacketLossAtLowBw,
          kDisablingBandwidthHigh, kDisablingRecoverablePacketLossAtHighBw)));

  UpdateNetworkMetrics(&controller, kDisablingBandwidthLow - 1, 1.0);
  CheckDecision(&controller, false, 1.0);

  UpdateNetworkMetrics(&controller, kEnablingBandwidthLow,
                       kEnablingRecoverablePacketLossAtHighBw * 0.99f);
  CheckDecision(&controller, false,
                kEnablingRecoverablePacketLossAtHighBw * 0.99f);

  UpdateNetworkMetrics(&controller, kEnablingBandwidthHigh,
                       kEnablingRecoverablePacketLossAtHighBw);
  CheckDecision(&controller, true, kEnablingRecoverablePacketLossAtHighBw);

  UpdateNetworkMetrics(&controller, kDisablingBandwidthHigh,
                       kDisablingRecoverablePacketLossAtHighBw);
  CheckDecision(&controller, true, kDisablingRecoverablePacketLossAtHighBw);

  UpdateNetworkMetrics(&controller, kDisablingBandwidthHigh + 1, 0.0);
  CheckDecision(&controller, false, 0.0);
}

TEST(FecControllerRplrBasedTest, SingleThresholdCurveForEnablingAndDisabling) {
  // Note: To avoid numerical errors, keep kRecoverablePacketLossAtLowBw and
  // kRecoverablePacketLossAthighBw as (negative) integer powers of 2.
  // This is mostly relevant for the O3 case.
  constexpr int kBandwidthLow = 10000;
  constexpr float kRecoverablePacketLossAtLowBw = 0.25f;
  constexpr int kBandwidthHigh = 20000;
  constexpr float kRecoverablePacketLossAtHighBw = 0.125f;
  auto curve = ThresholdCurve(kBandwidthLow, kRecoverablePacketLossAtLowBw,
                              kBandwidthHigh, kRecoverablePacketLossAtHighBw);

  // B* stands for "below-curve", O* for "on-curve", and A* for "above-curve".
  //
  //                                            //
  // recoverable ^                              //
  // packet-loss |    |                         //
  //             | B1 O1                        //
  //             |    |                         //
  //             |    O2                        //
  //             |     \ A1                     //
  //             |      \                       //
  //             |       O3   A2                //
  //             |     B2 \                     //
  //             |         \                    //
  //             |          O4--O5----          //
  //             |                              //
  //             |            B3                //
  //             |-----------------> bandwidth  //

  struct NetworkState {
    int bandwidth;
    float recoverable_packet_loss;
  };

  std::vector<NetworkState> below{
      {kBandwidthLow - 1, kRecoverablePacketLossAtLowBw + 0.1f},  // B1
      {(kBandwidthLow + kBandwidthHigh) / 2,
       (kRecoverablePacketLossAtLowBw + kRecoverablePacketLossAtHighBw) / 2 -
           kEpsilon},                                                  // B2
      {kBandwidthHigh + 1, kRecoverablePacketLossAtHighBw - kEpsilon}  // B3
  };

  std::vector<NetworkState> on{
      {kBandwidthLow, kRecoverablePacketLossAtLowBw + 0.1f},  // O1
      {kBandwidthLow, kRecoverablePacketLossAtLowBw},         // O2
      {(kBandwidthLow + kBandwidthHigh) / 2,
       (kRecoverablePacketLossAtLowBw + kRecoverablePacketLossAtHighBw) /
           2},                                               // O3
      {kBandwidthHigh, kRecoverablePacketLossAtHighBw},      // O4
      {kBandwidthHigh + 1, kRecoverablePacketLossAtHighBw},  // O5
  };

  std::vector<NetworkState> above{
      {(kBandwidthLow + kBandwidthHigh) / 2,
       (kRecoverablePacketLossAtLowBw + kRecoverablePacketLossAtHighBw) / 2 +
           kEpsilon},                                                   // A1
      {kBandwidthHigh + 1, kRecoverablePacketLossAtHighBw + kEpsilon},  // A2
  };

  // Test that FEC is turned off whenever we're below the curve, independent
  // of the starting FEC state.
  for (NetworkState net_state : below) {
    for (bool initial_fec_enabled : {false, true}) {
      FecControllerRplrBased controller(
          FecControllerRplrBased::Config(initial_fec_enabled, curve, curve));
      UpdateNetworkMetrics(&controller, net_state.bandwidth,
                           net_state.recoverable_packet_loss);
      CheckDecision(&controller, false, net_state.recoverable_packet_loss);
    }
  }

  // Test that FEC is turned on whenever we're on the curve or above it,
  // independent of the starting FEC state.
  for (std::vector<NetworkState> states_list : {on, above}) {
    for (NetworkState net_state : states_list) {
      for (bool initial_fec_enabled : {false, true}) {
        FecControllerRplrBased controller(
            FecControllerRplrBased::Config(initial_fec_enabled, curve, curve));
        UpdateNetworkMetrics(&controller, net_state.bandwidth,
                             net_state.recoverable_packet_loss);
        CheckDecision(&controller, true, net_state.recoverable_packet_loss);
      }
    }
  }
}

TEST(FecControllerRplrBasedTest, FecAlwaysOff) {
  ThresholdCurve always_off_curve(0, 1.0f + kEpsilon, 0, 1.0f + kEpsilon);
  for (bool initial_fec_enabled : {false, true}) {
    for (int bandwidth : {0, 10000}) {
      for (float recoverable_packet_loss : {0.0f, 0.5f, 1.0f}) {
        FecControllerRplrBased controller(FecControllerRplrBased::Config(
            initial_fec_enabled, always_off_curve, always_off_curve));
        UpdateNetworkMetrics(&controller, bandwidth, recoverable_packet_loss);
        CheckDecision(&controller, false, recoverable_packet_loss);
      }
    }
  }
}

TEST(FecControllerRplrBasedTest, FecAlwaysOn) {
  ThresholdCurve always_on_curve(0, 0.0f, 0, 0.0f);
  for (bool initial_fec_enabled : {false, true}) {
    for (int bandwidth : {0, 10000}) {
      for (float recoverable_packet_loss : {0.0f, 0.5f, 1.0f}) {
        FecControllerRplrBased controller(FecControllerRplrBased::Config(
            initial_fec_enabled, always_on_curve, always_on_curve));
        UpdateNetworkMetrics(&controller, bandwidth, recoverable_packet_loss);
        CheckDecision(&controller, true, recoverable_packet_loss);
      }
    }
  }
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST(FecControllerRplrBasedDeathTest, InvalidConfig) {
  EXPECT_DEATH(
      FecControllerRplrBased controller(FecControllerRplrBased::Config(
          true,
          ThresholdCurve(
              kDisablingBandwidthLow - 1, kEnablingRecoverablePacketLossAtLowBw,
              kEnablingBandwidthHigh, kEnablingRecoverablePacketLossAtHighBw),
          ThresholdCurve(kDisablingBandwidthLow,
                         kDisablingRecoverablePacketLossAtLowBw,
                         kDisablingBandwidthHigh,
                         kDisablingRecoverablePacketLossAtHighBw))),
      "Check failed");
}
#endif
}  // namespace webrtc
