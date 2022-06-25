// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/chapter_display_parser.h"

#include "gtest/gtest.h"

#include "test_utils/element_parser_test.h"
#include "webm/id.h"

using webm::ChapterDisplay;
using webm::ChapterDisplayParser;
using webm::ElementParserTest;
using webm::Id;

namespace {

class ChapterDisplayParserTest
    : public ElementParserTest<ChapterDisplayParser, Id::kChapterDisplay> {};

TEST_F(ChapterDisplayParserTest, DefaultParse) {
  ParseAndVerify();

  const ChapterDisplay chapter_display = parser_.value();

  EXPECT_FALSE(chapter_display.string.is_present());
  EXPECT_EQ("", chapter_display.string.value());

  ASSERT_EQ(static_cast<std::uint64_t>(1), chapter_display.languages.size());
  EXPECT_FALSE(chapter_display.languages[0].is_present());
  EXPECT_EQ("eng", chapter_display.languages[0].value());

  EXPECT_EQ(static_cast<std::size_t>(0), chapter_display.countries.size());
}

TEST_F(ChapterDisplayParserTest, DefaultValues) {
  SetReaderData({
      0x85,  // ID = 0x85 (ChapString).
      0x40, 0x00,  // Size = 0.

      0x43, 0x7C,  // ID = 0x437C (ChapLanguage).
      0x80,  // Size = 0.

      0x43, 0x7E,  // ID = 0x437E (ChapCountry).
      0x80,  // Size = 0.
  });

  ParseAndVerify();

  const ChapterDisplay chapter_display = parser_.value();

  EXPECT_TRUE(chapter_display.string.is_present());
  EXPECT_EQ("", chapter_display.string.value());

  ASSERT_EQ(static_cast<std::size_t>(1), chapter_display.languages.size());
  EXPECT_TRUE(chapter_display.languages[0].is_present());
  EXPECT_EQ("eng", chapter_display.languages[0].value());

  ASSERT_EQ(static_cast<std::size_t>(1), chapter_display.countries.size());
  EXPECT_TRUE(chapter_display.countries[0].is_present());
  EXPECT_EQ("", chapter_display.countries[0].value());
}

TEST_F(ChapterDisplayParserTest, CustomValues) {
  SetReaderData({
      0x85,  // ID = 0x85 (ChapString).
      0x40, 0x05,  // Size = 5.
      0x68, 0x65, 0x6C, 0x6C, 0x6F,  // Body (value = "hello").

      0x43, 0x7C,  // ID = 0x437C (ChapLanguage).
      0x85,  // Size = 5.
      0x6C, 0x61, 0x6E, 0x67, 0x30,  // body (value = "lang0").

      0x43, 0x7E,  // ID = 0x437E (ChapCountry).
      0x85,  // Size = 5.
      0x61, 0x72, 0x65, 0x61, 0x30,  // Body (value = "area0").

      0x43, 0x7C,  // ID = 0x437C (ChapLanguage).
      0x85,  // Size = 5.
      0x6C, 0x61, 0x6E, 0x67, 0x31,  // body (value = "lang1").

      0x43, 0x7C,  // ID = 0x437C (ChapLanguage).
      0x85,  // Size = 5.
      0x6C, 0x61, 0x6E, 0x67, 0x32,  // body (value = "lang2").

      0x43, 0x7E,  // ID = 0x437E (ChapCountry).
      0x85,  // Size = 5.
      0x61, 0x72, 0x65, 0x61, 0x31,  // Body (value = "area1").
  });

  ParseAndVerify();

  const ChapterDisplay chapter_display = parser_.value();

  EXPECT_TRUE(chapter_display.string.is_present());
  EXPECT_EQ("hello", chapter_display.string.value());

  ASSERT_EQ(static_cast<std::size_t>(3), chapter_display.languages.size());
  EXPECT_TRUE(chapter_display.languages[0].is_present());
  EXPECT_EQ("lang0", chapter_display.languages[0].value());
  EXPECT_TRUE(chapter_display.languages[1].is_present());
  EXPECT_EQ("lang1", chapter_display.languages[1].value());
  EXPECT_TRUE(chapter_display.languages[2].is_present());
  EXPECT_EQ("lang2", chapter_display.languages[2].value());

  ASSERT_EQ(static_cast<std::size_t>(2), chapter_display.countries.size());
  EXPECT_TRUE(chapter_display.countries[0].is_present());
  EXPECT_EQ("area0", chapter_display.countries[0].value());
  EXPECT_TRUE(chapter_display.countries[1].is_present());
  EXPECT_EQ("area1", chapter_display.countries[1].value());
}

}  // namespace
