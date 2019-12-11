/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

TEST(WTF_OptionSet, Equal)
{
    OptionSet<ExampleFlags> set { ExampleFlags::A, ExampleFlags::B };
    
    EXPECT_TRUE((set == OptionSet<ExampleFlags> { ExampleFlags::A, ExampleFlags::B }));
    EXPECT_TRUE((set == OptionSet<ExampleFlags> { ExampleFlags::B, ExampleFlags::A }));
    EXPECT_FALSE(set == ExampleFlags::B);
}

TEST(WTF_OptionSet, NotEqual)
{
    OptionSet<ExampleFlags> set = ExampleFlags::A;
    
    EXPECT_TRUE(set != ExampleFlags::B);
    EXPECT_FALSE(set != ExampleFlags::A);
}

TEST(WTF_OptionSet, Or)
{
    OptionSet<ExampleFlags> set { ExampleFlags::A, ExampleFlags::B, ExampleFlags::C };
    OptionSet<ExampleFlags> set2 { ExampleFlags::C, ExampleFlags::D };

    EXPECT_TRUE(((set | ExampleFlags::A) == OptionSet<ExampleFlags> { ExampleFlags::A, ExampleFlags::B, ExampleFlags::C }));
    EXPECT_TRUE(((set | ExampleFlags::D) == OptionSet<ExampleFlags> { ExampleFlags::A, ExampleFlags::B, ExampleFlags::C, ExampleFlags::D }));
    EXPECT_TRUE(((set | set2) == OptionSet<ExampleFlags> { ExampleFlags::A, ExampleFlags::B, ExampleFlags::C, ExampleFlags::D }));
}

TEST(WTF_OptionSet, Minus)
{
    OptionSet<ExampleFlags> set { ExampleFlags::A, ExampleFlags::B, ExampleFlags::C };

    EXPECT_TRUE(((set - ExampleFlags::A) == OptionSet<ExampleFlags> { ExampleFlags::B, ExampleFlags::C }));
    EXPECT_TRUE(((set - ExampleFlags::D) == OptionSet<ExampleFlags> { ExampleFlags::A, ExampleFlags::B, ExampleFlags::C }));
    EXPECT_TRUE((set - set).isEmpty());
}

TEST(WTF_OptionSet, AddAndRemove)
{
    OptionSet<ExampleFlags> set;

    set.add(ExampleFlags::A);
    EXPECT_TRUE(set.contains(ExampleFlags::A));
    EXPECT_FALSE(set.contains(ExampleFlags::B));
    EXPECT_FALSE(set.contains(ExampleFlags::C));

    set.add({ ExampleFlags::B, ExampleFlags::C });
    EXPECT_TRUE(set.contains(ExampleFlags::A));
    EXPECT_TRUE(set.contains(ExampleFlags::B));
    EXPECT_TRUE(set.contains(ExampleFlags::C));

    set.remove(ExampleFlags::B);
    EXPECT_TRUE(set.contains(ExampleFlags::A));
    EXPECT_FALSE(set.contains(ExampleFlags::B));
    EXPECT_TRUE(set.contains(ExampleFlags::C));

    set.remove({ ExampleFlags::A, ExampleFlags::C });
    EXPECT_FALSE(set.contains(ExampleFlags::A));
    EXPECT_FALSE(set.contains(ExampleFlags::B));
    EXPECT_FALSE(set.contains(ExampleFlags::C));
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
    set1.add(ExampleFlags::A);

    OptionSet<ExampleFlags> set2 = ExampleFlags::A;
    set2.add(ExampleFlags::C);

    OptionSet<ExampleFlags>::iterator it1 = set1.begin();
    OptionSet<ExampleFlags>::iterator it2 = set2.begin();

    EXPECT_TRUE(*it1 == *it2);
    ++it1;
    ++it2;
    EXPECT_TRUE(*it1 == *it2);
}

