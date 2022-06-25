// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/virtual_block_parser.h"

#include "gtest/gtest.h"

#include "test_utils/element_parser_test.h"
#include "webm/id.h"
#include "webm/status.h"

using webm::ElementParserTest;
using webm::Id;
using webm::kUnknownElementSize;
using webm::Status;
using webm::VirtualBlock;
using webm::VirtualBlockParser;

namespace {

class VirtualBlockParserTest
    : public ElementParserTest<VirtualBlockParser, Id::kBlockVirtual> {};

TEST_F(VirtualBlockParserTest, InvalidSize) {
  TestInit(3, Status::kInvalidElementSize);
  TestInit(kUnknownElementSize, Status::kInvalidElementSize);
}

TEST_F(VirtualBlockParserTest, InvalidBlock) {
  SetReaderData({
      0x40, 0x01,  // Track number = 1.
      0x00, 0x00,  // Timecode = 0.
      0x00,  // Flags.
  });

  // Initialize with 1 byte short.
  ParseAndExpectResult(Status::kInvalidElementValue, reader_.size() - 1);
}

TEST_F(VirtualBlockParserTest, ValidBlock) {
  SetReaderData({
      0x81,  // Track number = 1.
      0x12, 0x34,  // Timecode = 4660.
      0x00,  // Flags.
  });

  ParseAndVerify();

  const VirtualBlock virtual_block = parser_.value();

  EXPECT_EQ(static_cast<std::uint64_t>(1), virtual_block.track_number);
  EXPECT_EQ(0x1234, virtual_block.timecode);
}

TEST_F(VirtualBlockParserTest, IncrementalParse) {
  SetReaderData({
      0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,  // Track number = 2.
      0xFF, 0xFE,  // Timecode = -2.
      0x00,  // Flags.
  });

  IncrementalParseAndVerify();

  const VirtualBlock virtual_block = parser_.value();

  EXPECT_EQ(static_cast<std::uint64_t>(2), virtual_block.track_number);
  EXPECT_EQ(-2, virtual_block.timecode);
}

}  // namespace
