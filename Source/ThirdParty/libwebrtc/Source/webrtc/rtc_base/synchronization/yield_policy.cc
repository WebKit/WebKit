/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/synchronization/yield_policy.h"

namespace webrtc {
namespace {

constinit thread_local YieldInterface* current_yield_policy = nullptr;

}  // namespace

ScopedYieldPolicy::ScopedYieldPolicy(YieldInterface* policy)
    : previous_(current_yield_policy) {
  current_yield_policy = policy;
}

ScopedYieldPolicy::~ScopedYieldPolicy() {
  current_yield_policy = previous_;
}

void ScopedYieldPolicy::YieldExecution() {
  if (YieldInterface* current = current_yield_policy; current != nullptr) {
    current->YieldExecution();
  }
}

}  // namespace webrtc
