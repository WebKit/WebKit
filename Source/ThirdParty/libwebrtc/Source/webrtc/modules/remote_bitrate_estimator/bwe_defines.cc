/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

const char kBweTypeHistogram[] = "WebRTC.BWE.Types";

namespace congestion_controller {
int GetMinBitrateBps() {
  constexpr int kAudioMinBitrateBps = 5000;
  constexpr int kMinBitrateBps = 10000;
  if (webrtc::field_trial::IsEnabled("WebRTC-Audio-SendSideBwe") &&
      !webrtc::field_trial::IsEnabled("WebRTC-Audio-SendSideBwe-For-Video")) {
    return kAudioMinBitrateBps;
  }
  return kMinBitrateBps;
}

DataRate GetMinBitrate() {
  return DataRate::bps(GetMinBitrateBps());
}

}  // namespace congestion_controller

RateControlInput::RateControlInput(
    BandwidthUsage bw_state,
    const absl::optional<uint32_t>& estimated_throughput_bps)
    : bw_state(bw_state), estimated_throughput_bps(estimated_throughput_bps) {}

RateControlInput::~RateControlInput() = default;

}  // namespace webrtc
