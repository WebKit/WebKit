// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/content_encoding_parser.h"

#include "gtest/gtest.h"

#include "test_utils/element_parser_test.h"
#include "webm/id.h"

using webm::ContentEncAlgo;
using webm::ContentEncoding;
using webm::ContentEncodingParser;
using webm::ContentEncodingType;
using webm::ContentEncryption;
using webm::ElementParserTest;
using webm::Id;

namespace {

class ContentEncodingParserTest
    : public ElementParserTest<ContentEncodingParser, Id::kContentEncoding> {};

TEST_F(ContentEncodingParserTest, DefaultParse) {
  ParseAndVerify();

  const ContentEncoding content_encoding = parser_.value();

  EXPECT_FALSE(content_encoding.order.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), content_encoding.order.value());

  EXPECT_FALSE(content_encoding.scope.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(1), content_encoding.scope.value());

  EXPECT_FALSE(content_encoding.type.is_present());
  EXPECT_EQ(ContentEncodingType::kCompression, content_encoding.type.value());

  EXPECT_FALSE(content_encoding.encryption.is_present());
  EXPECT_EQ(ContentEncryption{}, content_encoding.encryption.value());
}

TEST_F(ContentEncodingParserTest, DefaultValues) {
  SetReaderData({
      0x50, 0x31,  // ID = 0x5031 (ContentEncodingOrder).
      0x80,  // Size = 0.

      0x50, 0x32,  // ID = 0x5032 (ContentEncodingScope).
      0x80,  // Size = 0.

      0x50, 0x33,  // ID = 0x5033 (ContentEncodingType).
      0x80,  // Size = 0.

      0x50, 0x35,  // ID = 0x5035 (ContentEncryption).
      0x80,  // Size = 0.
  });

  ParseAndVerify();

  const ContentEncoding content_encoding = parser_.value();

  EXPECT_TRUE(content_encoding.order.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), content_encoding.order.value());

  EXPECT_TRUE(content_encoding.scope.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(1), content_encoding.scope.value());

  EXPECT_TRUE(content_encoding.type.is_present());
  EXPECT_EQ(ContentEncodingType::kCompression, content_encoding.type.value());

  EXPECT_TRUE(content_encoding.encryption.is_present());
  EXPECT_EQ(ContentEncryption{}, content_encoding.encryption.value());
}

TEST_F(ContentEncodingParserTest, CustomValues) {
  SetReaderData({
      0x50, 0x31,  // ID = 0x5031 (ContentEncodingOrder).
      0x81,  // Size = 1.
      0x01,  // Body (value = 1).

      0x50, 0x32,  // ID = 0x5032 (ContentEncodingScope).
      0x81,  // Size = 1.
      0x02,  // Body (value = 2).

      0x50, 0x33,  // ID = 0x5033 (ContentEncodingType).
      0x81,  // Size = 1.
      0x01,  // Body (value = encryption).

      0x50, 0x35,  // ID = 0x5035 (ContentEncryption).
      0x84,  // Size = 4.

      0x47, 0xE1,  //   ID = 0x47E1 (ContentEncAlgo).
      0x81,  //   Size = 1.
      0x05,  //   Body (value = AES).
  });

  ParseAndVerify();

  const ContentEncoding content_encoding = parser_.value();

  EXPECT_TRUE(content_encoding.order.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(1), content_encoding.order.value());

  EXPECT_TRUE(content_encoding.scope.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(2), content_encoding.scope.value());

  EXPECT_TRUE(content_encoding.type.is_present());
  EXPECT_EQ(ContentEncodingType::kEncryption, content_encoding.type.value());

  ContentEncryption expected;
  expected.algorithm.Set(ContentEncAlgo::kAes, true);

  EXPECT_TRUE(content_encoding.encryption.is_present());
  EXPECT_EQ(expected, content_encoding.encryption.value());
}

}  // namespace
