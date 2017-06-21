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

#include "webrtc/logging/rtc_event_log/mock/mock_rtc_event_log.h"
#include "webrtc/modules/audio_coding/audio_network_adaptor/event_log_writer.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

namespace {

constexpr int kMinBitrateChangeBps = 5000;
constexpr float kMinPacketLossChangeFraction = 0.5;
constexpr float kMinBitrateChangeFraction = 0.25;

constexpr int kHighBitrateBps = 70000;
constexpr int kLowBitrateBps = 10000;
constexpr int kFrameLengthMs = 60;
constexpr bool kEnableFec = true;
constexpr bool kEnableDtx = true;
constexpr float kPacketLossFraction = 0.05f;
constexpr size_t kNumChannels = 1;

MATCHER_P(EncoderRuntimeConfigIs, config, "") {
  return arg.bitrate_bps == config.bitrate_bps &&
         arg.frame_length_ms == config.frame_length_ms &&
         arg.uplink_packet_loss_fraction ==
             config.uplink_packet_loss_fraction &&
         arg.enable_fec == config.enable_fec &&
         arg.enable_dtx == config.enable_dtx &&
         arg.num_channels == config.num_channels;
}

struct EventLogWriterStates {
  std::unique_ptr<EventLogWriter> event_log_writer;
  std::unique_ptr<testing::StrictMock<MockRtcEventLog>> event_log;
  AudioEncoderRuntimeConfig runtime_config;
};

EventLogWriterStates CreateEventLogWriter() {
  EventLogWriterStates state;
  state.event_log.reset(new testing::StrictMock<MockRtcEventLog>());
  state.event_log_writer.reset(new EventLogWriter(
      state.event_log.get(), kMinBitrateChangeBps, kMinBitrateChangeFraction,
      kMinPacketLossChangeFraction));
  state.runtime_config.bitrate_bps = rtc::Optional<int>(kHighBitrateBps);
  state.runtime_config.frame_length_ms = rtc::Optional<int>(kFrameLengthMs);
  state.runtime_config.uplink_packet_loss_fraction =
      rtc::Optional<float>(kPacketLossFraction);
  state.runtime_config.enable_fec = rtc::Optional<bool>(kEnableFec);
  state.runtime_config.enable_dtx = rtc::Optional<bool>(kEnableDtx);
  state.runtime_config.num_channels = rtc::Optional<size_t>(kNumChannels);
  return state;
}
}  // namespace

TEST(EventLogWriterTest, FirstConfigIsLogged) {
  auto state = CreateEventLogWriter();
  EXPECT_CALL(
      *state.event_log,
      LogAudioNetworkAdaptation(EncoderRuntimeConfigIs(state.runtime_config)))
      .Times(1);
  state.event_log_writer->MaybeLogEncoderConfig(state.runtime_config);
}

TEST(EventLogWriterTest, SameConfigIsNotLogged) {
  auto state = CreateEventLogWriter();
  EXPECT_CALL(
      *state.event_log,
      LogAudioNetworkAdaptation(EncoderRuntimeConfigIs(state.runtime_config)))
      .Times(1);
  state.event_log_writer->MaybeLogEncoderConfig(state.runtime_config);
  state.event_log_writer->MaybeLogEncoderConfig(state.runtime_config);
}

TEST(EventLogWriterTest, LogFecStateChange) {
  auto state = CreateEventLogWriter();
  EXPECT_CALL(
      *state.event_log,
      LogAudioNetworkAdaptation(EncoderRuntimeConfigIs(state.runtime_config)))
      .Times(1);
  state.event_log_writer->MaybeLogEncoderConfig(state.runtime_config);

  state.runtime_config.enable_fec = rtc::Optional<bool>(!kEnableFec);
  EXPECT_CALL(
      *state.event_log,
      LogAudioNetworkAdaptation(EncoderRuntimeConfigIs(state.runtime_config)))
      .Times(1);
  state.event_log_writer->MaybeLogEncoderConfig(state.runtime_config);
}

