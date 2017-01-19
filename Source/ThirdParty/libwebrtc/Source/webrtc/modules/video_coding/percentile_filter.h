/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_PERCENTILE_FILTER_H_
#define WEBRTC_MODULES_VIDEO_CODING_PERCENTILE_FILTER_H_

#include <stdint.h>

#include <set>

namespace webrtc {

// Class to efficiently get the percentile value from a group of observations.
// The percentile is the value below which a given percentage of the
// observations fall.
class PercentileFilter {
 public:
  // Construct filter. |percentile| should be between 0 and 1.
  explicit PercentileFilter(float percentile);

  // Insert one observation. The complexity of this operation is logarithmic in
  // the size of the container.
  void Insert(const int64_t& value);
  // Remove one observation. The complexity of this operation is logarithmic in
  // the size of the container.
  void Erase(const int64_t& value);
  // Get the percentile value. The complexity of this operation is constant.
  int64_t GetPercentileValue() const;

 private:
  // Update iterator and index to point at target percentile value.
  void UpdatePercentileIterator();

  const float percentile_;
  std::multiset<int64_t> set_;
  // Maintain iterator and index of current target percentile value.
  std::multiset<int64_t>::iterator percentile_it_;
  int64_t percentile_index_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_PERCENTILE_FILTER_H_
