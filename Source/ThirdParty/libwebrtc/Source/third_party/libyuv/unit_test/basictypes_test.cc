/*
 *  Copyright 2012 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "../unit_test/unit_test.h"
#include "libyuv/basic_types.h"

namespace libyuv {

TEST_F(LibYUVBaseTest, SizeOfTypes) {
  int8_t i8 = -1;
  uint8_t u8 = 1u;
  int16_t i16 = -1;
  uint16_t u16 = 1u;
  int32_t i32 = -1;
  uint32_t u32 = 1u;
  int64_t i64 = -1;
  uint64_t u64 = 1u;
  EXPECT_EQ(1u, sizeof(i8));
  EXPECT_EQ(1u, sizeof(u8));
  EXPECT_EQ(2u, sizeof(i16));
  EXPECT_EQ(2u, sizeof(u16));
  EXPECT_EQ(4u, sizeof(i32));
  EXPECT_EQ(4u, sizeof(u32));
  EXPECT_EQ(8u, sizeof(i64));
  EXPECT_EQ(8u, sizeof(u64));
  EXPECT_GT(0, i8);
  EXPECT_LT(0u, u8);
  EXPECT_GT(0, i16);
  EXPECT_LT(0u, u16);
  EXPECT_GT(0, i32);
  EXPECT_LT(0u, u32);
  EXPECT_GT(0, i64);
  EXPECT_LT(0u, u64);
}

}  // namespace libyuv
