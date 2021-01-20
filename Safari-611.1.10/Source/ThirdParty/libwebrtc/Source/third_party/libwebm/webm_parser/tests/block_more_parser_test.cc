// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/block_more_parser.h"

#include "gtest/gtest.h"

#include "test_utils/element_parser_test.h"
#include "webm/id.h"

using webm::BlockMore;
using webm::BlockMoreParser;
using webm::ElementParserTest;
using webm::Id;

namespace {

class BlockMoreParserTest
    : public ElementParserTest<BlockMoreParser, Id::kBlockMore> {};

TEST_F(BlockMoreParserTest, DefaultParse) {
  ParseAndVerify();

  const BlockMore block_more = parser_.value();

  EXPECT_FALSE(block_more.id.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(1), block_more.id.value());

  EXPECT_FALSE(block_more.data.is_present());
  EXPECT_EQ(std::vector<std::uint8_t>{}, block_more.data.value());
}

TEST_F(BlockMoreParserTest, DefaultValues) {
  SetReaderData({
      0xEE,  // ID = 0xEE (BlockAddID).
      0x80,  // Size = 0.

      0xA5,  // ID = 0xA5 (BlockAdditional).
      0x80,  // Size = 0.
  });

  ParseAndVerify();

  const BlockMore block_more = parser_.value();

  EXPECT_TRUE(block_more.id.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(1), block_more.id.value());

  EXPECT_TRUE(block_more.data.is_present());
  EXPECT_EQ(std::vector<std::uint8_t>{}, block_more.data.value());
}

TEST_F(BlockMoreParserTest, CustomValues) {
  SetReaderData({
      0xEE,  // ID = 0xEE (BlockAddID).
      0x81,  // Size = 1.
      0x02,  // Body (value = 2).

      0xA5,  // ID = 0xA5 (BlockAdditional).
      0x81,  // Size = 1.
      0x00,  // Body.
  });

  ParseAndVerify();

  const BlockMore block_more = parser_.value();

  EXPECT_TRUE(block_more.id.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(2), block_more.id.value());

  EXPECT_TRUE(block_more.data.is_present());
  EXPECT_EQ(std::vector<std::uint8_t>{0x00}, block_more.data.value());
}

}  // namespace
