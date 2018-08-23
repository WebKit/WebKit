/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_BITRATE_CONSTRAINTS_H_
#define API_BITRATE_CONSTRAINTS_H_

#include <algorithm>

namespace webrtc {
// TODO(srte): BitrateConstraints and BitrateSettings should be merged.
// Both represent the same kind data, but are using different default
// initializer and representation of unset values.
struct BitrateConstraints {
  int min_bitrate_bps = 0;
  int start_bitrate_bps = kDefaultStartBitrateBps;
  int max_bitrate_bps = -1;

 private:
  static constexpr int kDefaultStartBitrateBps = 300000;
};

// Like std::min, but considers non-positive values to be unset.
template <typename T>
static T MinPositive(T a, T b) {
  if (a <= 0) {
    return b;
  }
  if (b <= 0) {
    return a;
  }
  return std::min(a, b);
}
}  // namespace webrtc
#endif  // API_BITRATE_CONSTRAINTS_H_
