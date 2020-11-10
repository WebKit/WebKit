/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/strings/string_format.h"

#include "rtc_base/checks.h"
#include "test/gtest.h"

namespace rtc {

TEST(StringFormatTest, Empty) {
  EXPECT_EQ("", StringFormat("%s", ""));
}

TEST(StringFormatTest, Misc) {
  EXPECT_EQ("123hello w", StringFormat("%3d%2s %1c", 123, "hello", 'w'));
  EXPECT_EQ("3 = three", StringFormat("%d = %s", 1 + 2, "three"));
}

TEST(StringFormatTest, MaxSizeShouldWork) {
  const int kSrcLen = 512;
  char str[kSrcLen];
  std::fill_n(str, kSrcLen, 'A');
  str[kSrcLen - 1] = 0;
  EXPECT_EQ(str, StringFormat("%s", str));
}

}  // namespace rtc
