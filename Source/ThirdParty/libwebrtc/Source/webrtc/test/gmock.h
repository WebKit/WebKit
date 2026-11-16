/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_GMOCK_H_
#define TEST_GMOCK_H_

#include "rtc_base/ignore_wundef.h"

RTC_PUSH_IGNORING_WUNDEF()
#ifdef WEBRTC_WEBKIT_BUILD
#include <gmock/gmock.h>
#else
#include "testing/gmock/include/gmock/gmock.h"
#endif // WEBRTC_WEBKIT_BUILD
RTC_POP_IGNORING_WUNDEF()

#endif  // TEST_GMOCK_H_
