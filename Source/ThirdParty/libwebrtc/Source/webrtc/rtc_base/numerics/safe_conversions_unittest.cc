/*
 *  Copyright 2026 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/numerics/safe_conversions.h"

#include <cstdint>
#include <limits>

#include "test/gtest.h"

namespace webrtc {

TEST(SafeConversionsTest, SaturatedCastDoubleToInt64) {
  // Representable double in range.
  double val_in_range = 9223372036854774784.0;
  EXPECT_EQ(saturated_cast<int64_t>(val_in_range), 9223372036854774784LL);

  // Double that rounds to 2^63 (overflow).
  double val_overflow =
      9223372036854775800.0;  // Rounds to 9223372036854775808.0
  EXPECT_EQ(saturated_cast<int64_t>(val_overflow),
            std::numeric_limits<int64_t>::max());
}

}  // namespace webrtc