TEST(EventLogWriterTest, LogDtxStateChange) {
  auto state = CreateEventLogWriter();
  EXPECT_CALL(
      *state.event_log,
      LogAudioNetworkAdaptation(EncoderRuntimeConfigIs(state.runtime_config)))
      .Times(1);
  state.event_log_writer->MaybeLogEncoderConfig(state.runtime_config);

  state.runtime_config.enable_dtx = rtc::Optional<bool>(!kEnableDtx);
  EXPECT_CALL(
      *state.event_log,
      LogAudioNetworkAdaptation(EncoderRuntimeConfigIs(state.runtime_config)))
      .Times(1);
  state.event_log_writer->MaybeLogEncoderConfig(state.runtime_config);
}

TEST(EventLogWriterTest, LogChannelChange) {
  auto state = CreateEventLogWriter();
  EXPECT_CALL(
      *state.event_log,
      LogAudioNetworkAdaptation(EncoderRuntimeConfigIs(state.runtime_config)))
      .Times(1);
  state.event_log_writer->MaybeLogEncoderConfig(state.runtime_config);

  state.runtime_config.num_channels = rtc::Optional<size_t>(kNumChannels + 1);
  EXPECT_CALL(
      *state.event_log,
      LogAudioNetworkAdaptation(EncoderRuntimeConfigIs(state.runtime_config)))
      .Times(1);
  state.event_log_writer->MaybeLogEncoderConfig(state.runtime_config);
}

TEST(EventLogWriterTest, LogFrameLengthChange) {
  auto state = CreateEventLogWriter();
  EXPECT_CALL(
      *state.event_log,
      LogAudioNetworkAdaptation(EncoderRuntimeConfigIs(state.runtime_config)))
      .Times(1);
  state.event_log_writer->MaybeLogEncoderConfig(state.runtime_config);

  state.runtime_config.frame_length_ms = rtc::Optional<int>(20);
  EXPECT_CALL(
      *state.event_log,
      LogAudioNetworkAdaptation(EncoderRuntimeConfigIs(state.runtime_config)))
      .Times(1);
  state.event_log_writer->MaybeLogEncoderConfig(state.runtime_config);
}

TEST(EventLogWriterTest, DoNotLogSmallBitrateChange) {
  auto state = CreateEventLogWriter();
  EXPECT_CALL(
      *state.event_log,
      LogAudioNetworkAdaptation(EncoderRuntimeConfigIs(state.runtime_config)))
      .Times(1);
  state.event_log_writer->MaybeLogEncoderConfig(state.runtime_config);
  state.runtime_config.bitrate_bps =
      rtc::Optional<int>(kHighBitrateBps + kMinBitrateChangeBps - 1);
  state.event_log_writer->MaybeLogEncoderConfig(state.runtime_config);
}

TEST(EventLogWriterTest, LogLargeBitrateChange) {
  auto state = CreateEventLogWriter();
  EXPECT_CALL(
      *state.event_log,
      LogAudioNetworkAdaptation(EncoderRuntimeConfigIs(state.runtime_config)))
      .Times(1);
  state.event_log_writer->MaybeLogEncoderConfig(state.runtime_config);
  // At high bitrate, the min fraction rule requires a larger change than the
  // min change rule. We make sure that the min change rule applies.
  RTC_DCHECK_GT(kHighBitrateBps * kMinBitrateChangeFraction,
                kMinBitrateChangeBps);
  state.runtime_config.bitrate_bps =
      rtc::Optional<int>(kHighBitrateBps + kMinBitrateChangeBps);
  EXPECT_CALL(
      *state.event_log,
      LogAudioNetworkAdaptation(EncoderRuntimeConfigIs(state.runtime_config)))
      .Times(1);
  state.event_log_writer->MaybeLogEncoderConfig(state.runtime_config);
}

