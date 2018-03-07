/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_TIME_INTERVAL_H_
#define AUDIO_TIME_INTERVAL_H_

#include <stdint.h>

#include "api/optional.h"

namespace webrtc {

// This class logs the first and last time its Extend() function is called.
//
// This class is not thread-safe; Extend() calls should only be made by a
// single thread at a time, such as within a lock or destructor.
//
// Example usage:
//   // let x < y < z < u < v
//   rtc::TimeInterval interval;
//   ...  //   interval.Extend(); // at time x
//   ...
//   interval.Extend(); // at time y
//   ...
//   interval.Extend(); // at time u
//   ...
//   interval.Extend(z); // at time v
//   ...
//   if (!interval.Empty()) {
//     int64_t active_time = interval.Length(); // returns (u - x)
//   }
class TimeInterval {
 public:
  TimeInterval();
  ~TimeInterval();
  // Extend the interval with the current time.
  void Extend();
  // Extend the interval with a given time.
  void Extend(int64_t time);
  // Take the convex hull with another interval.
  void Extend(const TimeInterval& other_interval);
  // True iff Extend has never been called.
  bool Empty() const;
  // Returns the time between the first and the last tick, in milliseconds.
  int64_t Length() const;

 private:
  struct Interval {
    Interval(int64_t first, int64_t last);

    int64_t first, last;
  };
  rtc::Optional<Interval> interval_;
};

}  // namespace webrtc

#endif  // AUDIO_TIME_INTERVAL_H_
