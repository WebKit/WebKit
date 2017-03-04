/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/output_selector.h"

#include <algorithm>
#include <array>

#include "webrtc/modules/audio_processing/aec3/aec3_common.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

// Verifies that the switching between the signals in the output works as
// intended.
TEST(OutputSelector, ProperSwitching) {
  OutputSelector selector;

  constexpr int kNumBlocksToSwitchToSubtractor = 3;
  constexpr int kNumBlocksToSwitchFromSubtractor = 10;

  std::array<float, kBlockSize> weaker;
  std::array<float, kBlockSize> stronger;
  std::array<float, kBlockSize> y;
  std::array<float, kBlockSize> e;
  weaker.fill(10.f);
  stronger.fill(20.f);

  bool y_is_weakest = false;

  const auto form_e_and_y = [&](bool y_equals_weaker) {
    if (y_equals_weaker) {
      std::copy(weaker.begin(), weaker.end(), y.begin());
      std::copy(stronger.begin(), stronger.end(), e.begin());
    } else {
      std::copy(stronger.begin(), stronger.end(), y.begin());
      std::copy(weaker.begin(), weaker.end(), e.begin());
    }
  };

  for (int k = 0; k < 30; ++k) {
    // Verify that it takes a while for the signals transition to take effect.
    const int num_blocks_to_switch = y_is_weakest
                                         ? kNumBlocksToSwitchFromSubtractor
                                         : kNumBlocksToSwitchToSubtractor;
    for (int j = 0; j < num_blocks_to_switch; ++j) {
      form_e_and_y(y_is_weakest);
      selector.FormLinearOutput(e, y);
      EXPECT_EQ(stronger, y);
      EXPECT_EQ(y_is_weakest, selector.UseSubtractorOutput());
    }

    // Verify that the transition block is a mix between the signals.
    form_e_and_y(y_is_weakest);
    selector.FormLinearOutput(e, y);
    EXPECT_NE(weaker, y);
    EXPECT_NE(stronger, y);
    EXPECT_EQ(!y_is_weakest, selector.UseSubtractorOutput());

    y_is_weakest = !y_is_weakest;
  }
}

}  // namespace webrtc
