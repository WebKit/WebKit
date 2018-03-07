/*
 *  Copyright 2007 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_MACUTILS_H_
#define RTC_BASE_MACUTILS_H_

#include <CoreFoundation/CoreFoundation.h>
#include <string>

namespace rtc {
bool ToUtf8(const CFStringRef str16, std::string* str8);
bool ToUtf16(const std::string& str8, CFStringRef* str16);
}  // namespace rtc

#endif  // RTC_BASE_MACUTILS_H_
