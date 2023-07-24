/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/frame_helpers.h"

#include "api/units/timestamp.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(FrameHasBadRenderTimingTest, LargePositiveFrameDelayIsBad) {
  Timestamp render_time = Timestamp::Seconds(12);
  Timestamp now = Timestamp::Seconds(0);

  EXPECT_TRUE(FrameHasBadRenderTiming(render_time, now));
}

TEST(FrameHasBadRenderTimingTest, LargeNegativeFrameDelayIsBad) {
  Timestamp render_time = Timestamp::Seconds(12);
  Timestamp now = Timestamp::Seconds(24);

  EXPECT_TRUE(FrameHasBadRenderTiming(render_time, now));
}

}  // namespace
}  // namespace webrtc
