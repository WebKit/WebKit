// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/tag_parser.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "test_utils/element_parser_test.h"
#include "webm/id.h"

using webm::ElementParserTest;
using webm::Id;
using webm::SimpleTag;
using webm::Tag;
using webm::TagParser;
using webm::Targets;

namespace {

class TagParserTest : public ElementParserTest<TagParser, Id::kTag> {};

TEST_F(TagParserTest, DefaultParse) {
  EXPECT_CALL(callback_, OnTag(metadata_, Tag{})).Times(1);

  ParseAndVerify();
}

TEST_F(TagParserTest, DefaultValues) {
  SetReaderData({
      0x63, 0xC0,  // ID = 0x63C0 (Targets).
      0x80,  // Size = 0.

      0x67, 0xC8,  // ID = 0x67C8 (SimpleTag).
      0x80,  // Size = 0.
  });

  Tag tag;
  tag.targets.Set({}, true);
  tag.tags.emplace_back();
  tag.tags[0].Set({}, true);

  EXPECT_CALL(callback_, OnTag(metadata_, tag)).Times(1);

  ParseAndVerify();
}

TEST_F(TagParserTest, CustomValues) {
  SetReaderData({
      0x63, 0xC0,  // ID = 0x63C0 (Targets).
      0x84,  // Size = 4.

      0x68, 0xCA,  //   ID = 0x68CA (TargetTypeValue).
      0x81,  //   Size = 1.
      0x00,  //   Body (value = 0).

      0x67, 0xC8,  // ID = 0x67C8 (SimpleTag).
      0x84,  // Size = 4.

      0x45, 0xA3,  //   ID = 0x45A3 (TagName).
      0x81,  //   Size = 1.
      0x61,  //   Body (value = "a").

      0x67, 0xC8,  // ID = 0x67C8 (SimpleTag).
      0x84,  // Size = 4.

      0x44, 0x7A,  //   ID = 0x447A (TagLanguage).
      0x81,  //   Size = 1.
      0x62,  //   Body (value = "b").
  });

  Tag tag;
  Targets targets;
  targets.type_value.Set(0, true);
  tag.targets.Set(targets, true);
  SimpleTag simple_tag;
  simple_tag.name.Set("a", true);
  tag.tags.emplace_back(simple_tag, true);
  simple_tag = {};
  simple_tag.language.Set("b", true);
  tag.tags.emplace_back(simple_tag, true);

  EXPECT_CALL(callback_, OnTag(metadata_, tag)).Times(1);

  ParseAndVerify();
}

}  // namespace
