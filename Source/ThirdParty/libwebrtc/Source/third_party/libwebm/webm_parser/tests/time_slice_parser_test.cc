// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/time_slice_parser.h"

#include "gtest/gtest.h"

#include "test_utils/element_parser_test.h"
#include "webm/id.h"

using webm::ElementParserTest;
using webm::Id;
using webm::TimeSlice;
using webm::TimeSliceParser;

namespace {

class TimeSliceParserTest
    : public ElementParserTest<TimeSliceParser, Id::kTimeSlice> {};

TEST_F(TimeSliceParserTest, DefaultParse) {
  ParseAndVerify();

  const TimeSlice time_slice = parser_.value();

  EXPECT_FALSE(time_slice.lace_number.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), time_slice.lace_number.value());
}

TEST_F(TimeSliceParserTest, DefaultValues) {
  SetReaderData({
      0xCC,  // ID = 0xCC (LaceNumber).
      0x80,  // Size = 0.
  });

  ParseAndVerify();

  const TimeSlice time_slice = parser_.value();

  EXPECT_TRUE(time_slice.lace_number.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), time_slice.lace_number.value());
}

TEST_F(TimeSliceParserTest, CustomValues) {
  SetReaderData({
      0xCC,  // ID = 0xCC (LaceNumber).
      0x81,  // Size = 1.
      0x01,  // Body (value = 1).
  });

  ParseAndVerify();

  const TimeSlice time_slice = parser_.value();

  EXPECT_TRUE(time_slice.lace_number.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(1), time_slice.lace_number.value());
}

}  // namespace
