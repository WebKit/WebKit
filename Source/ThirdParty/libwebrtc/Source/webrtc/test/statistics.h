/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_STATISTICS_H_
#define TEST_STATISTICS_H_

#include <stdint.h>

namespace webrtc {
namespace test {

class Statistics {
 public:
  Statistics();

  void AddSample(double sample);

  double Max() const;
  double Mean() const;
  double Min() const;
  double Variance() const;
  double StandardDeviation() const;

 private:
  double sum_;
  double sum_squared_;
  double max_;
  double min_;
  uint64_t count_;
};
}  // namespace test
}  // namespace webrtc

#endif  // TEST_STATISTICS_H_
