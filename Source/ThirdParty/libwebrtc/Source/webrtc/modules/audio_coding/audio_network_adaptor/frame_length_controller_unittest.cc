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
#include <utility>

#include "webrtc/modules/audio_coding/audio_network_adaptor/frame_length_controller.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

namespace {

constexpr float kFlIncreasingPacketLossFraction = 0.04f;
constexpr float kFlDecreasingPacketLossFraction = 0.05f;
constexpr int kMinEncoderBitrateBps = 6000;
constexpr int kPreventOveruseMarginBps = 5000;
constexpr size_t kOverheadBytesPerPacket = 20;
constexpr int kFl20msTo60msBandwidthBps = 40000;
constexpr int kFl60msTo20msBandwidthBps = 50000;
constexpr int kFl60msTo120msBandwidthBps = 30000;
constexpr int kFl120msTo60msBandwidthBps = 40000;
constexpr int kMediumBandwidthBps =
    (kFl60msTo20msBandwidthBps + kFl20msTo60msBandwidthBps) / 2;
constexpr float kMediumPacketLossFraction =
    (kFlDecreasingPacketLossFraction + kFlIncreasingPacketLossFraction) / 2;

int VeryLowBitrate(int frame_length_ms) {
  return kMinEncoderBitrateBps + kPreventOveruseMarginBps +
         (kOverheadBytesPerPacket * 8 * 1000 / frame_length_ms);
}

std::unique_ptr<FrameLengthController> CreateController(
    const std::map<FrameLengthController::Config::FrameLengthChange, int>&
        frame_length_change_criteria,
    const std::vector<int>& encoder_frame_lengths_ms,
    int initial_frame_length_ms) {
  std::unique_ptr<FrameLengthController> controller(
      new FrameLengthController(FrameLengthController::Config(
          encoder_frame_lengths_ms, initial_frame_length_ms,
          kMinEncoderBitrateBps, kFlIncreasingPacketLossFraction,
          kFlDecreasingPacketLossFraction, frame_length_change_criteria)));

  return controller;
}

std::map<FrameLengthController::Config::FrameLengthChange, int>
CreateChangeCriteriaFor20msAnd60ms() {
  return std::map<FrameLengthController::Config::FrameLengthChange, int>{
      {FrameLengthController::Config::FrameLengthChange(20, 60),
       kFl20msTo60msBandwidthBps},
      {FrameLengthController::Config::FrameLengthChange(60, 20),
       kFl60msTo20msBandwidthBps}};
}

std::map<FrameLengthController::Config::FrameLengthChange, int>
CreateChangeCriteriaFor20ms60msAnd120ms() {
  return std::map<FrameLengthController::Config::FrameLengthChange, int>{
      {FrameLengthController::Config::FrameLengthChange(20, 60),
       kFl20msTo60msBandwidthBps},
      {FrameLengthController::Config::FrameLengthChange(60, 20),
       kFl60msTo20msBandwidthBps},
      {FrameLengthController::Config::FrameLengthChange(60, 120),
       kFl60msTo120msBandwidthBps},
      {FrameLengthController::Config::FrameLengthChange(120, 60),
       kFl120msTo60msBandwidthBps}};
}

void UpdateNetworkMetrics(
    FrameLengthController* controller,
    const rtc::Optional<int>& uplink_bandwidth_bps,
    const rtc::Optional<float>& uplink_packet_loss_fraction,
    const rtc::Optional<size_t>& overhead_bytes_per_packet) {
  // UpdateNetworkMetrics can accept multiple network metric updates at once.
  // However, currently, the most used case is to update one metric at a time.
  // To reflect this fact, we separate the calls.
  if (uplink_bandwidth_bps) {
    Controller::NetworkMetrics network_metrics;
    network_metrics.uplink_bandwidth_bps = uplink_bandwidth_bps;
    controller->UpdateNetworkMetrics(network_metrics);
  }
  if (uplink_packet_loss_fraction) {
    Controller::NetworkMetrics network_metrics;
    network_metrics.uplink_packet_loss_fraction = uplink_packet_loss_fraction;
    controller->UpdateNetworkMetrics(network_metrics);
  }
  if (overhead_bytes_per_packet) {
    Controller::NetworkMetrics network_metrics;
    network_metrics.overhead_bytes_per_packet = overhead_bytes_per_packet;
    controller->UpdateNetworkMetrics(network_metrics);
  }
}

void CheckDecision(FrameLengthController* controller,
                   const rtc::Optional<bool>& enable_fec,
                   int expected_frame_length_ms) {
  AudioNetworkAdaptor::EncoderRuntimeConfig config;
  config.enable_fec = enable_fec;
  controller->MakeDecision(&config);
  EXPECT_EQ(rtc::Optional<int>(expected_frame_length_ms),
            config.frame_length_ms);
}

}  // namespace

