/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/adaptation/test/fake_resource_consumer_configuration.h"

#include "rtc_base/strings/string_builder.h"

namespace webrtc {

FakeResourceConsumerConfiguration::FakeResourceConsumerConfiguration(
    int width,
    int height,
    double frame_rate_hz,
    double preference)
    : width_(width),
      height_(height),
      frame_rate_hz_(frame_rate_hz),
      preference_(preference) {}

std::string FakeResourceConsumerConfiguration::Name() const {
  rtc::StringBuilder sb;
  sb << width_ << "x" << height_ << "@" << rtc::ToString(frame_rate_hz_);
  sb << "/" << rtc::ToString(preference_);
  return sb.str();
}

double FakeResourceConsumerConfiguration::Cost() const {
  return width_ * height_ * frame_rate_hz_;
}

double FakeResourceConsumerConfiguration::Preference() const {
  return preference_;
}

}  // namespace webrtc
