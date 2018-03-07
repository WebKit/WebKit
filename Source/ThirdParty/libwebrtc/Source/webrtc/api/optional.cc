/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/optional.h"

namespace rtc {
namespace optional_internal {

#if RTC_HAS_ASAN

void* FunctionThatDoesNothingImpl(void* x) {
  return x;
}

#endif

struct NulloptArg {
  constexpr NulloptArg() {}
};

static NulloptArg nullopt_arg;

}  // namespace optional_internal

const nullopt_t nullopt(rtc::optional_internal::nullopt_arg);

}  // namespace rtc
