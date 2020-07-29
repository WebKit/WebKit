//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// string_utils_unittests:
//   Unit tests for the string utils.
//

#include "string_utils.h"

#include <gtest/gtest.h>

using namespace angle;

namespace
{

// Basic SplitString tests
TEST(StringUtilsTest, SplitString_Basics)
{
    std::vector<std::string> r;

    r = SplitString(std::string(), ",:;", KEEP_WHITESPACE, SPLIT_WANT_ALL);
    EXPECT_TRUE(r.empty());

    // Empty separator list
    r = SplitString("hello, world", "", KEEP_WHITESPACE, SPLIT_WANT_ALL);
    ASSERT_EQ(1u, r.size());
    EXPECT_EQ("hello, world", r[0]);

    // Should split on any of the separators.
    r = SplitString("::,,;;", ",:;", KEEP_WHITESPACE, SPLIT_WANT_ALL);
    ASSERT_EQ(7u, r.size());
    for (auto str : r)
        ASSERT_TRUE(str.empty());

    r = SplitString("red, green; blue:", ",:;", TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);
    ASSERT_EQ(3u, r.size());
    EXPECT_EQ("red", r[0]);
    EXPECT_EQ("green", r[1]);
    EXPECT_EQ("blue", r[2]);

    // Want to split a string along whitespace sequences.
    r = SplitString("  red green   \tblue\n", " \t\n", TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);
    ASSERT_EQ(3u, r.size());
    EXPECT_EQ("red", r[0]);
    EXPECT_EQ("green", r[1]);
    EXPECT_EQ("blue", r[2]);

    // Weird case of splitting on spaces but not trimming.
    r = SplitString(" red ", " ", TRIM_WHITESPACE, SPLIT_WANT_ALL);
    ASSERT_EQ(3u, r.size());
    EXPECT_EQ("", r[0]);  // Before the first space.
    EXPECT_EQ("red", r[1]);
    EXPECT_EQ("", r[2]);  // After the last space.
}

// Check different whitespace and result types for SplitString
TEST(StringUtilsTest, SplitString_WhitespaceAndResultType)
{
    std::vector<std::string> r;

    // Empty input handling.
    r = SplitString(std::string(), ",", KEEP_WHITESPACE, SPLIT_WANT_ALL);
    EXPECT_TRUE(r.empty());
    r = SplitString(std::string(), ",", KEEP_WHITESPACE, SPLIT_WANT_NONEMPTY);
    EXPECT_TRUE(r.empty());

    // Input string is space and we're trimming.
    r = SplitString(" ", ",", TRIM_WHITESPACE, SPLIT_WANT_ALL);
    ASSERT_EQ(1u, r.size());
    EXPECT_EQ("", r[0]);
    r = SplitString(" ", ",", TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);
    EXPECT_TRUE(r.empty());

    // Test all 4 combinations of flags on ", ,".
    r = SplitString(", ,", ",", KEEP_WHITESPACE, SPLIT_WANT_ALL);
    ASSERT_EQ(3u, r.size());
    EXPECT_EQ("", r[0]);
    EXPECT_EQ(" ", r[1]);
    EXPECT_EQ("", r[2]);
    r = SplitString(", ,", ",", KEEP_WHITESPACE, SPLIT_WANT_NONEMPTY);
    ASSERT_EQ(1u, r.size());
    ASSERT_EQ(" ", r[0]);
    r = SplitString(", ,", ",", TRIM_WHITESPACE, SPLIT_WANT_ALL);
    ASSERT_EQ(3u, r.size());
    EXPECT_EQ("", r[0]);
    EXPECT_EQ("", r[1]);
    EXPECT_EQ("", r[2]);
    r = SplitString(", ,", ",", TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);
    ASSERT_TRUE(r.empty());
}

// Tests for TrimString
TEST(StringUtilsTest, TrimString)
{
    // Basic tests
    EXPECT_EQ("a", TrimString("a", kWhitespaceASCII));
    EXPECT_EQ("a", TrimString(" a", kWhitespaceASCII));
    EXPECT_EQ("a", TrimString("a ", kWhitespaceASCII));
    EXPECT_EQ("a", TrimString(" a ", kWhitespaceASCII));

    // Tests with empty strings
    EXPECT_EQ("", TrimString("", kWhitespaceASCII));
    EXPECT_EQ("", TrimString(" \n\r\t", kWhitespaceASCII));
    EXPECT_EQ(" foo ", TrimString(" foo ", ""));

    // Tests it doesn't removes characters in the middle
    EXPECT_EQ("foo bar", TrimString(" foo bar ", kWhitespaceASCII));

    // Test with non-whitespace trimChars
    EXPECT_EQ(" ", TrimString("foo bar", "abcdefghijklmnopqrstuvwxyz"));
}

// Basic functionality tests for HexStringToUInt
TEST(StringUtilsTest, HexStringToUIntBasic)
{
    unsigned int uintValue;

    std::string emptyString;
    ASSERT_FALSE(HexStringToUInt(emptyString, &uintValue));

    std::string testStringA("0xBADF00D");
    ASSERT_TRUE(HexStringToUInt(testStringA, &uintValue));
    EXPECT_EQ(0xBADF00Du, uintValue);

    std::string testStringB("0xBADFOOD");
    EXPECT_FALSE(HexStringToUInt(testStringB, &uintValue));

    std::string testStringC("BADF00D");
    EXPECT_TRUE(HexStringToUInt(testStringC, &uintValue));
    EXPECT_EQ(0xBADF00Du, uintValue);

    std::string testStringD("0x BADF00D");
    EXPECT_FALSE(HexStringToUInt(testStringD, &uintValue));
}

// Note: ReadFileToString is harder to test

class BeginsWithTest : public testing::Test
{
  public:
    BeginsWithTest() : mMode(TestMode::CHAR_ARRAY) {}

