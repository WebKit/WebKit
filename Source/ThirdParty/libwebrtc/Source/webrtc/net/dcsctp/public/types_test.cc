/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/public/types.h"

#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {

TEST(TypesTest, DurationOperators) {
  DurationMs d1(10);
  DurationMs d2(25);
  EXPECT_EQ(d1 + d2, DurationMs(35));
  EXPECT_EQ(d2 - d1, DurationMs(15));

  d1 += d2;
  EXPECT_EQ(d1, DurationMs(35));

  d1 -= DurationMs(5);
  EXPECT_EQ(d1, DurationMs(30));

  d1 *= 1.5;
  EXPECT_EQ(d1, DurationMs(45));

  EXPECT_EQ(DurationMs(10) * 2, DurationMs(20));
}

TEST(TypesTest, TimeOperators) {
  EXPECT_EQ(TimeMs(250) + DurationMs(100), TimeMs(350));
  EXPECT_EQ(DurationMs(250) + TimeMs(100), TimeMs(350));
  EXPECT_EQ(TimeMs(250) - DurationMs(100), TimeMs(150));
  EXPECT_EQ(TimeMs(250) - TimeMs(100), DurationMs(150));

  TimeMs t1(150);
  t1 -= DurationMs(50);
  EXPECT_EQ(t1, TimeMs(100));
  t1 += DurationMs(200);
  EXPECT_EQ(t1, TimeMs(300));
}

}  // namespace
}  // namespace dcsctp
