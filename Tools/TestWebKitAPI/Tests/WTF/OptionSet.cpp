/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "Test.h"
#include <wtf/OptionSet.h>

namespace TestWebKitAPI {

enum class ExampleFlags : uint64_t {
    A = 1 << 0,
    B = 1 << 1,
    C = 1 << 2,
    D = 1ULL << 31,
    E = 1ULL << 63,
};

TEST(WTF_OptionSet, EmptySet)
{
    OptionSet<ExampleFlags> set;
    EXPECT_TRUE(set.isEmpty());
    EXPECT_FALSE(set.contains(ExampleFlags::A));
    EXPECT_FALSE(set.contains(ExampleFlags::B));
    EXPECT_FALSE(set.contains(ExampleFlags::C));
    EXPECT_FALSE(set.contains(ExampleFlags::D));
    EXPECT_FALSE(set.contains(ExampleFlags::E));
}

TEST(WTF_OptionSet, ContainsOneFlag)
{
    OptionSet<ExampleFlags> set = ExampleFlags::A;
    EXPECT_FALSE(set.isEmpty());
    EXPECT_TRUE(set.contains(ExampleFlags::A));
    EXPECT_FALSE(set.contains(ExampleFlags::B));
    EXPECT_FALSE(set.contains(ExampleFlags::C));
    EXPECT_FALSE(set.contains(ExampleFlags::D));
    EXPECT_FALSE(set.contains(ExampleFlags::E));
}

TEST(WTF_OptionSet, ContainsTwoFlags)
{
    OptionSet<ExampleFlags> set { ExampleFlags::A, ExampleFlags::B };
    EXPECT_FALSE(set.isEmpty());
    EXPECT_TRUE(set.contains(ExampleFlags::A));
    EXPECT_TRUE(set.contains(ExampleFlags::B));
    EXPECT_FALSE(set.contains(ExampleFlags::C));
    EXPECT_FALSE(set.contains(ExampleFlags::D));
    EXPECT_FALSE(set.contains(ExampleFlags::E));
}

TEST(WTF_OptionSet, ContainsTwoFlags2)
{
    OptionSet<ExampleFlags> set { ExampleFlags::A, ExampleFlags::D };
    EXPECT_FALSE(set.isEmpty());
    EXPECT_TRUE(set.contains(ExampleFlags::A));
    EXPECT_TRUE(set.contains(ExampleFlags::D));
    EXPECT_FALSE(set.contains(ExampleFlags::B));
    EXPECT_FALSE(set.contains(ExampleFlags::C));
    EXPECT_FALSE(set.contains(ExampleFlags::E));
}

TEST(WTF_OptionSet, ContainsTwoFlags3)
{
    OptionSet<ExampleFlags> set { ExampleFlags::D, ExampleFlags::E };
    EXPECT_FALSE(set.isEmpty());
    EXPECT_TRUE(set.contains(ExampleFlags::D));
    EXPECT_TRUE(set.contains(ExampleFlags::E));
    EXPECT_FALSE(set.contains(ExampleFlags::A));
    EXPECT_FALSE(set.contains(ExampleFlags::B));
    EXPECT_FALSE(set.contains(ExampleFlags::C));
}

TEST(WTF_OptionSet, OperatorBitwiseOr)
{
    OptionSet<ExampleFlags> set = ExampleFlags::A;
    set |= ExampleFlags::C;
    EXPECT_TRUE(set.contains(ExampleFlags::A));
    EXPECT_FALSE(set.contains(ExampleFlags::B));
    EXPECT_TRUE(set.contains(ExampleFlags::C));
}

TEST(WTF_OptionSet, EmptyOptionSetToRawValueToOptionSet)
{
    OptionSet<ExampleFlags> set;
    EXPECT_TRUE(set.isEmpty());
    EXPECT_FALSE(set.contains(ExampleFlags::A));
    EXPECT_FALSE(set.contains(ExampleFlags::B));
    EXPECT_FALSE(set.contains(ExampleFlags::C));

    auto set2 = OptionSet<ExampleFlags>::fromRaw(set.toRaw());
    EXPECT_TRUE(set2.isEmpty());
    EXPECT_FALSE(set2.contains(ExampleFlags::A));
    EXPECT_FALSE(set2.contains(ExampleFlags::B));
    EXPECT_FALSE(set2.contains(ExampleFlags::C));
}

TEST(WTF_OptionSet, OptionSetThatContainsOneFlagToRawValueToOptionSet)
{
    OptionSet<ExampleFlags> set = ExampleFlags::A;
    EXPECT_FALSE(set.isEmpty());
    EXPECT_TRUE(set.contains(ExampleFlags::A));
    EXPECT_FALSE(set.contains(ExampleFlags::B));
    EXPECT_FALSE(set.contains(ExampleFlags::C));
    EXPECT_FALSE(set.contains(ExampleFlags::D));
    EXPECT_FALSE(set.contains(ExampleFlags::E));

    auto set2 = OptionSet<ExampleFlags>::fromRaw(set.toRaw());
    EXPECT_FALSE(set2.isEmpty());
    EXPECT_TRUE(set2.contains(ExampleFlags::A));
    EXPECT_FALSE(set2.contains(ExampleFlags::B));
    EXPECT_FALSE(set2.contains(ExampleFlags::C));
    EXPECT_FALSE(set2.contains(ExampleFlags::D));
    EXPECT_FALSE(set2.contains(ExampleFlags::E));
}

TEST(WTF_OptionSet, OptionSetThatContainsOneFlagToRawValueToOptionSet2)
{
    OptionSet<ExampleFlags> set = ExampleFlags::E;
    EXPECT_FALSE(set.isEmpty());
    EXPECT_TRUE(set.contains(ExampleFlags::E));
    EXPECT_FALSE(set.contains(ExampleFlags::A));
    EXPECT_FALSE(set.contains(ExampleFlags::B));
    EXPECT_FALSE(set.contains(ExampleFlags::C));
    EXPECT_FALSE(set.contains(ExampleFlags::D));

    auto set2 = OptionSet<ExampleFlags>::fromRaw(set.toRaw());
    EXPECT_FALSE(set2.isEmpty());
    EXPECT_TRUE(set2.contains(ExampleFlags::E));
    EXPECT_FALSE(set2.contains(ExampleFlags::A));
    EXPECT_FALSE(set2.contains(ExampleFlags::B));
    EXPECT_FALSE(set2.contains(ExampleFlags::C));
    EXPECT_FALSE(set2.contains(ExampleFlags::D));
}

TEST(WTF_OptionSet, OptionSetThatContainsTwoFlagsToRawValueToOptionSet)
{
    OptionSet<ExampleFlags> set { ExampleFlags::A, ExampleFlags::C };
    EXPECT_FALSE(set.isEmpty());
    EXPECT_TRUE(set.contains(ExampleFlags::A));
    EXPECT_TRUE(set.contains(ExampleFlags::C));
    EXPECT_FALSE(set.contains(ExampleFlags::B));

    auto set2 = OptionSet<ExampleFlags>::fromRaw(set.toRaw());
    EXPECT_FALSE(set2.isEmpty());
    EXPECT_TRUE(set2.contains(ExampleFlags::A));
    EXPECT_TRUE(set2.contains(ExampleFlags::C));
    EXPECT_FALSE(set2.contains(ExampleFlags::B));
}

TEST(WTF_OptionSet, OptionSetThatContainsTwoFlagsToRawValueToOptionSet2)
{
    OptionSet<ExampleFlags> set { ExampleFlags::D, ExampleFlags::E };
    EXPECT_FALSE(set.isEmpty());
    EXPECT_TRUE(set.contains(ExampleFlags::D));
    EXPECT_TRUE(set.contains(ExampleFlags::E));
    EXPECT_FALSE(set.contains(ExampleFlags::A));
    EXPECT_FALSE(set.contains(ExampleFlags::B));
    EXPECT_FALSE(set.contains(ExampleFlags::C));

    auto set2 = OptionSet<ExampleFlags>::fromRaw(set.toRaw());
    EXPECT_FALSE(set2.isEmpty());
    EXPECT_TRUE(set2.contains(ExampleFlags::D));
    EXPECT_TRUE(set2.contains(ExampleFlags::E));
    EXPECT_FALSE(set2.contains(ExampleFlags::A));
    EXPECT_FALSE(set2.contains(ExampleFlags::B));
    EXPECT_FALSE(set2.contains(ExampleFlags::C));
}

TEST(WTF_OptionSet, TwoIteratorsIntoSameOptionSet)
{
    OptionSet<ExampleFlags> set { ExampleFlags::C, ExampleFlags::B };
    OptionSet<ExampleFlags>::iterator it1 = set.begin();
    OptionSet<ExampleFlags>::iterator it2 = it1;
    ++it1;
    EXPECT_STRONG_ENUM_EQ(ExampleFlags::C, *it1);
    EXPECT_STRONG_ENUM_EQ(ExampleFlags::B, *it2);
}

TEST(WTF_OptionSet, IterateOverOptionSetThatContainsTwoFlags)
{
    OptionSet<ExampleFlags> set { ExampleFlags::A, ExampleFlags::C };
    OptionSet<ExampleFlags>::iterator it = set.begin();
    OptionSet<ExampleFlags>::iterator end = set.end();
    EXPECT_TRUE(it != end);
    EXPECT_STRONG_ENUM_EQ(ExampleFlags::A, *it);
    ++it;
    EXPECT_STRONG_ENUM_EQ(ExampleFlags::C, *it);
    ++it;
    EXPECT_TRUE(it == end);
}

TEST(WTF_OptionSet, IterateOverOptionSetThatContainsFlags2)
{
    OptionSet<ExampleFlags> set { ExampleFlags::D, ExampleFlags::E };
    OptionSet<ExampleFlags>::iterator it = set.begin();
    OptionSet<ExampleFlags>::iterator end = set.end();
    EXPECT_TRUE(it != end);
    EXPECT_STRONG_ENUM_EQ(ExampleFlags::D, *it);
    ++it;
    EXPECT_STRONG_ENUM_EQ(ExampleFlags::E, *it);
    ++it;
    EXPECT_TRUE(it == end);
}

TEST(WTF_OptionSet, NextItemAfterLargestIn32BitFlagSet)
{
    enum class ThirtyTwoBitFlags : uint32_t {
        A = 1UL << 31,
    };
    OptionSet<ThirtyTwoBitFlags> set { ThirtyTwoBitFlags::A };
    OptionSet<ThirtyTwoBitFlags>::iterator it = set.begin();
    OptionSet<ThirtyTwoBitFlags>::iterator end = set.end();
    EXPECT_TRUE(it != end);
    ++it;
    EXPECT_TRUE(it == end);
}

TEST(WTF_OptionSet, NextItemAfterLargestIn64BitFlagSet)
{
    enum class SixtyFourBitFlags : uint64_t {
        A = 1ULL << 63,
    };
    OptionSet<SixtyFourBitFlags> set { SixtyFourBitFlags::A };
    OptionSet<SixtyFourBitFlags>::iterator it = set.begin();
    OptionSet<SixtyFourBitFlags>::iterator end = set.end();
    EXPECT_TRUE(it != end);
    ++it;
    EXPECT_TRUE(it == end);
}

TEST(WTF_OptionSet, IterationOrderTheSameRegardlessOfInsertionOrder)
{
    OptionSet<ExampleFlags> set1 = ExampleFlags::C;
    set1 |= ExampleFlags::A;

    OptionSet<ExampleFlags> set2 = ExampleFlags::A;
    set2 |= ExampleFlags::C;

    OptionSet<ExampleFlags>::iterator it1 = set1.begin();
    OptionSet<ExampleFlags>::iterator it2 = set2.begin();

    EXPECT_TRUE(*it1 == *it2);
    ++it1;
    ++it2;
    EXPECT_TRUE(*it1 == *it2);
}

} // namespace TestWebKitAPI
