// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ImmutableString_test.cpp:
//   Tests for ImmutableString and ImmutableStringBuilder.

#include "compiler/translator/ImmutableString.h"
#include "compiler/translator/ImmutableStringBuilder.h"
#include "compiler/translator/PoolAlloc.h"
#include "gtest/gtest.h"

using namespace sh;

class ImmutableStringBuilderTest : public testing::Test
{
  public:
    ImmutableStringBuilderTest() {}

  protected:
    void SetUp() override
    {
        allocator.push();
        SetGlobalPoolAllocator(&allocator);
    }

    void TearDown() override
    {
        SetGlobalPoolAllocator(nullptr);
        allocator.pop();
    }

    angle::PoolAllocator allocator;
};

// Test writing a 32-bit signed int as hexadecimal using ImmutableStringBuilder.
TEST_F(ImmutableStringBuilderTest, AppendHexInt32)
{
    int32_t i = -1;
    ImmutableStringBuilder strBuilder(2 * sizeof(int32_t));
    strBuilder.appendHex(i);
    ImmutableString str = strBuilder;
    EXPECT_EQ(std::string("ffffffff"), str.data());
}

// Test writing a 32-bit unsigned int as hexadecimal using ImmutableStringBuilder.
TEST_F(ImmutableStringBuilderTest, AppendHexUint32)
{
    uint32_t i = 0x1234beefu;
    ImmutableStringBuilder strBuilder(2 * sizeof(uint32_t));
    strBuilder.appendHex(i);
    ImmutableString str = strBuilder;
    EXPECT_EQ(std::string("1234beef"), str.data());
}

// Test writing a 64-bit signed int as hexadecimal using ImmutableStringBuilder.
TEST_F(ImmutableStringBuilderTest, AppendHexInt64)
{
    int64_t i = -1;
    ImmutableStringBuilder strBuilder(2 * sizeof(int64_t));
    strBuilder.appendHex(i);
    ImmutableString str = strBuilder;
    EXPECT_EQ(std::string("ffffffffffffffff"), str.data());
}

// Test writing a 64-bit unsigned int as hexadecimal using ImmutableStringBuilder.
TEST_F(ImmutableStringBuilderTest, AppendHexUint64)
{
    uint64_t i = 0xfeedcafe9876beefull;
    ImmutableStringBuilder strBuilder(2 * sizeof(uint64_t));
    strBuilder.appendHex(i);
    ImmutableString str = strBuilder;
    EXPECT_EQ(std::string("feedcafe9876beef"), str.data());
}
