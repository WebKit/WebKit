/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/moving_average.h"
#include "test/gtest.h"

namespace webrtc {

TEST(MovingAverage, Average) {
  constexpr size_t num_elem = 4;
  constexpr size_t mem_len = 3;
  constexpr float e = 1e-6f;
  aec3::MovingAverage ma(num_elem, mem_len);
  std::array<float, num_elem> data1 = {1, 2, 3, 4};
  std::array<float, num_elem> data2 = {5, 1, 9, 7};
  std::array<float, num_elem> data3 = {3, 3, 5, 6};
  std::array<float, num_elem> data4 = {8, 4, 2, 1};
  std::array<float, num_elem> output;

  ma.Average(data1, output);
  EXPECT_NEAR(output[0], data1[0] / 3.0f, e);
  EXPECT_NEAR(output[1], data1[1] / 3.0f, e);
  EXPECT_NEAR(output[2], data1[2] / 3.0f, e);
  EXPECT_NEAR(output[3], data1[3] / 3.0f, e);

  ma.Average(data2, output);
  EXPECT_NEAR(output[0], (data1[0] + data2[0]) / 3.0f, e);
  EXPECT_NEAR(output[1], (data1[1] + data2[1]) / 3.0f, e);
  EXPECT_NEAR(output[2], (data1[2] + data2[2]) / 3.0f, e);
  EXPECT_NEAR(output[3], (data1[3] + data2[3]) / 3.0f, e);

  ma.Average(data3, output);
  EXPECT_NEAR(output[0], (data1[0] + data2[0] + data3[0]) / 3.0f, e);
  EXPECT_NEAR(output[1], (data1[1] + data2[1] + data3[1]) / 3.0f, e);
  EXPECT_NEAR(output[2], (data1[2] + data2[2] + data3[2]) / 3.0f, e);
  EXPECT_NEAR(output[3], (data1[3] + data2[3] + data3[3]) / 3.0f, e);

  ma.Average(data4, output);
  EXPECT_NEAR(output[0], (data2[0] + data3[0] + data4[0]) / 3.0f, e);
  EXPECT_NEAR(output[1], (data2[1] + data3[1] + data4[1]) / 3.0f, e);
  EXPECT_NEAR(output[2], (data2[2] + data3[2] + data4[2]) / 3.0f, e);
  EXPECT_NEAR(output[3], (data2[3] + data3[3] + data4[3]) / 3.0f, e);
}

TEST(MovingAverage, PassThrough) {
  constexpr size_t num_elem = 4;
  constexpr size_t mem_len = 1;
  constexpr float e = 1e-6f;
  aec3::MovingAverage ma(num_elem, mem_len);
  std::array<float, num_elem> data1 = {1, 2, 3, 4};
  std::array<float, num_elem> data2 = {5, 1, 9, 7};
  std::array<float, num_elem> data3 = {3, 3, 5, 6};
  std::array<float, num_elem> data4 = {8, 4, 2, 1};
  std::array<float, num_elem> output;

  ma.Average(data1, output);
  EXPECT_NEAR(output[0], data1[0], e);
  EXPECT_NEAR(output[1], data1[1], e);
  EXPECT_NEAR(output[2], data1[2], e);
  EXPECT_NEAR(output[3], data1[3], e);

  ma.Average(data2, output);
  EXPECT_NEAR(output[0], data2[0], e);
  EXPECT_NEAR(output[1], data2[1], e);
  EXPECT_NEAR(output[2], data2[2], e);
  EXPECT_NEAR(output[3], data2[3], e);

  ma.Average(data3, output);
  EXPECT_NEAR(output[0], data3[0], e);
  EXPECT_NEAR(output[1], data3[1], e);
  EXPECT_NEAR(output[2], data3[2], e);
  EXPECT_NEAR(output[3], data3[3], e);

  ma.Average(data4, output);
  EXPECT_NEAR(output[0], data4[0], e);
  EXPECT_NEAR(output[1], data4[1], e);
  EXPECT_NEAR(output[2], data4[2], e);
  EXPECT_NEAR(output[3], data4[3], e);
}

}  // namespace webrtc
