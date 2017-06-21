/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/agc2/digital_gain_applier.h"

#include <algorithm>

namespace webrtc {
namespace {

constexpr float kMaxSampleValue = 32767.0f;
constexpr float kMinSampleValue = -32767.0f;

}  // namespace

DigitalGainApplier::DigitalGainApplier() = default;

void DigitalGainApplier::Process(float gain, rtc::ArrayView<float> samples) {
  if (gain == 1.f) { return; }
  for (auto& v : samples) { v *= gain; }
  LimitToAllowedRange(samples);
}

void DigitalGainApplier::LimitToAllowedRange(rtc::ArrayView<float> x) {
  for (auto& v : x) {
    v = std::max(kMinSampleValue, v);
    v = std::min(kMaxSampleValue, v);
  }
}

}  // namespace webrtc
