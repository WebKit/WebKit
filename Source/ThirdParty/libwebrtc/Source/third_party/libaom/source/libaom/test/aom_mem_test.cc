/*
 * Copyright (c) 2021, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "aom_mem/aom_mem.h"

#include <cstdio>
#include <cstddef>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

TEST(AomMemTest, Overflow) {
  // Allocations are aligned > 1 so SIZE_MAX should always fail.
  ASSERT_EQ(aom_malloc(SIZE_MAX), nullptr);
  ASSERT_EQ(aom_calloc(1, SIZE_MAX), nullptr);
  ASSERT_EQ(aom_calloc(32, SIZE_MAX / 32), nullptr);
  ASSERT_EQ(aom_calloc(SIZE_MAX, SIZE_MAX), nullptr);
  ASSERT_EQ(aom_memalign(1, SIZE_MAX), nullptr);
  ASSERT_EQ(aom_memalign(64, SIZE_MAX), nullptr);
  ASSERT_EQ(aom_memalign(64, SIZE_MAX - 64), nullptr);
  ASSERT_EQ(aom_memalign(64, SIZE_MAX - 64 - sizeof(size_t) + 2), nullptr);
}

TEST(AomMemTest, NullParams) {
  ASSERT_EQ(aom_memset16(nullptr, 0, 0), nullptr);
  aom_free(nullptr);
}
