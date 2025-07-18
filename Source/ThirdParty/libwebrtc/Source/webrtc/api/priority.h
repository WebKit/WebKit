/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_PRIORITY_H_
#define API_PRIORITY_H_

#include <stdint.h>

#include "rtc_base/strong_alias.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// GENERATED_JAVA_ENUM_PACKAGE: org.webrtc
enum class Priority {
  kVeryLow,
  kLow,
  kMedium,
  kHigh,
};

class RTC_EXPORT PriorityValue
    : public webrtc::StrongAlias<class PriorityValueTag, uint16_t> {
 public:
  explicit PriorityValue(Priority priority);
  explicit PriorityValue(uint16_t priority) : StrongAlias(priority) {}
};

}  // namespace webrtc

#endif  // API_PRIORITY_H_
