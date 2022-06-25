/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/agc2_testing_common.h"

#include "rtc_base/gunit.h"

namespace webrtc {

TEST(GainController2TestingCommon, LinSpace) {
  std::vector<double> points1 = test::LinSpace(-1.0, 2.0, 4);
  const std::vector<double> expected_points1{{-1.0, 0.0, 1.0, 2.0}};
  EXPECT_EQ(expected_points1, points1);

  std::vector<double> points2 = test::LinSpace(0.0, 1.0, 4);
  const std::vector<double> expected_points2{{0.0, 1.0 / 3.0, 2.0 / 3.0, 1.0}};
  EXPECT_EQ(points2, expected_points2);
}

}  // namespace webrtc
