// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/size_parser.h"

#include "gtest/gtest.h"

#include "test_utils/parser_test.h"
#include "webm/status.h"

using webm::ParserTest;
using webm::SizeParser;
using webm::Status;

namespace {

class SizeParserTest : public ParserTest<SizeParser> {};

TEST_F(SizeParserTest, InvalidSize) {
  SetReaderData({0x00});
  ParseAndExpectResult(Status::kInvalidElementSize);
}

TEST_F(SizeParserTest, EarlyEndOfFile) {
  SetReaderData({0x01});
  ParseAndExpectResult(Status::kEndOfFile);
}

TEST_F(SizeParserTest, ValidSize) {
  SetReaderData({0x80});
  ParseAndVerify();
  EXPECT_EQ(static_cast<std::uint64_t>(0), parser_.size());

  ResetParser();
  SetReaderData({0x01, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE});
  ParseAndVerify();
  EXPECT_EQ(static_cast<std::uint64_t>(0x123456789ABCDE), parser_.size());
}

TEST_F(SizeParserTest, UnknownSize) {
  SetReaderData({0xFF});
  ParseAndVerify();
  EXPECT_EQ(static_cast<std::uint64_t>(0xFFFFFFFFFFFFFFFF), parser_.size());

  ResetParser();
  SetReaderData({0x1F, 0xFF, 0xFF, 0xFF});
  ParseAndVerify();
  EXPECT_EQ(static_cast<std::uint64_t>(0xFFFFFFFFFFFFFFFF), parser_.size());
}

TEST_F(SizeParserTest, IncrementalParse) {
  SetReaderData({0x11, 0x23, 0x45, 0x67});
  IncrementalParseAndVerify();
  EXPECT_EQ(static_cast<std::uint64_t>(0x01234567), parser_.size());
}

}  // namespace
