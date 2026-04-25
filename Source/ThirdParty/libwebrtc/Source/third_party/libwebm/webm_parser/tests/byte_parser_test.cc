// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/byte_parser.h"

#include "gtest/gtest.h"

#include "test_utils/element_parser_test.h"
#include "webm/status.h"

using webm::BinaryParser;
using webm::ElementParserTest;
using webm::kUnknownElementSize;
using webm::Status;
using webm::StringParser;

namespace {

class StringParserTest : public ElementParserTest<StringParser> {};

TEST_F(StringParserTest, StringInvalidSize) {
  TestInit(kUnknownElementSize, Status::kInvalidElementSize);
}

TEST_F(StringParserTest, StringCustomDefault) {
  ResetParser("foobar");

  ParseAndVerify();

  EXPECT_EQ("foobar", parser_.value());
}

TEST_F(StringParserTest, StringValidValue) {
  ParseAndVerify();
  EXPECT_EQ("", parser_.value());

  SetReaderData({'!'});
  ParseAndVerify();
  EXPECT_EQ("!", parser_.value());

  SetReaderData({'H', 'e', 'l', 'l', 'o', ',', ' ', 'w', 'o', 'r', 'l', 'd'});
  ParseAndVerify();
  EXPECT_EQ("Hello, world", parser_.value());
}

TEST_F(StringParserTest, StringTrailingNulCharacters) {
  // The trailing NUL characters should be trimmed.
  SetReaderData({'H', 'i', '\0', '\0'});
  ParseAndVerify();
  EXPECT_EQ("Hi", parser_.value());

  SetReaderData({'\0'});
  ParseAndVerify();
  EXPECT_EQ("", parser_.value());
}

TEST_F(StringParserTest, StringIncrementalParse) {
  SetReaderData({'M', 'a', 't', 'r', 'o', 's', 'k', 'a'});

  IncrementalParseAndVerify();

  EXPECT_EQ("Matroska", parser_.value());
}

class BinaryParserTest : public ElementParserTest<BinaryParser> {};

TEST_F(BinaryParserTest, BinaryInvalidSize) {
  TestInit(kUnknownElementSize, Status::kInvalidElementSize);
}

TEST_F(BinaryParserTest, BinaryCustomDefault) {
  std::vector<std::uint8_t> expected = {0x00, 0x02, 0x04, 0x06, 0x08};
  ResetParser(expected);

  ParseAndVerify();

  EXPECT_EQ(expected, parser_.value());
}

TEST_F(BinaryParserTest, BinaryValidValue) {
  std::vector<std::uint8_t> expected;

  ParseAndVerify();
  EXPECT_EQ(expected, parser_.value());

  expected = {0x00};
  SetReaderData(expected);
  ParseAndVerify();
  EXPECT_EQ(expected, parser_.value());

  expected = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09};
  SetReaderData(expected);
  ParseAndVerify();
  EXPECT_EQ(expected, parser_.value());

  // Unlike StringParser, the BinaryParser should not trim trailing 0-bytes.
  expected = {'H', 'i', '\0', '\0'};
  SetReaderData(expected);
  ParseAndVerify();
  EXPECT_EQ(expected, parser_.value());
}

TEST_F(BinaryParserTest, BinaryIncrementalParse) {
  const std::vector<std::uint8_t> expected = {
      0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F};
  SetReaderData(expected);

  IncrementalParseAndVerify();

  EXPECT_EQ(expected, parser_.value());
}

}  // namespace
