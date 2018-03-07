/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/output_selector.h"

#include <algorithm>
#include <array>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "test/gtest.h"

namespace webrtc {

// Verifies that the switching between the signals in the output works as
// intended.
TEST(OutputSelector, ProperSwitching) {
  OutputSelector selector;

  std::array<float, kBlockSize> y;
  std::array<float, kBlockSize> e;
  std::array<float, kBlockSize> e_ref;
  std::array<float, kBlockSize> y_ref;
  auto init_blocks = [](std::array<float, kBlockSize>* e,
                        std::array<float, kBlockSize>* y) {
    e->fill(10.f);
    y->fill(20.f);
  };

  init_blocks(&e_ref, &y_ref);

  init_blocks(&e, &y);
  selector.FormLinearOutput(false, e, y);
  EXPECT_EQ(y_ref, y);

  init_blocks(&e, &y);
  selector.FormLinearOutput(true, e, y);
  EXPECT_NE(e_ref, y);
  EXPECT_NE(y_ref, y);

  init_blocks(&e, &y);
  selector.FormLinearOutput(true, e, y);
  EXPECT_EQ(e_ref, y);

  init_blocks(&e, &y);
  selector.FormLinearOutput(true, e, y);
  EXPECT_EQ(e_ref, y);

  init_blocks(&e, &y);
  selector.FormLinearOutput(false, e, y);
  EXPECT_NE(e_ref, y);
  EXPECT_NE(y_ref, y);

  init_blocks(&e, &y);
  selector.FormLinearOutput(false, e, y);
  EXPECT_EQ(y_ref, y);

  init_blocks(&e, &y);
  selector.FormLinearOutput(false, e, y);
  EXPECT_EQ(y_ref, y);
}

}  // namespace webrtc
