/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/string_encode.h"

#include <string.h>

#include <sstream>  // no-presubmit-check TODO(webrtc:8982)

#include "test/gtest.h"

namespace rtc {

class HexEncodeTest : public ::testing::Test {
 public:
  HexEncodeTest() : dec_res_(0) {
    for (size_t i = 0; i < sizeof(data_); ++i) {
      data_[i] = (i + 128) & 0xff;
    }
    memset(decoded_, 0x7f, sizeof(decoded_));
  }

  char data_[10];
  char decoded_[11];
  size_t dec_res_;
};

// Test that we can convert to/from hex with no delimiter.
TEST_F(HexEncodeTest, TestWithNoDelimiter) {
  std::string encoded = hex_encode(data_, sizeof(data_));
  EXPECT_EQ("80818283848586878889", encoded);
  dec_res_ =
      hex_decode(decoded_, sizeof(decoded_), encoded.data(), encoded.size());
  ASSERT_EQ(sizeof(data_), dec_res_);
  ASSERT_EQ(0, memcmp(data_, decoded_, dec_res_));
}

// Test that we can convert to/from hex with a colon delimiter.
TEST_F(HexEncodeTest, TestWithDelimiter) {
  std::string encoded = hex_encode_with_delimiter(data_, sizeof(data_), ':');
  EXPECT_EQ("80:81:82:83:84:85:86:87:88:89", encoded);
  dec_res_ = hex_decode_with_delimiter(decoded_, sizeof(decoded_),
                                       encoded.data(), encoded.size(), ':');
  ASSERT_EQ(sizeof(data_), dec_res_);
  ASSERT_EQ(0, memcmp(data_, decoded_, dec_res_));
}

// Test that encoding with one delimiter and decoding with another fails.
TEST_F(HexEncodeTest, TestWithWrongDelimiter) {
  std::string encoded = hex_encode_with_delimiter(data_, sizeof(data_), ':');
  dec_res_ = hex_decode_with_delimiter(decoded_, sizeof(decoded_),
                                       encoded.data(), encoded.size(), '/');
  ASSERT_EQ(0U, dec_res_);
}

// Test that encoding without a delimiter and decoding with one fails.
TEST_F(HexEncodeTest, TestExpectedDelimiter) {
  std::string encoded = hex_encode(data_, sizeof(data_));
  EXPECT_EQ(sizeof(data_) * 2, encoded.size());
  dec_res_ = hex_decode_with_delimiter(decoded_, sizeof(decoded_),
                                       encoded.data(), encoded.size(), ':');
  ASSERT_EQ(0U, dec_res_);
}

// Test that encoding with a delimiter and decoding without one fails.
TEST_F(HexEncodeTest, TestExpectedNoDelimiter) {
  std::string encoded = hex_encode_with_delimiter(data_, sizeof(data_), ':');
  EXPECT_EQ(sizeof(data_) * 3 - 1, encoded.size());
  dec_res_ =
      hex_decode(decoded_, sizeof(decoded_), encoded.data(), encoded.size());
  ASSERT_EQ(0U, dec_res_);
}

// Test that we handle a zero-length buffer with no delimiter.
TEST_F(HexEncodeTest, TestZeroLengthNoDelimiter) {
  std::string encoded = hex_encode("", 0);
  EXPECT_TRUE(encoded.empty());
  dec_res_ =
      hex_decode(decoded_, sizeof(decoded_), encoded.data(), encoded.size());
  ASSERT_EQ(0U, dec_res_);
}

// Test that we handle a zero-length buffer with a delimiter.
TEST_F(HexEncodeTest, TestZeroLengthWithDelimiter) {
  std::string encoded = hex_encode_with_delimiter("", 0, ':');
  EXPECT_TRUE(encoded.empty());
  dec_res_ = hex_decode_with_delimiter(decoded_, sizeof(decoded_),
                                       encoded.data(), encoded.size(), ':');
  ASSERT_EQ(0U, dec_res_);
}

// Test that decoding into a too-small output buffer fails.
TEST_F(HexEncodeTest, TestDecodeTooShort) {
  dec_res_ = hex_decode_with_delimiter(decoded_, 4, "0123456789", 10, 0);
  ASSERT_EQ(0U, dec_res_);
  ASSERT_EQ(0x7f, decoded_[4]);
}

// Test that decoding non-hex data fails.
TEST_F(HexEncodeTest, TestDecodeBogusData) {
  dec_res_ =
      hex_decode_with_delimiter(decoded_, sizeof(decoded_), "axyz", 4, 0);
  ASSERT_EQ(0U, dec_res_);
}

// Test that decoding an odd number of hex characters fails.
TEST_F(HexEncodeTest, TestDecodeOddHexDigits) {
  dec_res_ = hex_decode_with_delimiter(decoded_, sizeof(decoded_), "012", 3, 0);
  ASSERT_EQ(0U, dec_res_);
}

// Test that decoding a string with too many delimiters fails.
TEST_F(HexEncodeTest, TestDecodeWithDelimiterTooManyDelimiters) {
  dec_res_ = hex_decode_with_delimiter(decoded_, 4, "01::23::45::67", 14, ':');
  ASSERT_EQ(0U, dec_res_);
}

// Test that decoding a string with a leading delimiter fails.
TEST_F(HexEncodeTest, TestDecodeWithDelimiterLeadingDelimiter) {
  dec_res_ = hex_decode_with_delimiter(decoded_, 4, ":01:23:45:67", 12, ':');
  ASSERT_EQ(0U, dec_res_);
}

// Test that decoding a string with a trailing delimiter fails.
TEST_F(HexEncodeTest, TestDecodeWithDelimiterTrailingDelimiter) {
  dec_res_ = hex_decode_with_delimiter(decoded_, 4, "01:23:45:67:", 12, ':');
  ASSERT_EQ(0U, dec_res_);
}

// Tests counting substrings.
TEST(TokenizeTest, CountSubstrings) {
  std::vector<std::string> fields;

  EXPECT_EQ(5ul, tokenize("one two three four five", ' ', &fields));
  fields.clear();
  EXPECT_EQ(1ul, tokenize("one", ' ', &fields));

  // Extra spaces should be ignored.
  fields.clear();
  EXPECT_EQ(5ul, tokenize("  one    two  three    four five  ", ' ', &fields));
  fields.clear();
  EXPECT_EQ(1ul, tokenize("  one  ", ' ', &fields));
  fields.clear();
  EXPECT_EQ(0ul, tokenize(" ", ' ', &fields));
}

// Tests comparing substrings.
TEST(TokenizeTest, CompareSubstrings) {
  std::vector<std::string> fields;

  tokenize("find middle one", ' ', &fields);
  ASSERT_EQ(3ul, fields.size());
  ASSERT_STREQ("middle", fields.at(1).c_str());
  fields.clear();

  // Extra spaces should be ignored.
  tokenize("  find   middle  one    ", ' ', &fields);
  ASSERT_EQ(3ul, fields.size());
  ASSERT_STREQ("middle", fields.at(1).c_str());
  fields.clear();
  tokenize(" ", ' ', &fields);
  ASSERT_EQ(0ul, fields.size());
}

TEST(TokenizeFirstTest, NoLeadingSpaces) {
  std::string token;
  std::string rest;

  ASSERT_TRUE(tokenize_first("A &*${}", ' ', &token, &rest));
  ASSERT_STREQ("A", token.c_str());
  ASSERT_STREQ("&*${}", rest.c_str());

  ASSERT_TRUE(tokenize_first("A B& *${}", ' ', &token, &rest));
  ASSERT_STREQ("A", token.c_str());
  ASSERT_STREQ("B& *${}", rest.c_str());

  ASSERT_TRUE(tokenize_first("A    B& *${}    ", ' ', &token, &rest));
  ASSERT_STREQ("A", token.c_str());
  ASSERT_STREQ("B& *${}    ", rest.c_str());
}

TEST(TokenizeFirstTest, LeadingSpaces) {
  std::string token;
  std::string rest;

  ASSERT_TRUE(tokenize_first("     A B C", ' ', &token, &rest));
  ASSERT_STREQ("", token.c_str());
  ASSERT_STREQ("A B C", rest.c_str());

  ASSERT_TRUE(tokenize_first("     A    B   C    ", ' ', &token, &rest));
  ASSERT_STREQ("", token.c_str());
  ASSERT_STREQ("A    B   C    ", rest.c_str());
}

TEST(TokenizeFirstTest, SingleToken) {
  std::string token;
  std::string rest;

  // In the case where we cannot find delimiter the whole string is a token.
  ASSERT_FALSE(tokenize_first("ABC", ' ', &token, &rest));

  ASSERT_TRUE(tokenize_first("ABC    ", ' ', &token, &rest));
  ASSERT_STREQ("ABC", token.c_str());
  ASSERT_STREQ("", rest.c_str());

  ASSERT_TRUE(tokenize_first("    ABC    ", ' ', &token, &rest));
  ASSERT_STREQ("", token.c_str());
  ASSERT_STREQ("ABC    ", rest.c_str());
}

// Tests counting substrings.
TEST(SplitTest, CountSubstrings) {
  std::vector<std::string> fields;

  EXPECT_EQ(5ul, split("one,two,three,four,five", ',', &fields));
  fields.clear();
  EXPECT_EQ(1ul, split("one", ',', &fields));

  // Empty fields between commas count.
  fields.clear();
  EXPECT_EQ(5ul, split("one,,three,four,five", ',', &fields));
  fields.clear();
  EXPECT_EQ(3ul, split(",three,", ',', &fields));
  fields.clear();
  EXPECT_EQ(1ul, split("", ',', &fields));
}

// Tests comparing substrings.
TEST(SplitTest, CompareSubstrings) {
  std::vector<std::string> fields;

  split("find,middle,one", ',', &fields);
  ASSERT_EQ(3ul, fields.size());
  ASSERT_STREQ("middle", fields.at(1).c_str());
  fields.clear();

  // Empty fields between commas count.
  split("find,,middle,one", ',', &fields);
  ASSERT_EQ(4ul, fields.size());
  ASSERT_STREQ("middle", fields.at(2).c_str());
  fields.clear();
  split("", ',', &fields);
  ASSERT_EQ(1ul, fields.size());
  ASSERT_STREQ("", fields.at(0).c_str());
}

TEST(SplitTest, EmptyTokens) {
  std::vector<std::string> fields;
  EXPECT_EQ(3ul, split("a.b.c", '.', &fields));
  EXPECT_EQ("a", fields[0]);
  EXPECT_EQ("b", fields[1]);
  EXPECT_EQ("c", fields[2]);

  EXPECT_EQ(3ul, split("..c", '.', &fields));
  EXPECT_TRUE(fields[0].empty());
  EXPECT_TRUE(fields[1].empty());
  EXPECT_EQ("c", fields[2]);

  EXPECT_EQ(1ul, split("", '.', &fields));
  EXPECT_TRUE(fields[0].empty());
}

TEST(ToString, SanityCheck) {
  EXPECT_EQ(ToString(true), "true");
  EXPECT_EQ(ToString(false), "false");

  const char* c = "message";
  EXPECT_EQ(ToString(c), c);
  EXPECT_EQ(ToString(std::string(c)), c);

  EXPECT_EQ(ToString(short{-123}), "-123");
  EXPECT_EQ(ToString((unsigned short)123), "123");
  EXPECT_EQ(ToString(int{-123}), "-123");
  EXPECT_EQ(ToString((unsigned int)123), "123");
  EXPECT_EQ(ToString((long int)-123), "-123");
  EXPECT_EQ(ToString((unsigned long int)123), "123");
  EXPECT_EQ(ToString((long long int)-123), "-123");
  EXPECT_EQ(ToString((unsigned long long int)123), "123");

  int i = 10;
  int* p = &i;
  std::ostringstream s;  // no-presubmit-check TODO(webrtc:8982)
  s << p;
  EXPECT_EQ(s.str(), ToString(p));

  EXPECT_EQ(ToString(0.5), "0.5");
}

template <typename T>
void ParsesTo(std::string s, T t) {
  T value;
  EXPECT_TRUE(FromString(s, &value));
  EXPECT_EQ(value, t);
}

TEST(FromString, DecodeValid) {
  ParsesTo("true", true);
  ParsesTo("false", false);

  ParsesTo("105", 105);
  ParsesTo("0.25", 0.25);
}

template <typename T>
void FailsToParse(std::string s) {
  T value;
  EXPECT_FALSE(FromString(s, &value)) << "[" << s << "]";
}

TEST(FromString, DecodeInvalid) {
  FailsToParse<bool>("True");
  FailsToParse<bool>("0");
  FailsToParse<bool>("yes");

  FailsToParse<int>("0.5");
  FailsToParse<int>("XIV");
  FailsToParse<double>("");
  FailsToParse<double>("  ");
  FailsToParse<int>("1 2");
}

template <typename T>
void RoundTrip(T t) {
  std::string s = ToString(t);
  T value;
  EXPECT_TRUE(FromString(s, &value));
  EXPECT_EQ(value, t);
}

TEST(FromString, RoundTrip) {
  RoundTrip<int>(123);
  RoundTrip(false);
  RoundTrip(true);
  RoundTrip(0.5);
  RoundTrip(-15l);
}

}  // namespace rtc
