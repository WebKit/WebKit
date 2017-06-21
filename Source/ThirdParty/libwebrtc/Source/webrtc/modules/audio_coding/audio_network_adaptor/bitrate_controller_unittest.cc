/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/audio_network_adaptor/bitrate_controller.h"
#include "webrtc/test/field_trial.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace audio_network_adaptor {

namespace {

void UpdateNetworkMetrics(
    BitrateController* controller,
    const rtc::Optional<int>& target_audio_bitrate_bps,
    const rtc::Optional<size_t>& overhead_bytes_per_packet) {
  // UpdateNetworkMetrics can accept multiple network metric updates at once.
  // However, currently, the most used case is to update one metric at a time.
  // To reflect this fact, we separate the calls.
  if (target_audio_bitrate_bps) {
    Controller::NetworkMetrics network_metrics;
    network_metrics.target_audio_bitrate_bps = target_audio_bitrate_bps;
    controller->UpdateNetworkMetrics(network_metrics);
  }
  if (overhead_bytes_per_packet) {
    Controller::NetworkMetrics network_metrics;
    network_metrics.overhead_bytes_per_packet = overhead_bytes_per_packet;
    controller->UpdateNetworkMetrics(network_metrics);
  }
}

void CheckDecision(BitrateController* controller,
                   const rtc::Optional<int>& frame_length_ms,
                   int expected_bitrate_bps) {
  AudioEncoderRuntimeConfig config;
  config.frame_length_ms = frame_length_ms;
  controller->MakeDecision(&config);
  EXPECT_EQ(rtc::Optional<int>(expected_bitrate_bps), config.bitrate_bps);
}

}  // namespace

// These tests are named AnaBitrateControllerTest to distinguish from
// BitrateControllerTest in
// modules/bitrate_controller/bitrate_controller_unittest.cc.

