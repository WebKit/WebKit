/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/network_route.h"

#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace rtc {

TEST(NetworkRoute, Equals) {
  NetworkRoute r1;
  NetworkRoute r2 = r1;
  EXPECT_TRUE(r1 == r2);
}

}  // namespace rtc
