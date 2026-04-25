/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/near_matcher.h"

#include <optional>

#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::Not;

TEST(NearMatcherTest, MarginIsExclusive) {
  EXPECT_THAT(6, Not(Near(10, 3)));
  EXPECT_THAT(7, Not(Near(10, 3)));
  EXPECT_THAT(8, Near(10, 3));
  EXPECT_THAT(10, Near(10, 3));
  EXPECT_THAT(12, Near(10, 3));
  EXPECT_THAT(13, Not(Near(10, 3)));
  EXPECT_THAT(14, Not(Near(10, 3)));
}

// This test intentianally contains failed expectation. Run it manually with
// `--gtest_also_run_disabled_tests` flag to check how error message looks like
// in various scenarios.
TEST(NearMatcherTest, DISABLED_PrintsDetailedError) {
  EXPECT_THAT(Timestamp::Millis(5), Near(Timestamp::Millis(10)));
  EXPECT_THAT(Timestamp::Millis(15), Near(Timestamp::Millis(10)));
  EXPECT_THAT(Timestamp::MinusInfinity(),
              Near(Timestamp::Millis(10), TimeDelta::Millis(20)));

  EXPECT_THAT(Timestamp::Millis(11),
              Not(Near(Timestamp::Millis(10), TimeDelta::Millis(5))));
  EXPECT_THAT(Timestamp::Millis(11),
              Not(Near(Timestamp::Millis(10), TimeDelta::Millis(20))));
}

TEST(NearMatcherTest, MatchWebrtcTypes) {
  EXPECT_THAT(Timestamp::Millis(1'002),
              Near(Timestamp::Seconds(1), TimeDelta::Millis(3)));
  EXPECT_THAT(TimeDelta::Millis(1'002),
              Near(TimeDelta::Seconds(1), TimeDelta::Millis(3)));
  EXPECT_THAT(DataRate::BitsPerSec(1'234'005),
              Near(DataRate::KilobitsPerSec(1'234), DataRate::BitsPerSec(10)));
}

TEST(NearMatcherTest, DefaultMarginForTimeTypesIs1ms) {
  EXPECT_THAT(Timestamp::Micros(999'001), Near(Timestamp::Seconds(1)));
  EXPECT_THAT(Timestamp::Millis(999), Not(Near(Timestamp::Seconds(1))));

  EXPECT_THAT(TimeDelta::Micros(1'000'999), Near(TimeDelta::Seconds(1)));
  EXPECT_THAT(TimeDelta::Millis(1'001), Not(Near(TimeDelta::Seconds(1))));
}

TEST(NearMatcherTest, CanMatchTypesWrappedIntoOptional) {
  // nullopt is less than any non-optional and thus always fails the match.
  EXPECT_THAT(std::optional<Timestamp>(std::nullopt),
              Not(Near(Timestamp::Seconds(1), TimeDelta::Millis(10))));

  EXPECT_THAT(std::optional(Timestamp::Millis(1'002)),
              Near(Timestamp::Seconds(1), TimeDelta::Millis(10)));
}

TEST(NearMatcherTest, CanMatchTimestampNearZero) {
  EXPECT_THAT(Timestamp::Zero(), Near(Timestamp::Zero()));

  // Check lower bound behave as usual when `max_error.us() == expected.us()`
  EXPECT_THAT(Timestamp::Micros(1),
              Near(Timestamp::Millis(10), TimeDelta::Millis(10)));
  EXPECT_THAT(Timestamp::Zero(),
              Not(Near(Timestamp::Millis(10), TimeDelta::Millis(10))));

  // max_error.us() > expected.us() scenario shouldn't compare with negative
  // `Timestamp` values while they are invalid.
  EXPECT_THAT(Timestamp::Micros(1),
              Near(Timestamp::Millis(10), TimeDelta::Millis(11)));
  EXPECT_THAT(Timestamp::Zero(),
              Near(Timestamp::Millis(10), TimeDelta::Millis(11)));

  // Some values still can be too small when lower bound is below zero.
  EXPECT_THAT(Timestamp::MinusInfinity(),
              Not(Near(Timestamp::Millis(10), TimeDelta::Millis(11))));
  EXPECT_THAT(std::optional<Timestamp>(std::nullopt),
              Not(Near(Timestamp::Millis(10), TimeDelta::Millis(11))));

  // Checks on the upper bounds should work in `max_error.us() > expected.us()`
  // scenario same as in more common 'max_error.us() <= expected.us()` scenario.
  EXPECT_THAT(Timestamp::Micros(20'999),
              Near(Timestamp::Millis(10), TimeDelta::Millis(11)));
  EXPECT_THAT(Timestamp::Millis(21),
              Not(Near(Timestamp::Millis(10), TimeDelta::Millis(11))));
}

}  // namespace
}  // namespace webrtc
