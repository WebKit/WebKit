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
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace audio_network_adaptor {

namespace {

// L2(14B) + IPv4(20B) + UDP(8B) + RTP(12B) + SRTP_AUTH(10B) = 64B = 512 bits
constexpr int kPacketOverheadBits = 512;

void CheckDecision(BitrateController* controller,
                   const rtc::Optional<int>& target_audio_bitrate_bps,
                   const rtc::Optional<int>& frame_length_ms,
                   int expected_bitrate_bps) {
  Controller::NetworkMetrics metrics;
  metrics.target_audio_bitrate_bps = target_audio_bitrate_bps;
  AudioNetworkAdaptor::EncoderRuntimeConfig config;
  config.frame_length_ms = frame_length_ms;
  controller->MakeDecision(metrics, &config);
  EXPECT_EQ(rtc::Optional<int>(expected_bitrate_bps), config.bitrate_bps);
}

}  // namespace

// These tests are named AnaBitrateControllerTest to distinguish from
// BitrateControllerTest in
// modules/bitrate_controller/bitrate_controller_unittest.cc.

TEST(AnaBitrateControllerTest, OutputInitValueWhenTargetBitrateUnknown) {
  constexpr int kInitialBitrateBps = 32000;
  constexpr int kInitialFrameLengthMs = 20;
  BitrateController controller(
      BitrateController::Config(kInitialBitrateBps, kInitialFrameLengthMs));
  CheckDecision(&controller, rtc::Optional<int>(),
                rtc::Optional<int>(kInitialFrameLengthMs * 2),
                kInitialBitrateBps);
}

TEST(AnaBitrateControllerTest, ChangeBitrateOnTargetBitrateChanged) {
  constexpr int kInitialBitrateBps = 32000;
  constexpr int kInitialFrameLengthMs = 20;
  BitrateController controller(
      BitrateController::Config(kInitialBitrateBps, kInitialFrameLengthMs));
  constexpr int kTargetBitrateBps = 48000;
  // Frame length unchanged, bitrate changes in accordance with
  // |metrics.target_audio_bitrate_bps|
  CheckDecision(&controller, rtc::Optional<int>(kTargetBitrateBps),
                rtc::Optional<int>(kInitialFrameLengthMs), kTargetBitrateBps);
}

TEST(AnaBitrateControllerTest, TreatUnknownFrameLengthAsFrameLengthUnchanged) {
  constexpr int kInitialBitrateBps = 32000;
  constexpr int kInitialFrameLengthMs = 20;
  BitrateController controller(
      BitrateController::Config(kInitialBitrateBps, kInitialFrameLengthMs));
  constexpr int kTargetBitrateBps = 48000;
  CheckDecision(&controller, rtc::Optional<int>(kTargetBitrateBps),
                rtc::Optional<int>(), kTargetBitrateBps);
}

TEST(AnaBitrateControllerTest, IncreaseBitrateOnFrameLengthIncreased) {
  constexpr int kInitialBitrateBps = 32000;
  constexpr int kInitialFrameLengthMs = 20;
  BitrateController controller(
      BitrateController::Config(kInitialBitrateBps, kInitialFrameLengthMs));
  constexpr int kFrameLengthMs = 60;
  constexpr int kPacketOverheadRateDiff =
      kPacketOverheadBits * 1000 / kInitialFrameLengthMs -
      kPacketOverheadBits * 1000 / kFrameLengthMs;
  CheckDecision(&controller, rtc::Optional<int>(kInitialBitrateBps),
                rtc::Optional<int>(kFrameLengthMs),
                kInitialBitrateBps + kPacketOverheadRateDiff);
}

TEST(AnaBitrateControllerTest, DecreaseBitrateOnFrameLengthDecreased) {
  constexpr int kInitialBitrateBps = 32000;
  constexpr int kInitialFrameLengthMs = 60;
  BitrateController controller(
      BitrateController::Config(kInitialBitrateBps, kInitialFrameLengthMs));
  constexpr int kFrameLengthMs = 20;
  constexpr int kPacketOverheadRateDiff =
      kPacketOverheadBits * 1000 / kInitialFrameLengthMs -
      kPacketOverheadBits * 1000 / kFrameLengthMs;
  CheckDecision(&controller, rtc::Optional<int>(kInitialBitrateBps),
                rtc::Optional<int>(kFrameLengthMs),
                kInitialBitrateBps + kPacketOverheadRateDiff);
}

TEST(AnaBitrateControllerTest, CheckBehaviorOnChangingCondition) {
  constexpr int kInitialBitrateBps = 32000;
  constexpr int kInitialFrameLengthMs = 20;
  BitrateController controller(
      BitrateController::Config(kInitialBitrateBps, kInitialFrameLengthMs));

  int last_overhead_bitrate =
      kPacketOverheadBits * 1000 / kInitialFrameLengthMs;
  int current_overhead_bitrate = kPacketOverheadBits * 1000 / 20;
  // Start from an arbitrary overall bitrate.
  int overall_bitrate = 34567;
  CheckDecision(
      &controller, rtc::Optional<int>(overall_bitrate - last_overhead_bitrate),
      rtc::Optional<int>(20), overall_bitrate - current_overhead_bitrate);

  // Next: increase overall bitrate.
  overall_bitrate += 100;
  CheckDecision(
      &controller, rtc::Optional<int>(overall_bitrate - last_overhead_bitrate),
      rtc::Optional<int>(20), overall_bitrate - current_overhead_bitrate);

  // Next: change frame length.
  current_overhead_bitrate = kPacketOverheadBits * 1000 / 60;
  CheckDecision(
      &controller, rtc::Optional<int>(overall_bitrate - last_overhead_bitrate),
      rtc::Optional<int>(60), overall_bitrate - current_overhead_bitrate);
  last_overhead_bitrate = current_overhead_bitrate;

  // Next: change frame length.
  current_overhead_bitrate = kPacketOverheadBits * 1000 / 20;
  CheckDecision(
      &controller, rtc::Optional<int>(overall_bitrate - last_overhead_bitrate),
      rtc::Optional<int>(20), overall_bitrate - current_overhead_bitrate);
  last_overhead_bitrate = current_overhead_bitrate;

  // Next: decrease overall bitrate and frame length.
  overall_bitrate -= 100;
  current_overhead_bitrate = kPacketOverheadBits * 1000 / 60;
  CheckDecision(
      &controller, rtc::Optional<int>(overall_bitrate - last_overhead_bitrate),
      rtc::Optional<int>(60), overall_bitrate - current_overhead_bitrate);
}

}  // namespace audio_network_adaptor
}  // namespace webrtc
