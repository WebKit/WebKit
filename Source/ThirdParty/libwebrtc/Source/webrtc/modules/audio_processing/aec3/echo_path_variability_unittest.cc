/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/echo_path_variability.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

TEST(EchoPathVariability, CorrectBehavior) {
  // Test correct passing and reporting of the gain change information.
  EchoPathVariability v(true, true);
  EXPECT_TRUE(v.gain_change);
  EXPECT_TRUE(v.delay_change);
  EXPECT_TRUE(v.AudioPathChanged());

  v = EchoPathVariability(true, false);
  EXPECT_TRUE(v.gain_change);
  EXPECT_FALSE(v.delay_change);
  EXPECT_TRUE(v.AudioPathChanged());

  v = EchoPathVariability(false, true);
  EXPECT_FALSE(v.gain_change);
  EXPECT_TRUE(v.delay_change);
  EXPECT_TRUE(v.AudioPathChanged());

  v = EchoPathVariability(false, false);
  EXPECT_FALSE(v.gain_change);
  EXPECT_FALSE(v.delay_change);
  EXPECT_FALSE(v.AudioPathChanged());
}

}  // namespace webrtc
