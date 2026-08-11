/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/fake_clock.h"

#include <cstdint>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

int64_t FakeClock::TimeNanos() const {
  MutexLock lock(&lock_);
  return time_ns_;
}

void FakeClock::SetTime(Timestamp new_time) {
  MutexLock lock(&lock_);
  RTC_DCHECK_GE(new_time.us() * 1000, time_ns_);
  time_ns_ = new_time.us() * 1000;
}

void FakeClock::AdvanceTime(TimeDelta delta) {
  RTC_DCHECK_GE(delta, TimeDelta::Zero());
  MutexLock lock(&lock_);
  time_ns_ += delta.ns();
}

void ThreadProcessingFakeClock::SetTime(Timestamp time) {
  clock_.SetTime(time);
  // If message queues are waiting in a socket select() with a timeout provided
  // by the OS, they should wake up and dispatch all messages that are ready.
  ThreadManager::ProcessAllMessageQueuesForTesting();
}

void ThreadProcessingFakeClock::AdvanceTime(TimeDelta delta) {
  clock_.AdvanceTime(delta);
  ThreadManager::ProcessAllMessageQueuesForTesting();
}

ScopedBaseFakeClock::ScopedBaseFakeClock() {
  prev_clock_ = SetClockForTesting(this);
}

ScopedBaseFakeClock::~ScopedBaseFakeClock() {
  SetClockForTesting(prev_clock_);
}

ScopedFakeClock::ScopedFakeClock() {
  prev_clock_ = SetClockForTesting(this);
}

ScopedFakeClock::~ScopedFakeClock() {
  SetClockForTesting(prev_clock_);
}

}  // namespace webrtc
