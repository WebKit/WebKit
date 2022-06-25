// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/float_parser.h"

#include "gtest/gtest.h"

#include "test_utils/element_parser_test.h"
#include "webm/status.h"

using webm::ElementParserTest;
using webm::FloatParser;
using webm::kUnknownElementSize;
using webm::Status;

namespace {

class FloatParserTest : public ElementParserTest<FloatParser> {};

TEST_F(FloatParserTest, InvalidSize) {
  TestInit(1, Status::kInvalidElementSize);
  TestInit(5, Status::kInvalidElementSize);
  TestInit(9, Status::kInvalidElementSize);
  TestInit(kUnknownElementSize, Status::kInvalidElementSize);
}

TEST_F(FloatParserTest, CustomDefault) {
  const double sqrt2 = 1.4142135623730951454746218587388284504413604736328125;
  ResetParser(sqrt2);

  ParseAndVerify();

  EXPECT_EQ(sqrt2, parser_.value());
}

TEST_F(FloatParserTest, ValidFloat) {
  ParseAndVerify();
  EXPECT_EQ(0, parser_.value());

  SetReaderData({0x40, 0xC9, 0x0F, 0xDB});
  ParseAndVerify();
  EXPECT_EQ(6.283185482025146484375, parser_.value());

  SetReaderData({0x40, 0x05, 0xBF, 0x0A, 0x8B, 0x14, 0x57, 0x69});
  ParseAndVerify();
  EXPECT_EQ(2.718281828459045090795598298427648842334747314453125,
            parser_.value());
}

TEST_F(FloatParserTest, IncrementalParse) {
  SetReaderData({0x3F, 0xF9, 0xE3, 0x77, 0x9B, 0x97, 0xF4, 0xA8});

  IncrementalParseAndVerify();

  EXPECT_EQ(1.6180339887498949025257388711906969547271728515625,
            parser_.value());
}

}  // namespace
