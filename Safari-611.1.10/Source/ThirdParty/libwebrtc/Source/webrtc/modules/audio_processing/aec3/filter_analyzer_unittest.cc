/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/filter_analyzer.h"

#include <algorithm>

#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

// Verifies that the filter analyzer handles filter resizes properly.
TEST(FilterAnalyzer, FilterResize) {
  EchoCanceller3Config c;
  std::vector<float> filter(65, 0.f);
  for (size_t num_capture_channels : {1, 2, 4}) {
    FilterAnalyzer fa(c, num_capture_channels);
    fa.SetRegionToAnalyze(filter.size());
    fa.SetRegionToAnalyze(filter.size());
    filter.resize(32);
    fa.SetRegionToAnalyze(filter.size());
  }
}

}  // namespace webrtc
