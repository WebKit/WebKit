/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_ROBO_CALLER_H_
#define RTC_BASE_ROBO_CALLER_H_

#include <utility>
#include <vector>

#include "api/function_view.h"
#include "rtc_base/system/assume.h"
#include "rtc_base/system/inline.h"
#include "rtc_base/untyped_function.h"

namespace webrtc {
namespace robo_caller_impl {

class RoboCallerReceivers {
 public:
  RoboCallerReceivers();
  RoboCallerReceivers(const RoboCallerReceivers&) = delete;
  RoboCallerReceivers& operator=(const RoboCallerReceivers&) = delete;
  RoboCallerReceivers(RoboCallerReceivers&&) = delete;
  RoboCallerReceivers& operator=(RoboCallerReceivers&&) = delete;
  ~RoboCallerReceivers();

  template <typename UntypedFunctionArgsT>
  RTC_NO_INLINE void AddReceiver(UntypedFunctionArgsT args) {
    receivers_.push_back(UntypedFunction::Create(args));
  }

  void Foreach(rtc::FunctionView<void(UntypedFunction&)> fv);

 private:
  std::vector<UntypedFunction> receivers_;
};

extern template void RoboCallerReceivers::AddReceiver(
    UntypedFunction::TrivialUntypedFunctionArgs<1>);
extern template void RoboCallerReceivers::AddReceiver(
    UntypedFunction::TrivialUntypedFunctionArgs<2>);
extern template void RoboCallerReceivers::AddReceiver(
    UntypedFunction::TrivialUntypedFunctionArgs<3>);
extern template void RoboCallerReceivers::AddReceiver(
    UntypedFunction::TrivialUntypedFunctionArgs<4>);
extern template void RoboCallerReceivers::AddReceiver(
    UntypedFunction::NontrivialUntypedFunctionArgs);
extern template void RoboCallerReceivers::AddReceiver(
    UntypedFunction::FunctionPointerUntypedFunctionArgs);

}  // namespace robo_caller_impl

// A collection of receivers (callable objects) that can be called all at once.
// Optimized for minimal binary size.
//
// Neither copyable nor movable. Could easily be made movable if necessary.
//
// TODO(kwiberg): Add support for removing receivers, if necessary. AddReceiver
// would have to return some sort of ID that the caller could save and then pass
// to RemoveReceiver. Alternatively, the callable objects could return one value
// if they wish to stay in the CSC and another value if they wish to be removed.
// It depends on what's convenient for the callers...
template <typename... ArgT>
class RoboCaller {
 public:
  RoboCaller() = default;
  RoboCaller(const RoboCaller&) = delete;
  RoboCaller& operator=(const RoboCaller&) = delete;
  RoboCaller(RoboCaller&&) = delete;
  RoboCaller& operator=(RoboCaller&&) = delete;

  // Adds a new receiver. The receiver (a callable object or a function pointer)
  // must be movable, but need not be copyable. Its call signature should be
  // `void(ArgT...)`.
  template <typename F>
  void AddReceiver(F&& f) {
    receivers_.AddReceiver(
        UntypedFunction::PrepareArgs<void(ArgT...)>(std::forward<F>(f)));
  }

  // Calls all receivers with the given arguments.
  template <typename... ArgU>
  void Send(ArgU&&... args) {
    receivers_.Foreach([&](UntypedFunction& f) {
      f.Call<void(ArgT...)>(std::forward<ArgU>(args)...);
    });
  }

 private:
  robo_caller_impl::RoboCallerReceivers receivers_;
};

}  // namespace webrtc

#endif  // RTC_BASE_ROBO_CALLER_H_