TEST(WTF_OptionSet, OperatorAnd)
{
    OptionSet<ExampleFlags> a { ExampleFlags::A };
    OptionSet<ExampleFlags> ac { ExampleFlags::A, ExampleFlags::C };
    OptionSet<ExampleFlags> bc { ExampleFlags::B, ExampleFlags::C };
    {
        auto set = a & ac;
        EXPECT_TRUE(!!set);
        EXPECT_FALSE(set.isEmpty());
        EXPECT_TRUE(set.contains(ExampleFlags::A));
        EXPECT_FALSE(set.contains(ExampleFlags::B));
        EXPECT_FALSE(set.contains(ExampleFlags::C));
    }
    {
        auto set = a & bc;
        EXPECT_FALSE(!!set);
        EXPECT_TRUE(set.isEmpty());
        EXPECT_FALSE(set.contains(ExampleFlags::A));
        EXPECT_FALSE(set.contains(ExampleFlags::B));
        EXPECT_FALSE(set.contains(ExampleFlags::C));
    }
    {
        auto set = ac & bc;
        EXPECT_TRUE(!!set);
        EXPECT_FALSE(set.isEmpty());
        EXPECT_FALSE(set.contains(ExampleFlags::A));
        EXPECT_FALSE(set.contains(ExampleFlags::B));
        EXPECT_TRUE(set.contains(ExampleFlags::C));
    }
    {
        auto set = ExampleFlags::A & bc;
        EXPECT_FALSE(!!set);
        EXPECT_TRUE(set.isEmpty());
        EXPECT_FALSE(set.contains(ExampleFlags::A));
        EXPECT_FALSE(set.contains(ExampleFlags::B));
        EXPECT_FALSE(set.contains(ExampleFlags::C));
    }
    {
        auto set = ExampleFlags::A & ac;
        EXPECT_TRUE(!!set);
        EXPECT_FALSE(set.isEmpty());
        EXPECT_TRUE(set.contains(ExampleFlags::A));
        EXPECT_FALSE(set.contains(ExampleFlags::B));
        EXPECT_FALSE(set.contains(ExampleFlags::C));
    }
    {
        auto set = bc & ExampleFlags::A;
        EXPECT_FALSE(!!set);
        EXPECT_TRUE(set.isEmpty());
        EXPECT_FALSE(set.contains(ExampleFlags::A));
        EXPECT_FALSE(set.contains(ExampleFlags::B));
        EXPECT_FALSE(set.contains(ExampleFlags::C));
    }
    {
        auto set = ac & ExampleFlags::A;
        EXPECT_TRUE(!!set);
        EXPECT_FALSE(set.isEmpty());
        EXPECT_TRUE(set.contains(ExampleFlags::A));
        EXPECT_FALSE(set.contains(ExampleFlags::B));
        EXPECT_FALSE(set.contains(ExampleFlags::C));
    }
}

TEST(WTF_OptionSet, OperatorXor)
{
    OptionSet<ExampleFlags> a { ExampleFlags::A };
    OptionSet<ExampleFlags> ac { ExampleFlags::A, ExampleFlags::C };
    OptionSet<ExampleFlags> bc { ExampleFlags::B, ExampleFlags::C };
    {
        auto set = a ^ ac;
        EXPECT_FALSE(set.contains(ExampleFlags::A));
        EXPECT_FALSE(set.contains(ExampleFlags::B));
        EXPECT_TRUE(set.contains(ExampleFlags::C));
    }
    {
        auto set = a ^ bc;
        EXPECT_TRUE(set.contains(ExampleFlags::A));
        EXPECT_TRUE(set.contains(ExampleFlags::B));
        EXPECT_TRUE(set.contains(ExampleFlags::C));
    }
    {
        auto set = ac ^ bc;
        EXPECT_TRUE(set.contains(ExampleFlags::A));
        EXPECT_TRUE(set.contains(ExampleFlags::B));
        EXPECT_FALSE(set.contains(ExampleFlags::C));
    }
}

TEST(WTF_OptionSet, ContainsAny)
{
    OptionSet<ExampleFlags> set { ExampleFlags::A, ExampleFlags::B };

    EXPECT_TRUE(set.containsAny({ ExampleFlags::A }));
    EXPECT_TRUE(set.containsAny({ ExampleFlags::B }));
    EXPECT_FALSE(set.containsAny({ ExampleFlags::C }));
    EXPECT_FALSE(set.containsAny({ ExampleFlags::C, ExampleFlags::D }));
    EXPECT_TRUE(set.containsAny({ ExampleFlags::A, ExampleFlags::B }));
    EXPECT_TRUE(set.containsAny({ ExampleFlags::B, ExampleFlags::C }));
    EXPECT_TRUE(set.containsAny({ ExampleFlags::A, ExampleFlags::C }));
    EXPECT_TRUE(set.containsAny({ ExampleFlags::A, ExampleFlags::B, ExampleFlags::C }));
}

TEST(WTF_OptionSet, ContainsAll)
{
    OptionSet<ExampleFlags> set { ExampleFlags::A, ExampleFlags::B };

    EXPECT_TRUE(set.containsAll({ ExampleFlags::A }));
    EXPECT_TRUE(set.containsAll({ ExampleFlags::B }));
    EXPECT_FALSE(set.containsAll({ ExampleFlags::C }));
    EXPECT_FALSE(set.containsAll({ ExampleFlags::C, ExampleFlags::D }));
    EXPECT_TRUE(set.containsAll({ ExampleFlags::A, ExampleFlags::B }));
    EXPECT_FALSE(set.containsAll({ ExampleFlags::B, ExampleFlags::C }));
    EXPECT_FALSE(set.containsAll({ ExampleFlags::A, ExampleFlags::C }));
    EXPECT_FALSE(set.containsAll({ ExampleFlags::A, ExampleFlags::B, ExampleFlags::C }));
}

} // namespace TestWebKitAPI