TEST(FrameLengthControllerTest, DecreaseTo20MsOnHighUplinkBandwidth) {
  auto controller =
      CreateController(CreateChangeCriteriaFor20msAnd60ms(), {20, 60}, 60);
  UpdateNetworkMetrics(
      controller.get(), rtc::Optional<int>(kFl60msTo20msBandwidthBps),
      rtc::Optional<float>(), rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 20);
}

TEST(FrameLengthControllerTest, DecreaseTo20MsOnHighUplinkPacketLossFraction) {
  auto controller =
      CreateController(CreateChangeCriteriaFor20msAnd60ms(), {20, 60}, 60);
  UpdateNetworkMetrics(controller.get(), rtc::Optional<int>(),
                       rtc::Optional<float>(kFlDecreasingPacketLossFraction),
                       rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 20);
}

TEST(FrameLengthControllerTest, DecreaseTo20MsWhenFecIsOn) {
  auto controller =
      CreateController(CreateChangeCriteriaFor20msAnd60ms(), {20, 60}, 60);
  CheckDecision(controller.get(), rtc::Optional<bool>(true), 20);
}

TEST(FrameLengthControllerTest,
     Maintain60MsIf20MsNotInReceiverFrameLengthRange) {
  auto controller =
      CreateController(CreateChangeCriteriaFor20msAnd60ms(), {60}, 60);
  // Set FEC on that would cause frame length to decrease if receiver frame
  // length range included 20ms.
  CheckDecision(controller.get(), rtc::Optional<bool>(true), 60);
}

TEST(FrameLengthControllerTest, Maintain60MsOnMultipleConditions) {
  // Maintain 60ms frame length if
  // 1. |uplink_bandwidth_bps| is at medium level,
  // 2. |uplink_packet_loss_fraction| is at medium,
  // 3. FEC is not decided ON.
  auto controller =
      CreateController(CreateChangeCriteriaFor20msAnd60ms(), {20, 60}, 60);
  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kMediumBandwidthBps),
                       rtc::Optional<float>(kMediumPacketLossFraction),
                       rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 60);
}

TEST(FrameLengthControllerTest, IncreaseTo60MsOnMultipleConditions) {
  // Increase to 60ms frame length if
  // 1. |uplink_bandwidth_bps| is known to be smaller than a threshold AND
  // 2. |uplink_packet_loss_fraction| is known to be smaller than a threshold
  //    AND
  // 3. FEC is not decided or OFF.
  auto controller =
      CreateController(CreateChangeCriteriaFor20msAnd60ms(), {20, 60}, 20);
  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kFl20msTo60msBandwidthBps),
                       rtc::Optional<float>(kFlIncreasingPacketLossFraction),
                       rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 60);
}

TEST(FrameLengthControllerTest, IncreaseTo60MsOnVeryLowUplinkBandwidth) {
  auto controller =
      CreateController(CreateChangeCriteriaFor20msAnd60ms(), {20, 60}, 20);
  // We set packet loss fraction to kFlDecreasingPacketLossFraction, and FEC on,
  // both of which should have prevented frame length to increase, if the uplink
  // bandwidth was not this low.
  UpdateNetworkMetrics(controller.get(), rtc::Optional<int>(VeryLowBitrate(20)),
                       rtc::Optional<float>(kFlIncreasingPacketLossFraction),
                       rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(controller.get(), rtc::Optional<bool>(true), 60);
}

TEST(FrameLengthControllerTest, Maintain60MsOnVeryLowUplinkBandwidth) {
  auto controller =
      CreateController(CreateChangeCriteriaFor20msAnd60ms(), {20, 60}, 60);
  // We set packet loss fraction to FlDecreasingPacketLossFraction, and FEC on,
  // both of which should have caused the frame length to decrease, if the
  // uplink bandwidth was not this low.
  UpdateNetworkMetrics(controller.get(), rtc::Optional<int>(VeryLowBitrate(20)),
                       rtc::Optional<float>(kFlIncreasingPacketLossFraction),
                       rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(controller.get(), rtc::Optional<bool>(true), 60);
}

TEST(FrameLengthControllerTest, UpdateMultipleNetworkMetricsAtOnce) {
  // This test is similar to IncreaseTo60MsOnMultipleConditions. But instead of
  // using ::UpdateNetworkMetrics(...), which calls
  // FrameLengthController::UpdateNetworkMetrics(...) multiple times, we
  // we call it only once. This is to verify that
  // FrameLengthController::UpdateNetworkMetrics(...) can handle multiple
  // network updates at once. This is, however, not a common use case in current
  // audio_network_adaptor_impl.cc.
  auto controller =
      CreateController(CreateChangeCriteriaFor20msAnd60ms(), {20, 60}, 20);
  Controller::NetworkMetrics network_metrics;
  network_metrics.uplink_bandwidth_bps =
      rtc::Optional<int>(kFl20msTo60msBandwidthBps);
  network_metrics.uplink_packet_loss_fraction =
      rtc::Optional<float>(kFlIncreasingPacketLossFraction);
  controller->UpdateNetworkMetrics(network_metrics);
  CheckDecision(controller.get(), rtc::Optional<bool>(), 60);
}

TEST(FrameLengthControllerTest,
     Maintain20MsIf60MsNotInReceiverFrameLengthRange) {
  auto controller =
      CreateController(CreateChangeCriteriaFor20msAnd60ms(), {20}, 20);
  // Use a low uplink bandwidth and a low uplink packet loss fraction that would
  // cause frame length to increase if receiver frame length included 60ms.
  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kFl20msTo60msBandwidthBps),
                       rtc::Optional<float>(kFlIncreasingPacketLossFraction),
                       rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 20);
}

