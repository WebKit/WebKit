/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/zero_memory.h"

#include <stdint.h>

#include "api/array_view.h"
#include "test/gtest.h"

namespace rtc {

TEST(ZeroMemoryTest, TestZeroMemory) {
  static const size_t kBufferSize = 32;
  uint8_t buffer[kBufferSize];
  for (size_t i = 0; i < kBufferSize; i++) {
    buffer[i] = static_cast<uint8_t>(i + 1);
  }
  ExplicitZeroMemory(buffer, sizeof(buffer));
  for (size_t i = 0; i < kBufferSize; i++) {
    EXPECT_EQ(buffer[i], 0);
  }
}

TEST(ZeroMemoryTest, TestZeroArrayView) {
  static const size_t kBufferSize = 32;
  uint8_t buffer[kBufferSize];
  for (size_t i = 0; i < kBufferSize; i++) {
    buffer[i] = static_cast<uint8_t>(i + 1);
  }
  ExplicitZeroMemory(rtc::ArrayView<uint8_t>(buffer, sizeof(buffer)));
  for (size_t i = 0; i < kBufferSize; i++) {
    EXPECT_EQ(buffer[i], 0);
  }
}

// While this test doesn't actually test anything, it can be used to check
// the compiler output to make sure the call to "ExplicitZeroMemory" is not
// optimized away.
TEST(ZeroMemoryTest, TestZeroMemoryUnused) {
  static const size_t kBufferSize = 32;
  uint8_t buffer[kBufferSize];
  ExplicitZeroMemory(buffer, sizeof(buffer));
}

}  // namespace rtc
