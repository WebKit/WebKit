/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/robo_caller.h"

namespace webrtc {
namespace robo_caller_impl {

RoboCallerReceivers::RoboCallerReceivers() = default;
RoboCallerReceivers::~RoboCallerReceivers() = default;

void RoboCallerReceivers::Foreach(
    rtc::FunctionView<void(UntypedFunction&)> fv) {
  for (auto& r : receivers_) {
    fv(r);
  }
}

template void RoboCallerReceivers::AddReceiver(
    UntypedFunction::TrivialUntypedFunctionArgs<1>);
template void RoboCallerReceivers::AddReceiver(
    UntypedFunction::TrivialUntypedFunctionArgs<2>);
template void RoboCallerReceivers::AddReceiver(
    UntypedFunction::TrivialUntypedFunctionArgs<3>);
template void RoboCallerReceivers::AddReceiver(
    UntypedFunction::TrivialUntypedFunctionArgs<4>);
template void RoboCallerReceivers::AddReceiver(
    UntypedFunction::NontrivialUntypedFunctionArgs);
template void RoboCallerReceivers::AddReceiver(
    UntypedFunction::FunctionPointerUntypedFunctionArgs);

}  // namespace robo_caller_impl
}  // namespace webrtc
