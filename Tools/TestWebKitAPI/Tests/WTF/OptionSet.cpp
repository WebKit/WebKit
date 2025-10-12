/*
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
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
#include <wtf/HashSet.h>
#include <wtf/OptionSet.h>
#include <wtf/OptionSetHash.h>

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
    auto test = [] <ConcurrencyTag concurrency> () {
        using OptionSetType = OptionSet<ExampleFlags, concurrency>;
        OptionSetType set;
        EXPECT_TRUE(set.isEmpty());
        EXPECT_FALSE(set.contains(ExampleFlags::A));
        EXPECT_FALSE(set.contains(ExampleFlags::B));
        EXPECT_FALSE(set.contains(ExampleFlags::C));
        EXPECT_FALSE(set.contains(ExampleFlags::D));
        EXPECT_FALSE(set.contains(ExampleFlags::E));
    };
    test.operator()<ConcurrencyTag::None>();
    test.operator()<ConcurrencyTag::Atomic>();
}

TEST(WTF_OptionSet, ContainsOneFlag)
{
    auto test = [] <ConcurrencyTag concurrency> () {
        using OptionSetType = OptionSet<ExampleFlags, concurrency>;
        OptionSetType set = ExampleFlags::A;
        EXPECT_FALSE(set.isEmpty());
        EXPECT_TRUE(set.contains(ExampleFlags::A));
        EXPECT_FALSE(set.contains(ExampleFlags::B));
        EXPECT_FALSE(set.contains(ExampleFlags::C));
        EXPECT_FALSE(set.contains(ExampleFlags::D));
        EXPECT_FALSE(set.contains(ExampleFlags::E));
    };
    test.operator()<ConcurrencyTag::None>();
    test.operator()<ConcurrencyTag::Atomic>();
}

TEST(WTF_OptionSet, Equal)
{
    auto test = [] <ConcurrencyTag concurrency> () {
        using OptionSetType = OptionSet<ExampleFlags, concurrency>;
        OptionSetType set { ExampleFlags::A, ExampleFlags::B };
        EXPECT_TRUE((set == OptionSetType { ExampleFlags::A, ExampleFlags::B }));
        EXPECT_TRUE((set == OptionSetType { ExampleFlags::B, ExampleFlags::A }));
        EXPECT_FALSE(set == ExampleFlags::B);
    };
    test.operator()<ConcurrencyTag::None>();
    test.operator()<ConcurrencyTag::Atomic>();
}

TEST(WTF_OptionSet, NotEqual)
{
    auto test = [] <ConcurrencyTag concurrency> () {
        using OptionSetType = OptionSet<ExampleFlags, concurrency>;
        OptionSetType set = ExampleFlags::A;
        EXPECT_TRUE(set != ExampleFlags::B);
        EXPECT_FALSE(set != ExampleFlags::A);
    };
    test.operator()<ConcurrencyTag::None>();
    test.operator()<ConcurrencyTag::Atomic>();
}

TEST(WTF_OptionSet, Or)
{
    auto test = [] <ConcurrencyTag concurrency> () {
        using OptionSetType = OptionSet<ExampleFlags, concurrency>;
        OptionSetType set { ExampleFlags::A, ExampleFlags::B, ExampleFlags::C };
        OptionSetType set2 { ExampleFlags::C, ExampleFlags::D };
        EXPECT_TRUE(((set | ExampleFlags::A) == OptionSetType { ExampleFlags::A, ExampleFlags::B, ExampleFlags::C }));
        EXPECT_TRUE(((set | ExampleFlags::D) == OptionSetType { ExampleFlags::A, ExampleFlags::B, ExampleFlags::C, ExampleFlags::D }));
        EXPECT_TRUE(((set | set2) == OptionSetType { ExampleFlags::A, ExampleFlags::B, ExampleFlags::C, ExampleFlags::D }));
    };
    test.operator()<ConcurrencyTag::None>();
    test.operator()<ConcurrencyTag::Atomic>();
}

TEST(WTF_OptionSet, OrAssignment)
{
    auto test = [] <ConcurrencyTag concurrency> () {
        using OptionSetType = OptionSet<ExampleFlags, concurrency>;
        OptionSetType set { ExampleFlags::A, ExampleFlags::B, ExampleFlags::C };

        set |= { };
        EXPECT_TRUE((set == OptionSetType { ExampleFlags::A, ExampleFlags::B, ExampleFlags::C }));

        set |= { ExampleFlags::A };
        EXPECT_TRUE((set == OptionSetType { ExampleFlags::A, ExampleFlags::B, ExampleFlags::C }));

        set |= { ExampleFlags::C, ExampleFlags::D };
        EXPECT_TRUE((set == OptionSetType { ExampleFlags::A, ExampleFlags::B, ExampleFlags::C, ExampleFlags::D }));
    };
    test.operator()<ConcurrencyTag::None>();
    test.operator()<ConcurrencyTag::Atomic>();
}

TEST(WTF_OptionSet, Minus)
{
    auto test = [] <ConcurrencyTag concurrency> () {
        using OptionSetType = OptionSet<ExampleFlags, concurrency>;
        OptionSetType set { ExampleFlags::A, ExampleFlags::B, ExampleFlags::C };

        EXPECT_TRUE(((set - ExampleFlags::A) == OptionSetType { ExampleFlags::B, ExampleFlags::C }));
        EXPECT_TRUE(((set - ExampleFlags::D) == OptionSetType { ExampleFlags::A, ExampleFlags::B, ExampleFlags::C }));
        EXPECT_TRUE((set - set).isEmpty());
    };
    test.operator()<ConcurrencyTag::None>();
    test.operator()<ConcurrencyTag::Atomic>();
}

TEST(WTF_OptionSet, AddAndRemove)
{
    auto test = [] <ConcurrencyTag concurrency> () {
        using OptionSetType = OptionSet<ExampleFlags, concurrency>;
        OptionSetType set;

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
    };
    test.operator()<ConcurrencyTag::None>();
    test.operator()<ConcurrencyTag::Atomic>();
}

TEST(WTF_OptionSet, Set)
{
    auto test = [] <ConcurrencyTag concurrency> () {
        using OptionSetType = OptionSet<ExampleFlags, concurrency>;
        OptionSetType set;

        set.set(ExampleFlags::A, true);
        EXPECT_TRUE(set.contains(ExampleFlags::A));
        EXPECT_FALSE(set.contains(ExampleFlags::B));
        EXPECT_FALSE(set.contains(ExampleFlags::C));

        set.set({ ExampleFlags::B, ExampleFlags::C }, true);
        EXPECT_TRUE(set.contains(ExampleFlags::A));
        EXPECT_TRUE(set.contains(ExampleFlags::B));
        EXPECT_TRUE(set.contains(ExampleFlags::C));

        set.set(ExampleFlags::B, false);
        EXPECT_TRUE(set.contains(ExampleFlags::A));
        EXPECT_FALSE(set.contains(ExampleFlags::B));
        EXPECT_TRUE(set.contains(ExampleFlags::C));

        set.set({ ExampleFlags::A, ExampleFlags::C }, false);
        EXPECT_FALSE(set.contains(ExampleFlags::A));
        EXPECT_FALSE(set.contains(ExampleFlags::B));
        EXPECT_FALSE(set.contains(ExampleFlags::C));
    };
    test.operator()<ConcurrencyTag::None>();
    test.operator()<ConcurrencyTag::Atomic>();
}

TEST(WTF_OptionSet, ContainsTwoFlags)
{
    auto test = [] <ConcurrencyTag concurrency> () {
        using OptionSetType = OptionSet<ExampleFlags, concurrency>;
        OptionSetType set { ExampleFlags::A, ExampleFlags::B };

        EXPECT_FALSE(set.isEmpty());
        EXPECT_TRUE(set.contains(ExampleFlags::A));
        EXPECT_TRUE(set.contains(ExampleFlags::B));
        EXPECT_FALSE(set.contains(ExampleFlags::C));
        EXPECT_FALSE(set.contains(ExampleFlags::D));
        EXPECT_FALSE(set.contains(ExampleFlags::E));
    };
    test.operator()<ConcurrencyTag::None>();
    test.operator()<ConcurrencyTag::Atomic>();
}

TEST(WTF_OptionSet, ContainsTwoFlags2)
{
    auto test = [] <ConcurrencyTag concurrency> () {
        using OptionSetType = OptionSet<ExampleFlags, concurrency>;
        OptionSetType set { ExampleFlags::A, ExampleFlags::D };

        EXPECT_FALSE(set.isEmpty());
        EXPECT_TRUE(set.contains(ExampleFlags::A));
        EXPECT_TRUE(set.contains(ExampleFlags::D));
        EXPECT_FALSE(set.contains(ExampleFlags::B));
        EXPECT_FALSE(set.contains(ExampleFlags::C));
        EXPECT_FALSE(set.contains(ExampleFlags::E));
    };
    test.operator()<ConcurrencyTag::None>();
    test.operator()<ConcurrencyTag::Atomic>();
}

TEST(WTF_OptionSet, ContainsTwoFlags3)
{
    auto test = [] <ConcurrencyTag concurrency> () {
        using OptionSetType = OptionSet<ExampleFlags, concurrency>;
        OptionSetType set { ExampleFlags::D, ExampleFlags::E };

        EXPECT_FALSE(set.isEmpty());
        EXPECT_TRUE(set.contains(ExampleFlags::D));
        EXPECT_TRUE(set.contains(ExampleFlags::E));
        EXPECT_FALSE(set.contains(ExampleFlags::A));
        EXPECT_FALSE(set.contains(ExampleFlags::B));
        EXPECT_FALSE(set.contains(ExampleFlags::C));
    };
    test.operator()<ConcurrencyTag::None>();
    test.operator()<ConcurrencyTag::Atomic>();
}

TEST(WTF_OptionSet, EmptyOptionSetToRawValueToOptionSet)
{
    auto test = [] <ConcurrencyTag concurrency> () {
        using OptionSetType = OptionSet<ExampleFlags, concurrency>;
        OptionSetType set;
        EXPECT_TRUE(set.isEmpty());
        EXPECT_FALSE(set.contains(ExampleFlags::A));
        EXPECT_FALSE(set.contains(ExampleFlags::B));
        EXPECT_FALSE(set.contains(ExampleFlags::C));

        auto set2 = OptionSetType::fromRaw(set.toRaw());
        EXPECT_TRUE(set2.isEmpty());
        EXPECT_FALSE(set2.contains(ExampleFlags::A));
        EXPECT_FALSE(set2.contains(ExampleFlags::B));
        EXPECT_FALSE(set2.contains(ExampleFlags::C));
    };
    test.operator()<ConcurrencyTag::None>();
    test.operator()<ConcurrencyTag::Atomic>();
}

TEST(WTF_OptionSet, OptionSetThatContainsOneFlagToRawValueToOptionSet)
{
    auto test = [] <ConcurrencyTag concurrency> () {
        using OptionSetType = OptionSet<ExampleFlags, concurrency>;
        OptionSetType set = ExampleFlags::A;
        EXPECT_FALSE(set.isEmpty());
        EXPECT_TRUE(set.contains(ExampleFlags::A));
        EXPECT_FALSE(set.contains(ExampleFlags::B));
        EXPECT_FALSE(set.contains(ExampleFlags::C));
        EXPECT_FALSE(set.contains(ExampleFlags::D));
        EXPECT_FALSE(set.contains(ExampleFlags::E));

        auto set2 = OptionSetType::fromRaw(set.toRaw());
        EXPECT_FALSE(set2.isEmpty());
        EXPECT_TRUE(set2.contains(ExampleFlags::A));
        EXPECT_FALSE(set2.contains(ExampleFlags::B));
        EXPECT_FALSE(set2.contains(ExampleFlags::C));
        EXPECT_FALSE(set2.contains(ExampleFlags::D));
        EXPECT_FALSE(set2.contains(ExampleFlags::E));
    };
    test.operator()<ConcurrencyTag::None>();
    test.operator()<ConcurrencyTag::Atomic>();
}

TEST(WTF_OptionSet, OptionSetThatContainsOneFlagToRawValueToOptionSet2)
{
    auto test = [] <ConcurrencyTag concurrency> () {
        using OptionSetType = OptionSet<ExampleFlags, concurrency>;
        OptionSetType set = ExampleFlags::E;
        EXPECT_FALSE(set.isEmpty());
        EXPECT_TRUE(set.contains(ExampleFlags::E));
        EXPECT_FALSE(set.contains(ExampleFlags::A));
        EXPECT_FALSE(set.contains(ExampleFlags::B));
        EXPECT_FALSE(set.contains(ExampleFlags::C));
        EXPECT_FALSE(set.contains(ExampleFlags::D));

        auto set2 = OptionSetType::fromRaw(set.toRaw());
        EXPECT_FALSE(set2.isEmpty());
        EXPECT_TRUE(set2.contains(ExampleFlags::E));
        EXPECT_FALSE(set2.contains(ExampleFlags::A));
        EXPECT_FALSE(set2.contains(ExampleFlags::B));
        EXPECT_FALSE(set2.contains(ExampleFlags::C));
        EXPECT_FALSE(set2.contains(ExampleFlags::D));
    };
    test.operator()<ConcurrencyTag::None>();
    test.operator()<ConcurrencyTag::Atomic>();
}

TEST(WTF_OptionSet, OptionSetThatContainsTwoFlagsToRawValueToOptionSet)
{
    auto test = [] <ConcurrencyTag concurrency> () {
        using OptionSetType = OptionSet<ExampleFlags, concurrency>;
        OptionSetType set { ExampleFlags::A, ExampleFlags::C };
        EXPECT_FALSE(set.isEmpty());
        EXPECT_TRUE(set.contains(ExampleFlags::A));
        EXPECT_TRUE(set.contains(ExampleFlags::C));
        EXPECT_FALSE(set.contains(ExampleFlags::B));

        auto set2 = OptionSetType::fromRaw(set.toRaw());
        EXPECT_FALSE(set2.isEmpty());
        EXPECT_TRUE(set2.contains(ExampleFlags::A));
        EXPECT_TRUE(set2.contains(ExampleFlags::C));
        EXPECT_FALSE(set2.contains(ExampleFlags::B));
    };
    test.operator()<ConcurrencyTag::None>();
    test.operator()<ConcurrencyTag::Atomic>();
}

TEST(WTF_OptionSet, OptionSetThatContainsTwoFlagsToRawValueToOptionSet2)
{
    auto test = [] <ConcurrencyTag concurrency> () {
        using OptionSetType = OptionSet<ExampleFlags, concurrency>;
        OptionSetType set { ExampleFlags::D, ExampleFlags::E };
        EXPECT_FALSE(set.isEmpty());
        EXPECT_TRUE(set.contains(ExampleFlags::D));
        EXPECT_TRUE(set.contains(ExampleFlags::E));
        EXPECT_FALSE(set.contains(ExampleFlags::A));
        EXPECT_FALSE(set.contains(ExampleFlags::B));
        EXPECT_FALSE(set.contains(ExampleFlags::C));

        auto set2 = OptionSetType::fromRaw(set.toRaw());
        EXPECT_FALSE(set2.isEmpty());
        EXPECT_TRUE(set2.contains(ExampleFlags::D));
        EXPECT_TRUE(set2.contains(ExampleFlags::E));
        EXPECT_FALSE(set2.contains(ExampleFlags::A));
        EXPECT_FALSE(set2.contains(ExampleFlags::B));
        EXPECT_FALSE(set2.contains(ExampleFlags::C));
    };
    test.operator()<ConcurrencyTag::None>();
    test.operator()<ConcurrencyTag::Atomic>();
}

TEST(WTF_OptionSet, TwoIteratorsIntoSameOptionSet)
{
    auto test = [] <ConcurrencyTag concurrency> () {
        using OptionSetType = OptionSet<ExampleFlags, concurrency>;
        OptionSetType set { ExampleFlags::C, ExampleFlags::B };
        typename OptionSetType::iterator it1 = set.begin();
        typename OptionSetType::iterator it2 = it1;
        ++it1;
        EXPECT_STRONG_ENUM_EQ(ExampleFlags::C, *it1);
        EXPECT_STRONG_ENUM_EQ(ExampleFlags::B, *it2);
    };
    test.operator()<ConcurrencyTag::None>();
    test.operator()<ConcurrencyTag::Atomic>();
}

TEST(WTF_OptionSet, IterateOverOptionSetThatContainsTwoFlags)
{
    auto test = [] <ConcurrencyTag concurrency> () {
        using OptionSetType = OptionSet<ExampleFlags, concurrency>;
        OptionSetType set { ExampleFlags::A, ExampleFlags::C };
        typename OptionSetType::iterator it = set.begin();
        typename OptionSetType::iterator end = set.end();
        EXPECT_TRUE(it != end);
        EXPECT_STRONG_ENUM_EQ(ExampleFlags::A, *it);
        ++it;
        EXPECT_STRONG_ENUM_EQ(ExampleFlags::C, *it);
        ++it;
        EXPECT_TRUE(it == end);
    };
    test.operator()<ConcurrencyTag::None>();
    test.operator()<ConcurrencyTag::Atomic>();
}

TEST(WTF_OptionSet, IterateOverOptionSetThatContainsFlags2)
{
    auto test = [] <ConcurrencyTag concurrency> () {
        using OptionSetType = OptionSet<ExampleFlags, concurrency>;
        OptionSetType set { ExampleFlags::D, ExampleFlags::E };
        typename OptionSetType::iterator it = set.begin();
        typename OptionSetType::iterator end = set.end();
        EXPECT_TRUE(it != end);
        EXPECT_STRONG_ENUM_EQ(ExampleFlags::D, *it);
        ++it;
        EXPECT_STRONG_ENUM_EQ(ExampleFlags::E, *it);
        ++it;
        EXPECT_TRUE(it == end);
    };
    test.operator()<ConcurrencyTag::None>();
    test.operator()<ConcurrencyTag::Atomic>();
}

TEST(WTF_OptionSet, NextItemAfterLargestIn32BitFlagSet)
{
    auto test = [] <ConcurrencyTag concurrency> () {
        enum class ThirtyTwoBitFlags : uint32_t {
            A = 1UL << 31,
        };
        using OptionSetType = OptionSet<ThirtyTwoBitFlags, concurrency>;
        OptionSetType set { ThirtyTwoBitFlags::A };
        typename OptionSetType::iterator it = set.begin();
        typename OptionSetType::iterator end = set.end();
        EXPECT_TRUE(it != end);
        ++it;
        EXPECT_TRUE(it == end);
    };
    test.operator()<ConcurrencyTag::None>();
    test.operator()<ConcurrencyTag::Atomic>();
}

TEST(WTF_OptionSet, NextItemAfterLargestIn64BitFlagSet)
{
    auto test = [] <ConcurrencyTag concurrency> () {
        enum class SixtyFourBitFlags : uint64_t {
            A = 1ULL << 63,
        };
        using OptionSetType = OptionSet<SixtyFourBitFlags, concurrency>;
        OptionSetType set { SixtyFourBitFlags::A };
        typename OptionSetType::iterator it = set.begin();
        typename OptionSetType::iterator end = set.end();
        EXPECT_TRUE(it != end);
        ++it;
        EXPECT_TRUE(it == end);
    };
    test.operator()<ConcurrencyTag::None>();
    test.operator()<ConcurrencyTag::Atomic>();
}

TEST(WTF_OptionSet, IterationOrderTheSameRegardlessOfInsertionOrder)
{
    auto test = [] <ConcurrencyTag concurrency> () {
        using OptionSetType = OptionSet<ExampleFlags, concurrency>;
        OptionSetType set1 = ExampleFlags::C;
        set1.add(ExampleFlags::A);

        OptionSetType set2 = ExampleFlags::A;
        set2.add(ExampleFlags::C);

        typename OptionSetType::iterator it1 = set1.begin();
        typename OptionSetType::iterator it2 = set2.begin();

        EXPECT_TRUE(*it1 == *it2);
        ++it1;
        ++it2;
        EXPECT_TRUE(*it1 == *it2);
    };
    test.operator()<ConcurrencyTag::None>();
    test.operator()<ConcurrencyTag::Atomic>();
}

TEST(WTF_OptionSet, OperatorAnd)
{
    auto test = [] <ConcurrencyTag concurrency> () {
        using OptionSetType = OptionSet<ExampleFlags, concurrency>;
        OptionSetType a { ExampleFlags::A };
        OptionSetType ac { ExampleFlags::A, ExampleFlags::C };
        OptionSetType bc { ExampleFlags::B, ExampleFlags::C };
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
    };
    test.operator()<ConcurrencyTag::None>();
    test.operator()<ConcurrencyTag::Atomic>();
}

TEST(WTF_OptionSet, OperatorXor)
{
    auto test = [] <ConcurrencyTag concurrency> () {
        using OptionSetType = OptionSet<ExampleFlags, concurrency>;
        OptionSetType a { ExampleFlags::A };
        OptionSetType ac { ExampleFlags::A, ExampleFlags::C };
        OptionSetType bc { ExampleFlags::B, ExampleFlags::C };
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
    };
    test.operator()<ConcurrencyTag::None>();
    test.operator()<ConcurrencyTag::Atomic>();
}

TEST(WTF_OptionSet, ContainsAny)
{
    auto test = [] <ConcurrencyTag concurrency> () {
        using OptionSetType = OptionSet<ExampleFlags, concurrency>;
        OptionSetType set { ExampleFlags::A, ExampleFlags::B };

        EXPECT_TRUE(set.containsAny({ ExampleFlags::A }));
        EXPECT_TRUE(set.containsAny({ ExampleFlags::B }));
        EXPECT_FALSE(set.containsAny({ ExampleFlags::C }));
        EXPECT_FALSE(set.containsAny({ ExampleFlags::C, ExampleFlags::D }));
        EXPECT_TRUE(set.containsAny({ ExampleFlags::A, ExampleFlags::B }));
        EXPECT_TRUE(set.containsAny({ ExampleFlags::B, ExampleFlags::C }));
        EXPECT_TRUE(set.containsAny({ ExampleFlags::A, ExampleFlags::C }));
        EXPECT_TRUE(set.containsAny({ ExampleFlags::A, ExampleFlags::B, ExampleFlags::C }));
    };
    test.operator()<ConcurrencyTag::None>();
    test.operator()<ConcurrencyTag::Atomic>();
}

TEST(WTF_OptionSet, ContainsAll)
{
    auto test = [] <ConcurrencyTag concurrency> () {
        using OptionSetType = OptionSet<ExampleFlags, concurrency>;
        OptionSetType set { ExampleFlags::A, ExampleFlags::B };

        EXPECT_TRUE(set.containsAll({ ExampleFlags::A }));
        EXPECT_TRUE(set.containsAll({ ExampleFlags::B }));
        EXPECT_FALSE(set.containsAll({ ExampleFlags::C }));
        EXPECT_FALSE(set.containsAll({ ExampleFlags::C, ExampleFlags::D }));
        EXPECT_TRUE(set.containsAll({ ExampleFlags::A, ExampleFlags::B }));
        EXPECT_FALSE(set.containsAll({ ExampleFlags::B, ExampleFlags::C }));
        EXPECT_FALSE(set.containsAll({ ExampleFlags::A, ExampleFlags::C }));
        EXPECT_FALSE(set.containsAll({ ExampleFlags::A, ExampleFlags::B, ExampleFlags::C }));
    };
    test.operator()<ConcurrencyTag::None>();
    test.operator()<ConcurrencyTag::Atomic>();
}

TEST(WTF_OptionSet, HashSet)
{
    auto test = [] <ConcurrencyTag concurrency> () {
        using OptionSetType = OptionSet<ExampleFlags, concurrency>;
        HashSet<OptionSetType> hashSet;
        EXPECT_TRUE(hashSet.add(OptionSetType()).isNewEntry);
        EXPECT_TRUE(hashSet.add({ ExampleFlags::A }).isNewEntry);
        EXPECT_TRUE(hashSet.add({ ExampleFlags::A, ExampleFlags::B }).isNewEntry);
        EXPECT_FALSE(hashSet.add(OptionSetType()).isNewEntry);
        EXPECT_FALSE(hashSet.add({ ExampleFlags::A }).isNewEntry);
        EXPECT_FALSE(hashSet.add({ ExampleFlags::A, ExampleFlags::B }).isNewEntry);
        EXPECT_TRUE(hashSet.remove(OptionSetType()));
        EXPECT_TRUE(hashSet.remove({ ExampleFlags::A }));
        EXPECT_TRUE(hashSet.remove({ ExampleFlags::A, ExampleFlags::B }));
        EXPECT_TRUE(hashSet.add(OptionSetType()).isNewEntry);
        EXPECT_TRUE(hashSet.add({ ExampleFlags::A }).isNewEntry);
        EXPECT_TRUE(hashSet.add({ ExampleFlags::A, ExampleFlags::B }).isNewEntry);
    };
    test.operator()<ConcurrencyTag::None>();
    test.operator()<ConcurrencyTag::Atomic>();
}

} // namespace TestWebKitAPI
