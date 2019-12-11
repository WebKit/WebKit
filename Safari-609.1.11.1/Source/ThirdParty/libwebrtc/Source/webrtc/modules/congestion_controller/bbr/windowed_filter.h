/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_CONGESTION_CONTROLLER_BBR_WINDOWED_FILTER_H_
#define MODULES_CONGESTION_CONTROLLER_BBR_WINDOWED_FILTER_H_

// From the Quic BBR implementation in Chromium

// Implements Kathleen Nichols' algorithm for tracking the minimum (or maximum)
// estimate of a stream of samples over some fixed time interval. (E.g.,
// the minimum RTT over the past five minutes.) The algorithm keeps track of
// the best, second best, and third best min (or max) estimates, maintaining an
// invariant that the measurement time of the n'th best >= n-1'th best.

// The algorithm works as follows. On a reset, all three estimates are set to
// the same sample. The second best estimate is then recorded in the second
// quarter of the window, and a third best estimate is recorded in the second
// half of the window, bounding the worst case error when the true min is
// monotonically increasing (or true max is monotonically decreasing) over the
// window.
//
// A new best sample replaces all three estimates, since the new best is lower
// (or higher) than everything else in the window and it is the most recent.
// The window thus effectively gets reset on every new min. The same property
// holds true for second best and third best estimates. Specifically, when a
// sample arrives that is better than the second best but not better than the
// best, it replaces the second and third best estimates but not the best
// estimate. Similarly, a sample that is better than the third best estimate
// but not the other estimates replaces only the third best estimate.
//
// Finally, when the best expires, it is replaced by the second best, which in
// turn is replaced by the third best. The newest sample replaces the third
// best.

namespace webrtc {
namespace bbr {

// Compares two values and returns true if the first is less than or equal
// to the second.
template <class T>
struct MinFilter {
  bool operator()(const T& lhs, const T& rhs) const { return lhs <= rhs; }
};

// Compares two values and returns true if the first is greater than or equal
// to the second.
template <class T>
struct MaxFilter {
  bool operator()(const T& lhs, const T& rhs) const { return lhs >= rhs; }
};

// Use the following to construct a windowed filter object of type T.
// For example, a min filter using Timestamp as the time type:
//   WindowedFilter<T, MinFilter<T>, Timestamp, TimeDelta>
//   ObjectName;
// A max filter using 64-bit integers as the time type:
//   WindowedFilter<T, MaxFilter<T>, uint64_t, int64_t> ObjectName;
// Specifically, this template takes four arguments:
// 1. T -- type of the measurement that is being filtered.
// 2. Compare -- MinFilter<T> or MaxFilter<T>, depending on the type of filter
//    desired.
// 3. TimeT -- the type used to represent timestamps.
// 4. TimeDeltaT -- the type used to represent continuous time intervals between
//    two timestamps.  Has to be the type of (a - b) if both |a| and |b| are
//    of type TimeT.
template <class T, class Compare, typename TimeT, typename TimeDeltaT>
class WindowedFilter {
 public:
  // |window_length| is the period after which a best estimate expires.
  // |zero_value| is used as the uninitialized value for objects of T.
  // Importantly, |zero_value| should be an invalid value for a true sample.
  WindowedFilter(TimeDeltaT window_length, T zero_value, TimeT zero_time)
      : window_length_(window_length),
        zero_value_(zero_value),
        estimates_{Sample(zero_value_, zero_time),
                   Sample(zero_value_, zero_time),
                   Sample(zero_value_, zero_time)} {}

  // Changes the window length.  Does not update any current samples.
  void SetWindowLength(TimeDeltaT window_length) {
    window_length_ = window_length;
  }

  // Updates best estimates with |sample|, and expires and updates best
  // estimates as necessary.
  void Update(T new_sample, TimeT new_time) {
    // Reset all estimates if they have not yet been initialized, if new sample
    // is a new best, or if the newest recorded estimate is too old.
    if (estimates_[0].sample == zero_value_ ||
        Compare()(new_sample, estimates_[0].sample) ||
        new_time - estimates_[2].time > window_length_) {
      Reset(new_sample, new_time);
      return;
    }

    if (Compare()(new_sample, estimates_[1].sample)) {
      estimates_[1] = Sample(new_sample, new_time);
      estimates_[2] = estimates_[1];
    } else if (Compare()(new_sample, estimates_[2].sample)) {
      estimates_[2] = Sample(new_sample, new_time);
    }

    // Expire and update estimates as necessary.
    if (new_time - estimates_[0].time > window_length_) {
      // The best estimate hasn't been updated for an entire window, so promote
      // second and third best estimates.
      estimates_[0] = estimates_[1];
      estimates_[1] = estimates_[2];
      estimates_[2] = Sample(new_sample, new_time);
      // Need to iterate one more time. Check if the new best estimate is
      // outside the window as well, since it may also have been recorded a
      // long time ago. Don't need to iterate once more since we cover that
      // case at the beginning of the method.
      if (new_time - estimates_[0].time > window_length_) {
        estimates_[0] = estimates_[1];
        estimates_[1] = estimates_[2];
      }
      return;
    }
    if (estimates_[1].sample == estimates_[0].sample &&
        new_time - estimates_[1].time > window_length_ >> 2) {
      // A quarter of the window has passed without a better sample, so the
      // second-best estimate is taken from the second quarter of the window.
      estimates_[2] = estimates_[1] = Sample(new_sample, new_time);
      return;
    }

    if (estimates_[2].sample == estimates_[1].sample &&
        new_time - estimates_[2].time > window_length_ >> 1) {
      // We've passed a half of the window without a better estimate, so take
      // a third-best estimate from the second half of the window.
      estimates_[2] = Sample(new_sample, new_time);
    }
  }

  // Resets all estimates to new sample.
  void Reset(T new_sample, TimeT new_time) {
    estimates_[0] = estimates_[1] = estimates_[2] =
        Sample(new_sample, new_time);
  }

  T GetBest() const { return estimates_[0].sample; }
  T GetSecondBest() const { return estimates_[1].sample; }
  T GetThirdBest() const { return estimates_[2].sample; }

 private:
  struct Sample {
    T sample;
    TimeT time;
    Sample(T init_sample, TimeT init_time)
        : sample(init_sample), time(init_time) {}
  };

  TimeDeltaT window_length_;  // Time length of window.
  T zero_value_;              // Uninitialized value of T.
  Sample estimates_[3];       // Best estimate is element 0.
};

}  // namespace bbr
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_BBR_WINDOWED_FILTER_H_