    enum class TestMode
    {
        CHAR_ARRAY,
        STRING_AND_CHAR_ARRAY,
        STRING
    };

    void setMode(TestMode mode) { mMode = mode; }

    bool runBeginsWith(const char *str, const char *prefix)
    {
        if (mMode == TestMode::CHAR_ARRAY)
        {
            return BeginsWith(str, prefix);
        }
        if (mMode == TestMode::STRING_AND_CHAR_ARRAY)
        {
            return BeginsWith(std::string(str), prefix);
        }
        return BeginsWith(std::string(str), std::string(prefix));
    }

    void runTest()
    {
        ASSERT_FALSE(runBeginsWith("foo", "bar"));
        ASSERT_FALSE(runBeginsWith("", "foo"));
        ASSERT_FALSE(runBeginsWith("foo", "foobar"));

        ASSERT_TRUE(runBeginsWith("foobar", "foo"));
        ASSERT_TRUE(runBeginsWith("foobar", ""));
        ASSERT_TRUE(runBeginsWith("foo", "foo"));
        ASSERT_TRUE(runBeginsWith("", ""));
    }

  private:
    TestMode mMode;
};

// Test that BeginsWith works correctly for const char * arguments.
TEST_F(BeginsWithTest, CharArrays)
{
    setMode(TestMode::CHAR_ARRAY);
    runTest();
}

// Test that BeginsWith works correctly for std::string and const char * arguments.
TEST_F(BeginsWithTest, StringAndCharArray)
{
    setMode(TestMode::STRING_AND_CHAR_ARRAY);
    runTest();
}

// Test that BeginsWith works correctly for std::string arguments.
TEST_F(BeginsWithTest, Strings)
{
    setMode(TestMode::STRING);
    runTest();
}

class EndsWithTest : public testing::Test
{
  public:
    EndsWithTest() : mMode(TestMode::CHAR_ARRAY) {}

    enum class TestMode
    {
        CHAR_ARRAY,
        STRING_AND_CHAR_ARRAY,
        STRING
    };

    void setMode(TestMode mode) { mMode = mode; }

    bool runEndsWith(const char *str, const char *suffix)
    {
        if (mMode == TestMode::CHAR_ARRAY)
        {
            return EndsWith(str, suffix);
        }
        if (mMode == TestMode::STRING_AND_CHAR_ARRAY)
        {
            return EndsWith(std::string(str), suffix);
        }
        return EndsWith(std::string(str), std::string(suffix));
    }

    void runTest()
    {
        ASSERT_FALSE(EndsWith("foo", "bar"));
        ASSERT_FALSE(EndsWith("", "bar"));
        ASSERT_FALSE(EndsWith("foo", "foobar"));

        ASSERT_TRUE(EndsWith("foobar", "bar"));
        ASSERT_TRUE(EndsWith("foobar", ""));
        ASSERT_TRUE(EndsWith("bar", "bar"));
        ASSERT_TRUE(EndsWith("", ""));
    }

  private:
    TestMode mMode;
};

// Test that EndsWith works correctly for const char * arguments.
TEST_F(EndsWithTest, CharArrays)
{
    setMode(TestMode::CHAR_ARRAY);
    runTest();
}

// Test that EndsWith works correctly for std::string and const char * arguments.
TEST_F(EndsWithTest, StringAndCharArray)
{
    setMode(TestMode::STRING_AND_CHAR_ARRAY);
    runTest();
}

// Test that EndsWith works correctly for std::string arguments.
TEST_F(EndsWithTest, Strings)
{
    setMode(TestMode::STRING);
    runTest();
}

}  // anonymous namespace