TEST(EventLogWriterTest, LogMinBitrateChangeFractionOnLowBitrateChange) {
  auto state = CreateEventLogWriter();
  state.runtime_config.bitrate_bps = rtc::Optional<int>(kLowBitrateBps);
  EXPECT_CALL(
      *state.event_log,
      LogAudioNetworkAdaptation(EncoderRuntimeConfigIs(state.runtime_config)))
      .Times(1);
  state.event_log_writer->MaybeLogEncoderConfig(state.runtime_config);
  // At high bitrate, the min change rule requires a larger change than the min
  // fraction rule. We make sure that the min fraction rule applies.
  state.runtime_config.bitrate_bps = rtc::Optional<int>(
      kLowBitrateBps + kLowBitrateBps * kMinBitrateChangeFraction);
  EXPECT_CALL(
      *state.event_log,
      LogAudioNetworkAdaptation(EncoderRuntimeConfigIs(state.runtime_config)))
      .Times(1);
  state.event_log_writer->MaybeLogEncoderConfig(state.runtime_config);
}

TEST(EventLogWriterTest, DoNotLogSmallPacketLossFractionChange) {
  auto state = CreateEventLogWriter();
  EXPECT_CALL(
      *state.event_log,
      LogAudioNetworkAdaptation(EncoderRuntimeConfigIs(state.runtime_config)))
      .Times(1);
  state.event_log_writer->MaybeLogEncoderConfig(state.runtime_config);
  state.runtime_config.uplink_packet_loss_fraction = rtc::Optional<float>(
      kPacketLossFraction + kMinPacketLossChangeFraction * kPacketLossFraction -
      0.001f);
  state.event_log_writer->MaybeLogEncoderConfig(state.runtime_config);
}

TEST(EventLogWriterTest, LogLargePacketLossFractionChange) {
  auto state = CreateEventLogWriter();
  EXPECT_CALL(
      *state.event_log,
      LogAudioNetworkAdaptation(EncoderRuntimeConfigIs(state.runtime_config)))
      .Times(1);
  state.event_log_writer->MaybeLogEncoderConfig(state.runtime_config);
  state.runtime_config.uplink_packet_loss_fraction = rtc::Optional<float>(
      kPacketLossFraction + kMinPacketLossChangeFraction * kPacketLossFraction);
  EXPECT_CALL(
      *state.event_log,
      LogAudioNetworkAdaptation(EncoderRuntimeConfigIs(state.runtime_config)))
      .Times(1);
  state.event_log_writer->MaybeLogEncoderConfig(state.runtime_config);
}

TEST(EventLogWriterTest, LogJustOnceOnMultipleChanges) {
  auto state = CreateEventLogWriter();
  EXPECT_CALL(
      *state.event_log,
      LogAudioNetworkAdaptation(EncoderRuntimeConfigIs(state.runtime_config)))
      .Times(1);
  state.event_log_writer->MaybeLogEncoderConfig(state.runtime_config);
  state.runtime_config.uplink_packet_loss_fraction = rtc::Optional<float>(
      kPacketLossFraction + kMinPacketLossChangeFraction * kPacketLossFraction);
  state.runtime_config.frame_length_ms = rtc::Optional<int>(20);
  EXPECT_CALL(
      *state.event_log,
      LogAudioNetworkAdaptation(EncoderRuntimeConfigIs(state.runtime_config)))
      .Times(1);
  state.event_log_writer->MaybeLogEncoderConfig(state.runtime_config);
}

TEST(EventLogWriterTest, LogAfterGradualChange) {
  auto state = CreateEventLogWriter();
  EXPECT_CALL(
      *state.event_log,
      LogAudioNetworkAdaptation(EncoderRuntimeConfigIs(state.runtime_config)))
      .Times(1);
  state.event_log_writer->MaybeLogEncoderConfig(state.runtime_config);
  state.runtime_config.bitrate_bps =
      rtc::Optional<int>(kHighBitrateBps + kMinBitrateChangeBps);
  EXPECT_CALL(
      *state.event_log,
      LogAudioNetworkAdaptation(EncoderRuntimeConfigIs(state.runtime_config)))
      .Times(1);
  for (int bitrate_bps = kHighBitrateBps;
       bitrate_bps <= kHighBitrateBps + kMinBitrateChangeBps; bitrate_bps++) {
    state.runtime_config.bitrate_bps = rtc::Optional<int>(bitrate_bps);
    state.event_log_writer->MaybeLogEncoderConfig(state.runtime_config);
  }
}
}  // namespace webrtc
