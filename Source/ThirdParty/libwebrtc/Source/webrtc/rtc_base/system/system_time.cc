/*
 *  Copyright 2026 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_SYSTEM_SYSTEM_TIME_H_
#define RTC_BASE_SYSTEM_SYSTEM_TIME_H_

#include "rtc_base/system/system_time.h"

#include "api/units/timestamp.h"
#include "rtc_base/system_time.h"

namespace webrtc {

Timestamp SystemTime() {
  return Timestamp::Micros(SystemTimeNanos() / 1'000);
}

}  // namespace webrtc

#endif  // RTC_BASE_SYSTEM_SYSTEM_TIME_H_