TEST(AnaBitrateControllerTest, OutputInitValueWhenTargetBitrateUnknown) {
  constexpr int kInitialBitrateBps = 32000;
  constexpr int kInitialFrameLengthMs = 20;
  constexpr size_t kOverheadBytesPerPacket = 64;
  BitrateController controller(
      BitrateController::Config(kInitialBitrateBps, kInitialFrameLengthMs));
  UpdateNetworkMetrics(&controller, rtc::Optional<int>(),
                       rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(&controller, rtc::Optional<int>(kInitialFrameLengthMs * 2),
                kInitialBitrateBps);
}

TEST(AnaBitrateControllerTest, OutputInitValueWhenOverheadUnknown) {
  constexpr int kInitialBitrateBps = 32000;
  constexpr int kInitialFrameLengthMs = 20;
  constexpr int kTargetBitrateBps = 48000;
  BitrateController controller(
      BitrateController::Config(kInitialBitrateBps, kInitialFrameLengthMs));
  UpdateNetworkMetrics(&controller, rtc::Optional<int>(kTargetBitrateBps),
                       rtc::Optional<size_t>());
  CheckDecision(&controller, rtc::Optional<int>(kInitialFrameLengthMs * 2),
                kInitialBitrateBps);
}

TEST(AnaBitrateControllerTest, ChangeBitrateOnTargetBitrateChanged) {
  test::ScopedFieldTrials override_field_trials(
      "WebRTC-SendSideBwe-WithOverhead/Enabled/");
  constexpr int kInitialFrameLengthMs = 20;
  BitrateController controller(
      BitrateController::Config(32000, kInitialFrameLengthMs));
  constexpr int kTargetBitrateBps = 48000;
  constexpr size_t kOverheadBytesPerPacket = 64;
  constexpr int kBitrateBps =
      kTargetBitrateBps -
      kOverheadBytesPerPacket * 8 * 1000 / kInitialFrameLengthMs;
  // Frame length unchanged, bitrate changes in accordance with
  // |metrics.target_audio_bitrate_bps| and |metrics.overhead_bytes_per_packet|.
  UpdateNetworkMetrics(&controller, rtc::Optional<int>(kTargetBitrateBps),
                       rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(&controller, rtc::Optional<int>(kInitialFrameLengthMs),
                kBitrateBps);
}

TEST(AnaBitrateControllerTest, UpdateMultipleNetworkMetricsAtOnce) {
  // This test is similar to ChangeBitrateOnTargetBitrateChanged. But instead of
  // using ::UpdateNetworkMetrics(...), which calls
  // BitrateController::UpdateNetworkMetrics(...) multiple times, we
  // we call it only once. This is to verify that
  // BitrateController::UpdateNetworkMetrics(...) can handle multiple
  // network updates at once. This is, however, not a common use case in current
  // audio_network_adaptor_impl.cc.
  test::ScopedFieldTrials override_field_trials(
      "WebRTC-SendSideBwe-WithOverhead/Enabled/");
  constexpr int kInitialFrameLengthMs = 20;
  BitrateController controller(
      BitrateController::Config(32000, kInitialFrameLengthMs));
  constexpr int kTargetBitrateBps = 48000;
  constexpr size_t kOverheadBytesPerPacket = 64;
  constexpr int kBitrateBps =
      kTargetBitrateBps -
      kOverheadBytesPerPacket * 8 * 1000 / kInitialFrameLengthMs;
  Controller::NetworkMetrics network_metrics;
  network_metrics.target_audio_bitrate_bps =
      rtc::Optional<int>(kTargetBitrateBps);
  network_metrics.overhead_bytes_per_packet =
      rtc::Optional<size_t>(kOverheadBytesPerPacket);
  controller.UpdateNetworkMetrics(network_metrics);
  CheckDecision(&controller, rtc::Optional<int>(kInitialFrameLengthMs),
                kBitrateBps);
}

TEST(AnaBitrateControllerTest, TreatUnknownFrameLengthAsFrameLengthUnchanged) {
  test::ScopedFieldTrials override_field_trials(
      "WebRTC-SendSideBwe-WithOverhead/Enabled/");
  constexpr int kInitialFrameLengthMs = 20;
  BitrateController controller(
      BitrateController::Config(32000, kInitialFrameLengthMs));
  constexpr int kTargetBitrateBps = 48000;
  constexpr size_t kOverheadBytesPerPacket = 64;
  constexpr int kBitrateBps =
      kTargetBitrateBps -
      kOverheadBytesPerPacket * 8 * 1000 / kInitialFrameLengthMs;
  UpdateNetworkMetrics(&controller, rtc::Optional<int>(kTargetBitrateBps),
                       rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(&controller, rtc::Optional<int>(), kBitrateBps);
}

TEST(AnaBitrateControllerTest, IncreaseBitrateOnFrameLengthIncreased) {
  test::ScopedFieldTrials override_field_trials(
      "WebRTC-SendSideBwe-WithOverhead/Enabled/");
  constexpr int kInitialFrameLengthMs = 20;
  BitrateController controller(
      BitrateController::Config(32000, kInitialFrameLengthMs));

  constexpr int kTargetBitrateBps = 48000;
  constexpr size_t kOverheadBytesPerPacket = 64;
  constexpr int kBitrateBps =
      kTargetBitrateBps -
      kOverheadBytesPerPacket * 8 * 1000 / kInitialFrameLengthMs;
  UpdateNetworkMetrics(&controller, rtc::Optional<int>(kTargetBitrateBps),
                       rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(&controller, rtc::Optional<int>(), kBitrateBps);

  constexpr int kFrameLengthMs = 60;
  constexpr size_t kPacketOverheadRateDiff =
      kOverheadBytesPerPacket * 8 * 1000 / 20 -
      kOverheadBytesPerPacket * 8 * 1000 / 60;
  UpdateNetworkMetrics(&controller, rtc::Optional<int>(kTargetBitrateBps),
                       rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(&controller, rtc::Optional<int>(kFrameLengthMs),
                kBitrateBps + kPacketOverheadRateDiff);
}

TEST(AnaBitrateControllerTest, DecreaseBitrateOnFrameLengthDecreased) {
  test::ScopedFieldTrials override_field_trials(
      "WebRTC-SendSideBwe-WithOverhead/Enabled/");
  constexpr int kInitialFrameLengthMs = 60;
  BitrateController controller(
      BitrateController::Config(32000, kInitialFrameLengthMs));

  constexpr int kTargetBitrateBps = 48000;
  constexpr size_t kOverheadBytesPerPacket = 64;
  constexpr int kBitrateBps =
      kTargetBitrateBps -
      kOverheadBytesPerPacket * 8 * 1000 / kInitialFrameLengthMs;
  UpdateNetworkMetrics(&controller, rtc::Optional<int>(kTargetBitrateBps),
                       rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(&controller, rtc::Optional<int>(), kBitrateBps);

  constexpr int kFrameLengthMs = 20;
  constexpr size_t kPacketOverheadRateDiff =
      kOverheadBytesPerPacket * 8 * 1000 / 20 -
      kOverheadBytesPerPacket * 8 * 1000 / 60;
  UpdateNetworkMetrics(&controller, rtc::Optional<int>(kTargetBitrateBps),
                       rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(&controller, rtc::Optional<int>(kFrameLengthMs),
                kBitrateBps - kPacketOverheadRateDiff);
}

TEST(AnaBitrateControllerTest, BitrateNeverBecomesNegative) {
  test::ScopedFieldTrials override_field_trials(
      "WebRTC-SendSideBwe-WithOverhead/Enabled/");
  BitrateController controller(BitrateController::Config(32000, 20));
  constexpr size_t kOverheadBytesPerPacket = 64;
  constexpr int kFrameLengthMs = 60;
  // Set a target rate smaller than overhead rate, the bitrate is bounded by 0.
  constexpr int kTargetBitrateBps =
      kOverheadBytesPerPacket * 8 * 1000 / kFrameLengthMs - 1;
  UpdateNetworkMetrics(&controller, rtc::Optional<int>(kTargetBitrateBps),
                       rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(&controller, rtc::Optional<int>(kFrameLengthMs), 0);
}

TEST(AnaBitrateControllerTest, CheckBehaviorOnChangingCondition) {
  test::ScopedFieldTrials override_field_trials(
      "WebRTC-SendSideBwe-WithOverhead/Enabled/");
  BitrateController controller(BitrateController::Config(32000, 20));

  // Start from an arbitrary overall bitrate.
  int overall_bitrate = 34567;
  size_t overhead_bytes_per_packet = 64;
  int frame_length_ms = 20;
  int current_bitrate =
      overall_bitrate - overhead_bytes_per_packet * 8 * 1000 / frame_length_ms;

  UpdateNetworkMetrics(&controller, rtc::Optional<int>(overall_bitrate),
                       rtc::Optional<size_t>(overhead_bytes_per_packet));
  CheckDecision(&controller, rtc::Optional<int>(frame_length_ms),
                current_bitrate);

  // Next: increase overall bitrate.
  overall_bitrate += 100;
  current_bitrate += 100;
  UpdateNetworkMetrics(&controller, rtc::Optional<int>(overall_bitrate),
                       rtc::Optional<size_t>(overhead_bytes_per_packet));
  CheckDecision(&controller, rtc::Optional<int>(frame_length_ms),
                current_bitrate);

  // Next: change frame length.
  frame_length_ms = 60;
  current_bitrate += overhead_bytes_per_packet * 8 * 1000 / 20 -
                     overhead_bytes_per_packet * 8 * 1000 / 60;
  UpdateNetworkMetrics(&controller, rtc::Optional<int>(overall_bitrate),
                       rtc::Optional<size_t>(overhead_bytes_per_packet));
  CheckDecision(&controller, rtc::Optional<int>(frame_length_ms),
                current_bitrate);

  // Next: change overhead.
  overhead_bytes_per_packet -= 30;
  current_bitrate += 30 * 8 * 1000 / frame_length_ms;
  UpdateNetworkMetrics(&controller, rtc::Optional<int>(overall_bitrate),
                       rtc::Optional<size_t>(overhead_bytes_per_packet));
  CheckDecision(&controller, rtc::Optional<int>(frame_length_ms),
                current_bitrate);

  // Next: change frame length.
  frame_length_ms = 20;
  current_bitrate -= overhead_bytes_per_packet * 8 * 1000 / 20 -
                     overhead_bytes_per_packet * 8 * 1000 / 60;
  UpdateNetworkMetrics(&controller, rtc::Optional<int>(overall_bitrate),
                       rtc::Optional<size_t>(overhead_bytes_per_packet));
  CheckDecision(&controller, rtc::Optional<int>(frame_length_ms),
                current_bitrate);

  // Next: decrease overall bitrate and frame length.
  overall_bitrate -= 100;
  current_bitrate -= 100;
  frame_length_ms = 60;
  current_bitrate += overhead_bytes_per_packet * 8 * 1000 / 20 -
                     overhead_bytes_per_packet * 8 * 1000 / 60;

  UpdateNetworkMetrics(&controller, rtc::Optional<int>(overall_bitrate),
                       rtc::Optional<size_t>(overhead_bytes_per_packet));
  CheckDecision(&controller, rtc::Optional<int>(frame_length_ms),
                current_bitrate);
}

}  // namespace audio_network_adaptor
}  // namespace webrtc
