/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/statistics.h"

#include <math.h>

#include <algorithm>

namespace webrtc {
namespace test {

Statistics::Statistics()
    : sum_(0.0),
      sum_squared_(0.0),
      max_(std::numeric_limits<double>::min()),
      min_(std::numeric_limits<double>::max()),
      count_(0) {}

void Statistics::AddSample(double sample) {
  sum_ += sample;
  sum_squared_ += sample * sample;
  max_ = std::max(max_, sample);
  min_ = std::min(min_, sample);
  ++count_;
}

double Statistics::Max() const {
  return max_;
}

double Statistics::Mean() const {
  if (count_ == 0)
    return 0.0;
  return sum_ / count_;
}

double Statistics::Min() const {
  return min_;
}

double Statistics::Variance() const {
  if (count_ == 0)
    return 0.0;
  return sum_squared_ / count_ - Mean() * Mean();
}

double Statistics::StandardDeviation() const {
  return sqrt(Variance());
}
}  // namespace test
}  // namespace webrtc
