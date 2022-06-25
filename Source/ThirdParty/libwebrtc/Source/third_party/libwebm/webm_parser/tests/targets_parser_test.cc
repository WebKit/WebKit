// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/targets_parser.h"

#include "gtest/gtest.h"

#include "test_utils/element_parser_test.h"
#include "webm/id.h"

using webm::ElementParserTest;
using webm::Id;
using webm::Targets;
using webm::TargetsParser;

namespace {

class TargetsParserTest
    : public ElementParserTest<TargetsParser, Id::kTargets> {};

TEST_F(TargetsParserTest, DefaultParse) {
  ParseAndVerify();

  const Targets targets = parser_.value();

  EXPECT_FALSE(targets.type_value.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(50), targets.type_value.value());

  EXPECT_FALSE(targets.type.is_present());
  EXPECT_EQ("", targets.type.value());

  EXPECT_EQ(static_cast<std::size_t>(0), targets.track_uids.size());
}

TEST_F(TargetsParserTest, DefaultValues) {
  SetReaderData({
      0x68, 0xCA,  // ID = 0x68CA (TargetTypeValue).
      0x80,  // Size = 0.

      0x63, 0xCA,  // ID = 0x63CA (TargetType).
      0x80,  // Size = 0.

      0x63, 0xC5,  // ID = 0x63C5 (TagTrackUID).
      0x80,  // Size = 0.
  });

  ParseAndVerify();

  const Targets targets = parser_.value();

  EXPECT_TRUE(targets.type_value.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(50), targets.type_value.value());

  EXPECT_TRUE(targets.type.is_present());
  EXPECT_EQ("", targets.type.value());

  ASSERT_EQ(static_cast<std::size_t>(1), targets.track_uids.size());
  EXPECT_TRUE(targets.track_uids[0].is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), targets.track_uids[0].value());
}

TEST_F(TargetsParserTest, CustomValues) {
  SetReaderData({
      0x68, 0xCA,  // ID = 0x68CA (TargetTypeValue).
      0x81,  // Size = 1.
      0x00,  // Body (value = 0).

      0x63, 0xCA,  // ID = 0x63CA (TargetType).
      0x82,  // Size = 2.
      0x48, 0x69,  // Body (value = "Hi").

      0x63, 0xC5,  // ID = 0x63C5 (TagTrackUID).
      0x81,  // Size = 1.
      0x01,  // Body (value = 1).

      0x63, 0xC5,  // ID = 0x63C5 (TagTrackUID).
      0x81,  // Size = 1.
      0x02,  // Body (value = 2).
  });

  ParseAndVerify();

  const Targets targets = parser_.value();

  EXPECT_TRUE(targets.type_value.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), targets.type_value.value());

  EXPECT_TRUE(targets.type.is_present());
  EXPECT_EQ("Hi", targets.type.value());

  ASSERT_EQ(static_cast<std::size_t>(2), targets.track_uids.size());
  EXPECT_TRUE(targets.track_uids[0].is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(1), targets.track_uids[0].value());
  EXPECT_TRUE(targets.track_uids[1].is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(2), targets.track_uids[1].value());
}

}  // namespace
