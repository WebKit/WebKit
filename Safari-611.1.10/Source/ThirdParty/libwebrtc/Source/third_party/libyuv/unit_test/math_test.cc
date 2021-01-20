/*
 *  Copyright 2013 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../unit_test/unit_test.h"
#include "libyuv/basic_types.h"
#include "libyuv/cpu_id.h"
#include "libyuv/scale.h"

#ifdef ENABLE_ROW_TESTS
#include "libyuv/scale_row.h"
#endif

namespace libyuv {

#ifdef ENABLE_ROW_TESTS
TEST_F(LibYUVBaseTest, TestFixedDiv) {
  int num[1280];
  int div[1280];
  int result_opt[1280];
  int result_c[1280];

  EXPECT_EQ(0x10000, libyuv::FixedDiv(1, 1));
  EXPECT_EQ(0x7fff0000, libyuv::FixedDiv(0x7fff, 1));
  // TODO(fbarchard): Avoid the following that throw exceptions.
  // EXPECT_EQ(0x100000000, libyuv::FixedDiv(0x10000, 1));
  // EXPECT_EQ(0x80000000, libyuv::FixedDiv(0x8000, 1));

  EXPECT_EQ(0x20000, libyuv::FixedDiv(640 * 2, 640));
  EXPECT_EQ(0x30000, libyuv::FixedDiv(640 * 3, 640));
  EXPECT_EQ(0x40000, libyuv::FixedDiv(640 * 4, 640));
  EXPECT_EQ(0x50000, libyuv::FixedDiv(640 * 5, 640));
  EXPECT_EQ(0x60000, libyuv::FixedDiv(640 * 6, 640));
  EXPECT_EQ(0x70000, libyuv::FixedDiv(640 * 7, 640));
  EXPECT_EQ(0x80000, libyuv::FixedDiv(640 * 8, 640));
  EXPECT_EQ(0xa0000, libyuv::FixedDiv(640 * 10, 640));
  EXPECT_EQ(0x20000, libyuv::FixedDiv(960 * 2, 960));
  EXPECT_EQ(0x08000, libyuv::FixedDiv(640 / 2, 640));
  EXPECT_EQ(0x04000, libyuv::FixedDiv(640 / 4, 640));
  EXPECT_EQ(0x20000, libyuv::FixedDiv(1080 * 2, 1080));
  EXPECT_EQ(0x20000, libyuv::FixedDiv(200000, 100000));
  EXPECT_EQ(0x18000, libyuv::FixedDiv(150000, 100000));
  EXPECT_EQ(0x20000, libyuv::FixedDiv(40000, 20000));
  EXPECT_EQ(0x20000, libyuv::FixedDiv(-40000, -20000));
  EXPECT_EQ(-0x20000, libyuv::FixedDiv(40000, -20000));
  EXPECT_EQ(-0x20000, libyuv::FixedDiv(-40000, 20000));
  EXPECT_EQ(0x10000, libyuv::FixedDiv(4095, 4095));
  EXPECT_EQ(0x10000, libyuv::FixedDiv(4096, 4096));
  EXPECT_EQ(0x10000, libyuv::FixedDiv(4097, 4097));
  EXPECT_EQ(123 * 65536, libyuv::FixedDiv(123, 1));

  for (int i = 1; i < 4100; ++i) {
    EXPECT_EQ(0x10000, libyuv::FixedDiv(i, i));
    EXPECT_EQ(0x20000, libyuv::FixedDiv(i * 2, i));
    EXPECT_EQ(0x30000, libyuv::FixedDiv(i * 3, i));
    EXPECT_EQ(0x40000, libyuv::FixedDiv(i * 4, i));
    EXPECT_EQ(0x08000, libyuv::FixedDiv(i, i * 2));
    EXPECT_NEAR(16384 * 65536 / i, libyuv::FixedDiv(16384, i), 1);
  }
  EXPECT_EQ(123 * 65536, libyuv::FixedDiv(123, 1));

  MemRandomize(reinterpret_cast<uint8_t*>(&num[0]), sizeof(num));
  MemRandomize(reinterpret_cast<uint8_t*>(&div[0]), sizeof(div));
  for (int j = 0; j < 1280; ++j) {
    if (div[j] == 0) {
      div[j] = 1280;
    }
    num[j] &= 0xffff;  // Clamp to avoid divide overflow.
  }
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    for (int j = 0; j < 1280; ++j) {
      result_opt[j] = libyuv::FixedDiv(num[j], div[j]);
    }
  }
  for (int j = 0; j < 1280; ++j) {
    result_c[j] = libyuv::FixedDiv_C(num[j], div[j]);
    EXPECT_NEAR(result_c[j], result_opt[j], 1);
  }
}

TEST_F(LibYUVBaseTest, TestFixedDiv_Opt) {
  int num[1280];
  int div[1280];
  int result_opt[1280];
  int result_c[1280];

  MemRandomize(reinterpret_cast<uint8_t*>(&num[0]), sizeof(num));
  MemRandomize(reinterpret_cast<uint8_t*>(&div[0]), sizeof(div));
  for (int j = 0; j < 1280; ++j) {
    num[j] &= 4095;  // Make numerator smaller.
    div[j] &= 4095;  // Make divisor smaller.
    if (div[j] == 0) {
      div[j] = 1280;
    }
  }

  int has_x86 = TestCpuFlag(kCpuHasX86);
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    if (has_x86) {
      for (int j = 0; j < 1280; ++j) {
        result_opt[j] = libyuv::FixedDiv(num[j], div[j]);
      }
    } else {
      for (int j = 0; j < 1280; ++j) {
        result_opt[j] = libyuv::FixedDiv_C(num[j], div[j]);
      }
    }
  }
  for (int j = 0; j < 1280; ++j) {
    result_c[j] = libyuv::FixedDiv_C(num[j], div[j]);
    EXPECT_NEAR(result_c[j], result_opt[j], 1);
  }
}

TEST_F(LibYUVBaseTest, TestFixedDiv1_Opt) {
  int num[1280];
  int div[1280];
  int result_opt[1280];
  int result_c[1280];

  MemRandomize(reinterpret_cast<uint8_t*>(&num[0]), sizeof(num));
  MemRandomize(reinterpret_cast<uint8_t*>(&div[0]), sizeof(div));
  for (int j = 0; j < 1280; ++j) {
    num[j] &= 4095;  // Make numerator smaller.
    div[j] &= 4095;  // Make divisor smaller.
    if (div[j] <= 1) {
      div[j] = 1280;
    }
  }

  int has_x86 = TestCpuFlag(kCpuHasX86);
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    if (has_x86) {
      for (int j = 0; j < 1280; ++j) {
        result_opt[j] = libyuv::FixedDiv1(num[j], div[j]);
      }
    } else {
      for (int j = 0; j < 1280; ++j) {
        result_opt[j] = libyuv::FixedDiv1_C(num[j], div[j]);
      }
    }
  }
  for (int j = 0; j < 1280; ++j) {
    result_c[j] = libyuv::FixedDiv1_C(num[j], div[j]);
    EXPECT_NEAR(result_c[j], result_opt[j], 1);
  }
}
#endif  // ENABLE_ROW_TESTS

}  // namespace libyuv
