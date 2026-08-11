/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/saturation_protector_buffer.h"

#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::Eq;
using ::testing::Optional;

TEST(GainController2SaturationProtectorBuffer, Init) {
  SaturationProtectorBuffer b;
  EXPECT_EQ(b.Size(), 0);
  EXPECT_FALSE(b.Front().has_value());
}

TEST(GainController2SaturationProtectorBuffer, PushBack) {
  SaturationProtectorBuffer b;
  constexpr float kValue = 123.0f;
  b.PushBack(kValue);
  EXPECT_EQ(b.Size(), 1);
  EXPECT_THAT(b.Front(), Optional(Eq(kValue)));
}

TEST(GainController2SaturationProtectorBuffer, Reset) {
  SaturationProtectorBuffer b;
  b.PushBack(123.0f);
  b.Reset();
  EXPECT_EQ(b.Size(), 0);
  EXPECT_FALSE(b.Front().has_value());
}

// Checks that the front value does not change until the ring buffer gets full.
TEST(GainController2SaturationProtectorBuffer, FrontUntilBufferIsFull) {
  SaturationProtectorBuffer b;
  constexpr float kValue = 123.0f;
  b.PushBack(kValue);
  for (int i = 1; i < b.Capacity(); ++i) {
    SCOPED_TRACE(i);
    EXPECT_THAT(b.Front(), Optional(Eq(kValue)));
    b.PushBack(kValue + i);
  }
}

// Checks that when the buffer is full it behaves as a shift register.
TEST(GainController2SaturationProtectorBuffer, FrontIsDelayed) {
  SaturationProtectorBuffer b;
  // Fill the buffer.
  for (int i = 0; i < b.Capacity(); ++i) {
    b.PushBack(i);
  }
  // The ring buffer should now behave as a shift register with a delay equal to
  // its capacity.
  for (int i = b.Capacity(); i < 2 * b.Capacity() + 1; ++i) {
    SCOPED_TRACE(i);
    EXPECT_THAT(b.Front(), Optional(Eq(i - b.Capacity())));
    b.PushBack(i);
  }
}

}  // namespace
}  // namespace webrtc
