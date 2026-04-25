/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_GUNIT_H_
#define RTC_BASE_GUNIT_H_

// TODO(bugs.webrtc.org/42226242): remove transitive includes
#include "rtc_base/thread.h"  // IWYU pragma: keep
#include "test/gtest.h"       // IWYU pragma: keep

// Wait until "ex" is true, or "timeout" expires, using fake clock where
// messages are processed every millisecond.
// TODO(pthatcher): Allow tests to control how many milliseconds to advance.
#define SIMULATED_WAIT(ex, timeout, clock)                          \
  for (int64_t wait_start = ::webrtc::TimeMillis();                 \
       !(ex) && ::webrtc::TimeMillis() < wait_start + (timeout);) { \
    (clock).AdvanceTime(webrtc::TimeDelta::Millis(1));              \
  }

#endif  // RTC_BASE_GUNIT_H_
