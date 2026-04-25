// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/bool_parser.h"

#include "gtest/gtest.h"

#include "test_utils/element_parser_test.h"
#include "webm/status.h"

using webm::BoolParser;
using webm::ElementParserTest;
using webm::kUnknownElementSize;
using webm::Status;

namespace {

class BoolParserTest : public ElementParserTest<BoolParser> {};

TEST_F(BoolParserTest, InvalidSize) {
  TestInit(9, Status::kInvalidElementSize);
  TestInit(kUnknownElementSize, Status::kInvalidElementSize);
}

TEST_F(BoolParserTest, InvalidValue) {
  SetReaderData({0x02});
  ParseAndExpectResult(Status::kInvalidElementValue);

  SetReaderData({0xFF, 0xFF});
  ParseAndExpectResult(Status::kInvalidElementValue);
}

TEST_F(BoolParserTest, CustomDefault) {
  ResetParser(true);

  ParseAndVerify();

  EXPECT_EQ(true, parser_.value());
}

TEST_F(BoolParserTest, ValidBool) {
  ParseAndVerify();
  EXPECT_EQ(false, parser_.value());

  SetReaderData({0x00, 0x00, 0x01});
  ParseAndVerify();
  EXPECT_EQ(true, parser_.value());

  SetReaderData({0x00, 0x00, 0x00, 0x00, 0x00});
  ParseAndVerify();
  EXPECT_EQ(false, parser_.value());

  SetReaderData({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01});
  ParseAndVerify();
  EXPECT_EQ(true, parser_.value());
}

TEST_F(BoolParserTest, IncrementalParse) {
  SetReaderData({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});

  IncrementalParseAndVerify();

  EXPECT_EQ(false, parser_.value());
}

}  // namespace
