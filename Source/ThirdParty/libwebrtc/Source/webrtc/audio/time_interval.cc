/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/time_interval.h"
#include "rtc_base/checks.h"
#include "rtc_base/timeutils.h"

namespace webrtc {

TimeInterval::TimeInterval() = default;
TimeInterval::~TimeInterval() = default;

void TimeInterval::Extend() {
  Extend(rtc::TimeMillis());
}

void TimeInterval::Extend(int64_t time) {
  if (!interval_) {
    interval_.emplace(time, time);
  } else {
    if (time < interval_->first) {
      interval_->first = time;
    }
    if (time > interval_->last) {
      interval_->last = time;
    }
  }
}

void TimeInterval::Extend(const TimeInterval& other_interval) {
  if (!other_interval.Empty()) {
    Extend(other_interval.interval_->first);
    Extend(other_interval.interval_->last);
  }
}

bool TimeInterval::Empty() const {
  return !interval_;
}

int64_t TimeInterval::Length() const {
  RTC_DCHECK(interval_);
  return interval_->last - interval_->first;
}

TimeInterval::Interval::Interval(int64_t first, int64_t last)
    : first(first), last(last) {}

}  // namespace webrtc
