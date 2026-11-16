// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/block_additions_parser.h"

#include "gtest/gtest.h"

#include "test_utils/element_parser_test.h"
#include "webm/id.h"

using webm::BlockAdditions;
using webm::BlockAdditionsParser;
using webm::BlockMore;
using webm::ElementParserTest;
using webm::Id;

namespace {

class BlockAdditionsParserTest
    : public ElementParserTest<BlockAdditionsParser, Id::kBlockAdditions> {};

TEST_F(BlockAdditionsParserTest, DefaultParse) {
  ParseAndVerify();

  const BlockAdditions block_additions = parser_.value();

  const std::size_t kExpectedBlockMoresSize = 0;
  EXPECT_EQ(kExpectedBlockMoresSize, block_additions.block_mores.size());
}

TEST_F(BlockAdditionsParserTest, DefaultValues) {
  SetReaderData({
      0xA6,  // ID = 0xA6 (BlockMore).
      0x80,  // Size = 0.
  });

  ParseAndVerify();

  const BlockAdditions block_additions = parser_.value();

  ASSERT_EQ(static_cast<std::size_t>(1), block_additions.block_mores.size());
  EXPECT_TRUE(block_additions.block_mores[0].is_present());
  EXPECT_EQ(BlockMore{}, block_additions.block_mores[0].value());
}

TEST_F(BlockAdditionsParserTest, CustomValues) {
  SetReaderData({
      0xA6,  // ID = 0xA6 (BlockMore).
      0x83,  // Size = 3.

      0xEE,  //   ID = 0xEE (BlockAddID).
      0x81,  //   Size = 1.
      0x02,  //   Body (value = 2).

      0xA6,  // ID = 0xA6 (BlockMore).
      0x83,  // Size = 3.

      0xEE,  //   ID = 0xEE (BlockAddID).
      0x81,  //   Size = 1.
      0x03,  //   Body (value = 3).
  });

  ParseAndVerify();

  const BlockAdditions block_additions = parser_.value();

  BlockMore expected;

  ASSERT_EQ(static_cast<std::size_t>(2), block_additions.block_mores.size());
  expected.id.Set(2, true);
  EXPECT_TRUE(block_additions.block_mores[0].is_present());
  EXPECT_EQ(expected, block_additions.block_mores[0].value());
  expected.id.Set(3, true);
  EXPECT_TRUE(block_additions.block_mores[1].is_present());
  EXPECT_EQ(expected, block_additions.block_mores[1].value());
}

}  // namespace
