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

#include "webrtc/modules/audio_coding/audio_network_adaptor/dtx_controller.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

namespace {

constexpr int kDtxEnablingBandwidthBps = 55000;
constexpr int kDtxDisablingBandwidthBps = 65000;
constexpr int kMediumBandwidthBps =
    (kDtxEnablingBandwidthBps + kDtxDisablingBandwidthBps) / 2;

std::unique_ptr<DtxController> CreateController(int initial_dtx_enabled) {
  std::unique_ptr<DtxController> controller(new DtxController(
      DtxController::Config(initial_dtx_enabled, kDtxEnablingBandwidthBps,
                            kDtxDisablingBandwidthBps)));
  return controller;
}

void CheckDecision(DtxController* controller,
                   const rtc::Optional<int>& uplink_bandwidth_bps,
                   bool expected_dtx_enabled) {
  AudioNetworkAdaptor::EncoderRuntimeConfig config;
  Controller::NetworkMetrics metrics;
  metrics.uplink_bandwidth_bps = uplink_bandwidth_bps;
  controller->MakeDecision(metrics, &config);
  EXPECT_EQ(rtc::Optional<bool>(expected_dtx_enabled), config.enable_dtx);
}

}  // namespace

TEST(DtxControllerTest, OutputInitValueWhenUplinkBandwidthUnknown) {
  constexpr bool kInitialDtxEnabled = true;
  auto controller = CreateController(kInitialDtxEnabled);
  CheckDecision(controller.get(), rtc::Optional<int>(), kInitialDtxEnabled);
}

TEST(DtxControllerTest, TurnOnDtxForLowUplinkBandwidth) {
  auto controller = CreateController(false);
  CheckDecision(controller.get(), rtc::Optional<int>(kDtxEnablingBandwidthBps),
                true);
}

TEST(DtxControllerTest, TurnOffDtxForHighUplinkBandwidth) {
  auto controller = CreateController(true);
  CheckDecision(controller.get(), rtc::Optional<int>(kDtxDisablingBandwidthBps),
                false);
}

TEST(DtxControllerTest, MaintainDtxOffForMediumUplinkBandwidth) {
  auto controller = CreateController(false);
  CheckDecision(controller.get(), rtc::Optional<int>(kMediumBandwidthBps),
                false);
}

TEST(DtxControllerTest, MaintainDtxOnForMediumUplinkBandwidth) {
  auto controller = CreateController(true);
  CheckDecision(controller.get(), rtc::Optional<int>(kMediumBandwidthBps),
                true);
}

TEST(DtxControllerTest, CheckBehaviorOnChangingUplinkBandwidth) {
  auto controller = CreateController(false);
  CheckDecision(controller.get(), rtc::Optional<int>(kMediumBandwidthBps),
                false);
  CheckDecision(controller.get(), rtc::Optional<int>(kDtxEnablingBandwidthBps),
                true);
  CheckDecision(controller.get(), rtc::Optional<int>(kMediumBandwidthBps),
                true);
  CheckDecision(controller.get(), rtc::Optional<int>(kDtxDisablingBandwidthBps),
                false);
}

}  // namespace webrtc
