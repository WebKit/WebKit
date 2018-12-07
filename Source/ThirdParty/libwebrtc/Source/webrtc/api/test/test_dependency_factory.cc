/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "api/test/test_dependency_factory.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {

// This checks everything in this file gets called on the same thread. It's
// static because it needs to look at the static methods too.
rtc::ThreadChecker* GetThreadChecker() {
  static rtc::ThreadChecker checker;
  return &checker;
}

std::unique_ptr<TestDependencyFactory> TestDependencyFactory::instance_ =
    nullptr;

const TestDependencyFactory& TestDependencyFactory::GetInstance() {
  RTC_DCHECK(GetThreadChecker()->CalledOnValidThread());
  if (instance_ == nullptr) {
    instance_ = absl::make_unique<TestDependencyFactory>();
  }
  return *instance_;
}

void TestDependencyFactory::SetInstance(
    std::unique_ptr<TestDependencyFactory> instance) {
  RTC_DCHECK(GetThreadChecker()->CalledOnValidThread());
  RTC_CHECK(instance_ == nullptr);
  instance_ = std::move(instance);
}

std::unique_ptr<VideoQualityTestFixtureInterface::InjectionComponents>
TestDependencyFactory::CreateComponents() const {
  RTC_DCHECK(GetThreadChecker()->CalledOnValidThread());
  return nullptr;
}

}  // namespace webrtc
