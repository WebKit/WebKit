/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/numerics/windowed_min_filter.h"

#include <string>

#include "api/units/time_delta.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(WindowedMinFilterTest, EmptyFilterReturnZero) {
  WindowedMinFilter<TimeDelta> filter(/*window_length=*/3);
  EXPECT_EQ(filter.GetMin(), TimeDelta::Zero());
}

TEST(WindowedMinFilterTest, EmptyFilterReturnEmptyString) {
  WindowedMinFilter<std::string> filter(/*window_length=*/3);
  EXPECT_EQ(filter.GetMin(), "");
}

TEST(WindowedMinFilterTest, GetMinReturnsMin) {
  WindowedMinFilter<int> filter(/*window_length=*/3);

  filter.Insert(30);
  EXPECT_EQ(filter.GetMin(), 30);
  filter.Insert(20);
  EXPECT_EQ(filter.GetMin(), 20);
  filter.Insert(10);
  EXPECT_EQ(filter.GetMin(), 10);
}

TEST(WindowedMinFilterTest, GetMinReturnsMinNotSortedInput) {
  WindowedMinFilter<int> filter(/*window_length=*/4);

  filter.Insert(0);
  filter.Insert(30);
  EXPECT_EQ(filter.GetMin(), 0);
  filter.Insert(10);
  EXPECT_EQ(filter.GetMin(), 0);
  filter.Insert(40);
  EXPECT_EQ(filter.GetMin(), 0);
  filter.Insert(40);
  EXPECT_EQ(filter.GetMin(), 10);
}

TEST(WindowedMinFilterTest, GetMinReturnsMinWithStringsNotSorted) {
  WindowedMinFilter<std::string> filter(/*window_length=*/3);

  filter.Insert("bbb");
  EXPECT_EQ(filter.GetMin(), "bbb");
  filter.Insert("ccc");
  EXPECT_EQ(filter.GetMin(), "bbb");
  filter.Insert("aaa");
  EXPECT_EQ(filter.GetMin(), "aaa");
}

TEST(WindowedMinFilterTest, GetMinReturnsMinInWindow) {
  WindowedMinFilter<int> filter(/*window_length=*/3);

  filter.Insert(10);
  filter.Insert(20);
  filter.Insert(30);
  EXPECT_EQ(filter.GetMin(), 10);
  filter.Insert(40);
  EXPECT_EQ(filter.GetMin(), 20);
  filter.Insert(50);
  EXPECT_EQ(filter.GetMin(), 30);
}

TEST(WindowedMinFilterTest, RestartAfterReset) {
  WindowedMinFilter<int> filter(/*window_length=*/3);

  filter.Insert(10);
  filter.Insert(20);
  ASSERT_EQ(filter.GetMin(), 10);
  filter.Reset();
  EXPECT_EQ(filter.GetMin(), 0);
  filter.Insert(30);
  EXPECT_EQ(filter.GetMin(), 30);
}

}  // namespace
}  // namespace webrtc
