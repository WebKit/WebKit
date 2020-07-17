// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/simple_tag_parser.h"

#include <cstdint>

#include "gtest/gtest.h"

#include "test_utils/element_parser_test.h"
#include "webm/id.h"
#include "webm/status.h"

using webm::ElementParserTest;
using webm::Id;
using webm::SimpleTag;
using webm::SimpleTagParser;
using webm::Status;

namespace {

class SimpleTagParserTest
    : public ElementParserTest<SimpleTagParser, Id::kSimpleTag> {};

TEST_F(SimpleTagParserTest, DefaultParse) {
  ParseAndVerify();

  const SimpleTag simple_tag = parser_.value();

  EXPECT_FALSE(simple_tag.name.is_present());
  EXPECT_EQ("", simple_tag.name.value());

  EXPECT_FALSE(simple_tag.language.is_present());
  EXPECT_EQ("und", simple_tag.language.value());

  EXPECT_FALSE(simple_tag.is_default.is_present());
  EXPECT_EQ(true, simple_tag.is_default.value());

  EXPECT_FALSE(simple_tag.string.is_present());
  EXPECT_EQ("", simple_tag.string.value());

  EXPECT_FALSE(simple_tag.binary.is_present());
  EXPECT_EQ(std::vector<std::uint8_t>{}, simple_tag.binary.value());

  EXPECT_EQ(static_cast<std::size_t>(0), simple_tag.tags.size());
}

TEST_F(SimpleTagParserTest, DefaultValues) {
  SetReaderData({
      0x45, 0xA3,  // ID = 0x45A3 (TagName).
      0x80,  // Size = 0.

      0x44, 0x7A,  // ID = 0x447A (TagLanguage).
      0x80,  // Size = 0.

      0x44, 0x84,  // ID = 0x4484 (TagDefault).
      0x80,  // Size = 0.

      0x44, 0x87,  // ID = 0x4487 (TagString).
      0x80,  // Size = 0.

      0x44, 0x85,  // ID = 0x4485 (TagBinary).
      0x80,  // Size = 0.

      0x67, 0xC8,  // ID = 0x67C8 (SimpleTag).
      0x80,  // Size = 0.
  });

  ParseAndVerify();

  const SimpleTag simple_tag = parser_.value();

  EXPECT_TRUE(simple_tag.name.is_present());
  EXPECT_EQ("", simple_tag.name.value());

  EXPECT_TRUE(simple_tag.language.is_present());
  EXPECT_EQ("und", simple_tag.language.value());

  EXPECT_TRUE(simple_tag.is_default.is_present());
  EXPECT_EQ(true, simple_tag.is_default.value());

  EXPECT_TRUE(simple_tag.string.is_present());
  EXPECT_EQ("", simple_tag.string.value());

  EXPECT_TRUE(simple_tag.binary.is_present());
  EXPECT_EQ(std::vector<std::uint8_t>{}, simple_tag.binary.value());

  ASSERT_EQ(static_cast<std::size_t>(1), simple_tag.tags.size());
  EXPECT_TRUE(simple_tag.tags[0].is_present());
  EXPECT_EQ(SimpleTag{}, simple_tag.tags[0].value());
}

TEST_F(SimpleTagParserTest, CustomValues) {
  SetReaderData({
      0x45, 0xA3,  // ID = 0x45A3 (TagName).
      0x81,  // Size = 1.
      0x61,  // Body (value = "a").

      0x44, 0x7A,  // ID = 0x447A (TagLanguage).
      0x81,  // Size = 1.
      0x62,  // Body (value = "b").

      0x44, 0x84,  // ID = 0x4484 (TagDefault).
      0x81,  // Size = 1.
      0x00,  // Body (value = 0).

      0x44, 0x87,  // ID = 0x4487 (TagString).
      0x81,  // Size = 1.
      0x63,  // Body (value = "c").

      0x44, 0x85,  // ID = 0x4485 (TagBinary).
      0x81,  // Size = 1.
      0x01,  // Body.

      0x67, 0xC8,  // ID = 0x67C8 (SimpleTag).
      0x99,  // Size = 25.

      0x44, 0x87,  //   ID = 0x4487 (TagString).
      0x81,  //   Size = 1.
      0x64,  //   Body (value = "d").

      0x67, 0xC8,  //   ID = 0x67C8 (SimpleTag).
      0x8B,  //   Size = 11.

      0x44, 0x87,  //     ID = 0x4487 (TagString).
      0x81,  //     Size = 1.
      0x65,  //     Body (value = "e").

      0x67, 0xC8,  //     ID = 0x67C8 (SimpleTag).
      0x84,  //     Size = 4.

      0x44, 0x87,  //       ID = 0x4487 (TagString).
      0x81,  //       Size = 1.
      0x66,  //       Body (value = "f").

      0x67, 0xC8,  //   ID = 0x67C8 (SimpleTag).
      0x84,  //   Size = 4.

      0x44, 0x87,  //     ID = 0x4487 (TagString).
      0x81,  //     Size = 1.
      0x67,  //     Body (value = "g").
  });

  ParseAndVerify();

  const SimpleTag simple_tag = parser_.value();

  EXPECT_TRUE(simple_tag.name.is_present());
  EXPECT_EQ("a", simple_tag.name.value());

  EXPECT_TRUE(simple_tag.language.is_present());
  EXPECT_EQ("b", simple_tag.language.value());

  EXPECT_TRUE(simple_tag.is_default.is_present());
  EXPECT_EQ(false, simple_tag.is_default.value());

  EXPECT_TRUE(simple_tag.string.is_present());
  EXPECT_EQ("c", simple_tag.string.value());

  EXPECT_TRUE(simple_tag.binary.is_present());
  EXPECT_EQ(std::vector<std::uint8_t>{0x01}, simple_tag.binary.value());

  SimpleTag expected;
  expected.string.Set("d", true);

  SimpleTag temp{};

  temp.string.Set("e", true);
  expected.tags.emplace_back(temp, true);

  temp.string.Set("f", true);
  expected.tags[0].mutable_value()->tags.emplace_back(temp, true);

  temp.string.Set("g", true);
  expected.tags.emplace_back(temp, true);

  ASSERT_EQ(static_cast<std::size_t>(1), simple_tag.tags.size());
  EXPECT_TRUE(simple_tag.tags[0].is_present());
  EXPECT_EQ(expected, simple_tag.tags[0].value());
}

TEST_F(SimpleTagParserTest, ExceedMaxRecursionDepth) {
  ResetParser(1);

  SetReaderData({
      0x67, 0xC8,  // ID = 0x67C8 (SimpleTag).
      0x80,  // Size = 0.
  });
  ParseAndVerify();

  SetReaderData({
      0x67, 0xC8,  // ID = 0x67C8 (SimpleTag).
      0x83,  // Size = 3.

      0x67, 0xC8,  // ID = 0x67C8 (SimpleTag).
      0x80,  //   Size = 0.
  });
  ParseAndExpectResult(Status::kExceededRecursionDepthLimit);
}

}  // namespace
