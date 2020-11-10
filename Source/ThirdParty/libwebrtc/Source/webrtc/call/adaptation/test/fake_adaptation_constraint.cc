/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/adaptation/test/fake_adaptation_constraint.h"

#include <utility>

namespace webrtc {

FakeAdaptationConstraint::FakeAdaptationConstraint(std::string name)
    : name_(std::move(name)), is_adaptation_up_allowed_(true) {}

FakeAdaptationConstraint::~FakeAdaptationConstraint() = default;

void FakeAdaptationConstraint::set_is_adaptation_up_allowed(
    bool is_adaptation_up_allowed) {
  is_adaptation_up_allowed_ = is_adaptation_up_allowed;
}

std::string FakeAdaptationConstraint::Name() const {
  return name_;
}

bool FakeAdaptationConstraint::IsAdaptationUpAllowed(
    const VideoStreamInputState& input_state,
    const VideoSourceRestrictions& restrictions_before,
    const VideoSourceRestrictions& restrictions_after) const {
  return is_adaptation_up_allowed_;
}

}  // namespace webrtc