TEST(FrameLengthControllerTest, Maintain20MsOnMediumUplinkBandwidth) {
  auto controller =
      CreateController(CreateChangeCriteriaFor20msAnd60ms(), {20, 60}, 20);
  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kMediumBandwidthBps),
                       rtc::Optional<float>(kFlIncreasingPacketLossFraction),
                       rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 20);
}

TEST(FrameLengthControllerTest, Maintain20MsOnMediumUplinkPacketLossFraction) {
  auto controller =
      CreateController(CreateChangeCriteriaFor20msAnd60ms(), {20, 60}, 20);
  // Use a low uplink bandwidth that would cause frame length to increase if
  // uplink packet loss fraction was low.
  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kFl20msTo60msBandwidthBps),
                       rtc::Optional<float>(kMediumPacketLossFraction),
                       rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 20);
}

TEST(FrameLengthControllerTest, Maintain20MsWhenFecIsOn) {
  auto controller =
      CreateController(CreateChangeCriteriaFor20msAnd60ms(), {20, 60}, 20);
  // Use a low uplink bandwidth and a low uplink packet loss fraction that would
  // cause frame length to increase if FEC was not ON.
  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kFl20msTo60msBandwidthBps),
                       rtc::Optional<float>(kMediumPacketLossFraction),
                       rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(controller.get(), rtc::Optional<bool>(true), 20);
}

TEST(FrameLengthControllerTest, Maintain60MsWhenNo120msCriteriaIsSet) {
  auto controller =
      CreateController(CreateChangeCriteriaFor20msAnd60ms(), {20, 60, 120}, 60);
  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kFl60msTo120msBandwidthBps),
                       rtc::Optional<float>(kFlIncreasingPacketLossFraction),
                       rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 60);
}

TEST(FrameLengthControllerTest, From120MsTo20MsOnHighUplinkBandwidth) {
  auto controller = CreateController(CreateChangeCriteriaFor20ms60msAnd120ms(),
                                     {20, 60, 120}, 120);
  // It takes two steps for frame length to go from 120ms to 20ms.
  UpdateNetworkMetrics(
      controller.get(), rtc::Optional<int>(kFl60msTo20msBandwidthBps),
      rtc::Optional<float>(), rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 60);

  UpdateNetworkMetrics(
      controller.get(), rtc::Optional<int>(kFl60msTo20msBandwidthBps),
      rtc::Optional<float>(), rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 20);
}

TEST(FrameLengthControllerTest, From120MsTo20MsOnHighUplinkPacketLossFraction) {
  auto controller = CreateController(CreateChangeCriteriaFor20ms60msAnd120ms(),
                                     {20, 60, 120}, 120);
  // It takes two steps for frame length to go from 120ms to 20ms.
  UpdateNetworkMetrics(controller.get(), rtc::Optional<int>(),
                       rtc::Optional<float>(kFlDecreasingPacketLossFraction),
                       rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 60);

  UpdateNetworkMetrics(controller.get(), rtc::Optional<int>(),
                       rtc::Optional<float>(kFlDecreasingPacketLossFraction),
                       rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 20);
}

