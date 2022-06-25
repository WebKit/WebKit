// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/block_header_parser.h"

#include "gtest/gtest.h"

#include "test_utils/parser_test.h"
#include "webm/status.h"

using webm::BlockHeader;
using webm::BlockHeaderParser;
using webm::ParserTest;

namespace {

class BlockHeaderParserTest : public ParserTest<BlockHeaderParser> {};

TEST_F(BlockHeaderParserTest, ValidBlock) {
  SetReaderData({
      0x81,  // Track number = 1.
      0x12, 0x34,  // Timecode = 4660.
      0x00,  // Flags.
  });

  ParseAndVerify();

  const BlockHeader& block_header = parser_.value();

  EXPECT_EQ(static_cast<std::uint64_t>(1), block_header.track_number);
  EXPECT_EQ(0x1234, block_header.timecode);
  EXPECT_EQ(0x00, block_header.flags);
}

TEST_F(BlockHeaderParserTest, IncrementalParse) {
  SetReaderData({
      0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,  // Track number = 2.
      0xFF, 0xFE,  // Timecode = -2.
      0xFF,  // Flags.
  });

  IncrementalParseAndVerify();

  const BlockHeader& block_header = parser_.value();

  EXPECT_EQ(static_cast<std::uint64_t>(2), block_header.track_number);
  EXPECT_EQ(-2, block_header.timecode);
  EXPECT_EQ(0xFF, block_header.flags);
}

}  // namespace
