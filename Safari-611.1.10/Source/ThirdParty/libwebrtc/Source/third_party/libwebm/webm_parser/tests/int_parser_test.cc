// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/int_parser.h"

#include "gtest/gtest.h"

#include "test_utils/element_parser_test.h"
#include "webm/status.h"

using webm::ElementParserTest;
using webm::kUnknownElementSize;
using webm::SignedIntParser;
using webm::Status;
using webm::UnsignedIntParser;

namespace {

class UnsignedIntParserTest : public ElementParserTest<UnsignedIntParser> {};

TEST_F(UnsignedIntParserTest, UnsignedInvalidSize) {
  TestInit(9, Status::kInvalidElementSize);
  TestInit(kUnknownElementSize, Status::kInvalidElementSize);
}

TEST_F(UnsignedIntParserTest, UnsignedCustomDefault) {
  ResetParser(1);

  ParseAndVerify();

  EXPECT_EQ(static_cast<std::uint64_t>(1), parser_.value());
}

TEST_F(UnsignedIntParserTest, UnsignedValidInt) {
  ParseAndVerify();
  EXPECT_EQ(static_cast<std::uint64_t>(0), parser_.value());

  SetReaderData({0x01, 0x02, 0x03});
  ParseAndVerify();
  EXPECT_EQ(static_cast<std::uint64_t>(0x010203), parser_.value());

  SetReaderData({0xFF, 0xFF, 0xFF, 0xFF, 0xFF});
  ParseAndVerify();
  EXPECT_EQ(static_cast<std::uint64_t>(0xFFFFFFFFFF), parser_.value());

  SetReaderData({0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0});
  ParseAndVerify();
  EXPECT_EQ(static_cast<std::uint64_t>(0x123456789ABCDEF0), parser_.value());
}

TEST_F(UnsignedIntParserTest, UnsignedIncrementalParse) {
  SetReaderData({0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10});

  IncrementalParseAndVerify();

  EXPECT_EQ(static_cast<std::uint64_t>(0xFEDCBA9876543210), parser_.value());
}

class SignedIntParserTest : public ElementParserTest<SignedIntParser> {};

TEST_F(SignedIntParserTest, SignedInvalidSize) {
  TestInit(9, Status::kInvalidElementSize);
  TestInit(kUnknownElementSize, Status::kInvalidElementSize);
}

TEST_F(SignedIntParserTest, SignedCustomDefault) {
  ResetParser(-1);

  ParseAndVerify();

  EXPECT_EQ(-1, parser_.value());
}

TEST_F(SignedIntParserTest, SignedValidPositiveInt) {
  ParseAndVerify();
  EXPECT_EQ(0, parser_.value());

  SetReaderData({0x7f});
  ParseAndVerify();
  EXPECT_EQ(0x7f, parser_.value());

  SetReaderData({0x12, 0xD6, 0x87});
  ParseAndVerify();
  EXPECT_EQ(1234567, parser_.value());

  SetReaderData({0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0});
  ParseAndVerify();
  EXPECT_EQ(0x123456789ABCDEF0, parser_.value());
}

TEST_F(SignedIntParserTest, SignedValidNegativeInt) {
  SetReaderData({0xFF});
  ParseAndVerify();
  EXPECT_EQ(-1, parser_.value());

  SetReaderData({0xF8, 0xA4, 0x32, 0xEB});
  ParseAndVerify();
  EXPECT_EQ(-123456789, parser_.value());
}

TEST_F(SignedIntParserTest, SignedIncrementalParse) {
  SetReaderData({0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x11});

  IncrementalParseAndVerify();

  EXPECT_EQ(-81985529216486895, parser_.value());
}

}  // namespace