TEST(FrameLengthControllerTest, From120MsTo20MsWhenFecIsOn) {
  auto controller = CreateController(CreateChangeCriteriaFor20ms60msAnd120ms(),
                                     {20, 60, 120}, 120);
  // It takes two steps for frame length to go from 120ms to 20ms.
  CheckDecision(controller.get(), rtc::Optional<bool>(true), 60);
  CheckDecision(controller.get(), rtc::Optional<bool>(true), 20);
}

TEST(FrameLengthControllerTest, Maintain120MsOnVeryLowUplinkBandwidth) {
  auto controller = CreateController(CreateChangeCriteriaFor20ms60msAnd120ms(),
                                     {20, 60, 120}, 120);
  // We set packet loss fraction to FlDecreasingPacketLossFraction, and FEC on,
  // both of which should have caused the frame length to decrease, if the
  // uplink bandwidth was not this low.
  UpdateNetworkMetrics(controller.get(), rtc::Optional<int>(VeryLowBitrate(60)),
                       rtc::Optional<float>(kFlDecreasingPacketLossFraction),
                       rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(controller.get(), rtc::Optional<bool>(true), 120);
}

TEST(FrameLengthControllerTest, From60MsTo120MsOnVeryLowUplinkBandwidth) {
  auto controller = CreateController(CreateChangeCriteriaFor20ms60msAnd120ms(),
                                     {20, 60, 120}, 60);
  // We set packet loss fraction to FlDecreasingPacketLossFraction, and FEC on,
  // both of which should have prevented frame length to increase, if the uplink
  // bandwidth was not this low.
  UpdateNetworkMetrics(controller.get(), rtc::Optional<int>(VeryLowBitrate(60)),
                       rtc::Optional<float>(kFlDecreasingPacketLossFraction),
                       rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(controller.get(), rtc::Optional<bool>(true), 120);
}

TEST(FrameLengthControllerTest, From20MsTo120MsOnMultipleConditions) {
  // Increase to 120ms frame length if
  // 1. |uplink_bandwidth_bps| is known to be smaller than a threshold AND
  // 2. |uplink_packet_loss_fraction| is known to be smaller than a threshold
  //    AND
  // 3. FEC is not decided or OFF.
  auto controller = CreateController(CreateChangeCriteriaFor20ms60msAnd120ms(),
                                     {20, 60, 120}, 20);
  // It takes two steps for frame length to go from 20ms to 120ms.
  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kFl60msTo120msBandwidthBps),
                       rtc::Optional<float>(kFlIncreasingPacketLossFraction),
                       rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 60);
  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kFl60msTo120msBandwidthBps),
                       rtc::Optional<float>(kFlIncreasingPacketLossFraction),
                       rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 120);
}

TEST(FrameLengthControllerTest, Stall60MsIf120MsNotInReceiverFrameLengthRange) {
  auto controller =
      CreateController(CreateChangeCriteriaFor20ms60msAnd120ms(), {20, 60}, 20);
  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kFl60msTo120msBandwidthBps),
                       rtc::Optional<float>(kFlIncreasingPacketLossFraction),
                       rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 60);
  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kFl60msTo120msBandwidthBps),
                       rtc::Optional<float>(kFlIncreasingPacketLossFraction),
                       rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 60);
}

TEST(FrameLengthControllerTest, CheckBehaviorOnChangingNetworkMetrics) {
  auto controller = CreateController(CreateChangeCriteriaFor20ms60msAnd120ms(),
                                     {20, 60, 120}, 20);
  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kMediumBandwidthBps),
                       rtc::Optional<float>(kFlIncreasingPacketLossFraction),
                       rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 20);

  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kFl20msTo60msBandwidthBps),
                       rtc::Optional<float>(kFlIncreasingPacketLossFraction),
                       rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 60);

  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kFl60msTo120msBandwidthBps),
                       rtc::Optional<float>(kMediumPacketLossFraction),
                       rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 60);

  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kFl60msTo120msBandwidthBps),
                       rtc::Optional<float>(kFlIncreasingPacketLossFraction),
                       rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 120);

  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kFl120msTo60msBandwidthBps),
                       rtc::Optional<float>(kFlIncreasingPacketLossFraction),
                       rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 60);

  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kMediumBandwidthBps),
                       rtc::Optional<float>(kFlDecreasingPacketLossFraction),
                       rtc::Optional<size_t>(kOverheadBytesPerPacket));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 20);
}

}  // namespace webrtc
