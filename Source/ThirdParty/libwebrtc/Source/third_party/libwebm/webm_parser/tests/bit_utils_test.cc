// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/bit_utils.h"

#include "gtest/gtest.h"

using webm::CountLeadingZeros;

namespace {

class BitUtilsTest : public testing::Test {};

TEST_F(BitUtilsTest, CountLeadingZeros) {
  EXPECT_EQ(8, CountLeadingZeros(0x00));
  EXPECT_EQ(4, CountLeadingZeros(0x0f));
  EXPECT_EQ(0, CountLeadingZeros(0xf0));
}

}  // namespace
