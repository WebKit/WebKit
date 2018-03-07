/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/rgba_color.h"

#include <vector>

#include "test/gtest.h"

namespace webrtc {

TEST(RgbaColorTest, ConvertFromAndToUInt32) {
  static const std::vector<uint32_t> cases{
      0,         1000,       2693,       3725,       4097,      12532,
      19902,     27002,      27723,      30944,      65535,     65536,
      231194,    255985,     322871,     883798,     9585200,   12410056,
      12641940,  30496970,   105735668,  110117847,  482769275, 542368468,
      798173396, 2678656711, 3231043200, UINT32_MAX,
  };

  for (uint32_t value : cases) {
    RgbaColor left(value);
    ASSERT_EQ(left.ToUInt32(), value);
    RgbaColor right(left);
    ASSERT_EQ(left.ToUInt32(), right.ToUInt32());
  }
}

TEST(RgbaColorTest, AlphaChannelEquality) {
  RgbaColor left(10, 10, 10, 0);
  RgbaColor right(10, 10, 10, 255);
  ASSERT_EQ(left, right);
  right.alpha = 128;
  ASSERT_NE(left, right);
}

}  // namespace webrtc
