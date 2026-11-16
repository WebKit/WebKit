/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstddef>

#include "api/units/demo_unit_interop.rs.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(UnitsRustBridgeTest, TimestampInterop) {
  Timestamp ts = Timestamp::Millis(100);
  TimeDelta td = TimeDelta::Micros(20);
  EXPECT_EQ(ts + td, add_timestamp_with_time_delta(ts, td));
  EXPECT_EQ(td / 2, half_time_delta(td));
}

}  // namespace
}  // namespace webrtc
