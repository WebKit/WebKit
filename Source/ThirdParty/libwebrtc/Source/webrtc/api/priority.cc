/*
 *  Copyright 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/priority.h"

#include "rtc_base/checks.h"

namespace webrtc {

PriorityValue::PriorityValue(Priority priority) {
  switch (priority) {
    case Priority::kVeryLow:
      value_ = 128;
      break;
    case Priority::kLow:
      value_ = 256;
      break;
    case Priority::kMedium:
      value_ = 512;
      break;
    case Priority::kHigh:
      value_ = 1024;
      break;
    default:
      RTC_CHECK_NOTREACHED();
  }
}

}  // namespace webrtc
