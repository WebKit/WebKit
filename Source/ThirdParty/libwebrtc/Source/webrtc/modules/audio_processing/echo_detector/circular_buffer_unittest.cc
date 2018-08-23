/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/echo_detector/circular_buffer.h"
#include "test/gtest.h"

namespace webrtc {

TEST(CircularBufferTests, LessThanMaxTest) {
  CircularBuffer test_buffer(3);
  test_buffer.Push(1.f);
  test_buffer.Push(2.f);
  EXPECT_EQ(1.f, test_buffer.Pop());
  EXPECT_EQ(2.f, test_buffer.Pop());
}

TEST(CircularBufferTests, FillTest) {
  CircularBuffer test_buffer(3);
  test_buffer.Push(1.f);
  test_buffer.Push(2.f);
  test_buffer.Push(3.f);
  EXPECT_EQ(1.f, test_buffer.Pop());
  EXPECT_EQ(2.f, test_buffer.Pop());
  EXPECT_EQ(3.f, test_buffer.Pop());
}

TEST(CircularBufferTests, OverflowTest) {
  CircularBuffer test_buffer(3);
  test_buffer.Push(1.f);
  test_buffer.Push(2.f);
  test_buffer.Push(3.f);
  test_buffer.Push(4.f);
  // Because the circular buffer has a size of 3, the first insert should have
  // been forgotten.
  EXPECT_EQ(2.f, test_buffer.Pop());
  EXPECT_EQ(3.f, test_buffer.Pop());
  EXPECT_EQ(4.f, test_buffer.Pop());
}

TEST(CircularBufferTests, ReadFromEmpty) {
  CircularBuffer test_buffer(3);
  EXPECT_EQ(absl::nullopt, test_buffer.Pop());
}

}  // namespace webrtc
