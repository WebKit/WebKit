/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "Helpers/Test.h"
#include <wtf/EnumSet.h>
#include <wtf/HashSet.h>

namespace TestWebKitAPI {

enum class EnumSetTestFlags : uint8_t {
    A,
    B,
    C,
    D = 31,
    E = 63,
};

TEST(WTF_EnumSet, EmptySet)
{
    using EnumSetType = EnumSet<EnumSetTestFlags>;
    EnumSetType set;
    EXPECT_TRUE(set.isEmpty());
    EXPECT_FALSE(set.contains(EnumSetTestFlags::A));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::B));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::C));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::D));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::E));
}

TEST(WTF_EnumSet, ContainsOneFlag)
{
    using EnumSetType = EnumSet<EnumSetTestFlags>;
    EnumSetType set = EnumSetTestFlags::A;
    EXPECT_FALSE(set.isEmpty());
    EXPECT_TRUE(set.contains(EnumSetTestFlags::A));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::B));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::C));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::D));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::E));
}

TEST(WTF_EnumSet, Equal)
{
    using EnumSetType = EnumSet<EnumSetTestFlags>;
    EnumSetType set { EnumSetTestFlags::A, EnumSetTestFlags::B };
    EXPECT_TRUE((set == EnumSetType { EnumSetTestFlags::A, EnumSetTestFlags::B }));
    EXPECT_TRUE((set == EnumSetType { EnumSetTestFlags::B, EnumSetTestFlags::A }));
    EXPECT_FALSE(set == EnumSetTestFlags::B);
}

TEST(WTF_EnumSet, NotEqual)
{
    using EnumSetType = EnumSet<EnumSetTestFlags>;
    EnumSetType set = EnumSetTestFlags::A;
    EXPECT_TRUE(set != EnumSetTestFlags::B);
    EXPECT_FALSE(set != EnumSetTestFlags::A);
}

TEST(WTF_EnumSet, Or)
{
    using EnumSetType = EnumSet<EnumSetTestFlags>;
    EnumSetType set { EnumSetTestFlags::A, EnumSetTestFlags::B, EnumSetTestFlags::C };
    EnumSetType set2 { EnumSetTestFlags::C, EnumSetTestFlags::D };
    EXPECT_TRUE(((set | EnumSetTestFlags::A) == EnumSetType { EnumSetTestFlags::A, EnumSetTestFlags::B, EnumSetTestFlags::C }));
    EXPECT_TRUE(((set | EnumSetTestFlags::D) == EnumSetType { EnumSetTestFlags::A, EnumSetTestFlags::B, EnumSetTestFlags::C, EnumSetTestFlags::D }));
    EXPECT_TRUE(((set | set2) == EnumSetType { EnumSetTestFlags::A, EnumSetTestFlags::B, EnumSetTestFlags::C, EnumSetTestFlags::D }));
}

TEST(WTF_EnumSet, OrAssignment)
{
    using EnumSetType = EnumSet<EnumSetTestFlags>;
    EnumSetType set { EnumSetTestFlags::A, EnumSetTestFlags::B, EnumSetTestFlags::C };

    set |= { };
    EXPECT_TRUE((set == EnumSetType { EnumSetTestFlags::A, EnumSetTestFlags::B, EnumSetTestFlags::C }));

    set |= { EnumSetTestFlags::A };
    EXPECT_TRUE((set == EnumSetType { EnumSetTestFlags::A, EnumSetTestFlags::B, EnumSetTestFlags::C }));

    set |= { EnumSetTestFlags::C, EnumSetTestFlags::D };
    EXPECT_TRUE((set == EnumSetType { EnumSetTestFlags::A, EnumSetTestFlags::B, EnumSetTestFlags::C, EnumSetTestFlags::D }));
}

TEST(WTF_EnumSet, Minus)
{
    using EnumSetType = EnumSet<EnumSetTestFlags>;
    EnumSetType set { EnumSetTestFlags::A, EnumSetTestFlags::B, EnumSetTestFlags::C };

    EXPECT_TRUE(((set - EnumSetTestFlags::A) == EnumSetType { EnumSetTestFlags::B, EnumSetTestFlags::C }));
    EXPECT_TRUE(((set - EnumSetTestFlags::D) == EnumSetType { EnumSetTestFlags::A, EnumSetTestFlags::B, EnumSetTestFlags::C }));
    EXPECT_TRUE((set - set).isEmpty());
}

TEST(WTF_EnumSet, AddAndRemove)
{
    using EnumSetType = EnumSet<EnumSetTestFlags>;
    EnumSetType set;

    set.add(EnumSetTestFlags::A);
    EXPECT_TRUE(set.contains(EnumSetTestFlags::A));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::B));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::C));

    set.add({ EnumSetTestFlags::B, EnumSetTestFlags::C });
    EXPECT_TRUE(set.contains(EnumSetTestFlags::A));
    EXPECT_TRUE(set.contains(EnumSetTestFlags::B));
    EXPECT_TRUE(set.contains(EnumSetTestFlags::C));

    set.remove(EnumSetTestFlags::B);
    EXPECT_TRUE(set.contains(EnumSetTestFlags::A));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::B));
    EXPECT_TRUE(set.contains(EnumSetTestFlags::C));

    set.remove({ EnumSetTestFlags::A, EnumSetTestFlags::C });
    EXPECT_FALSE(set.contains(EnumSetTestFlags::A));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::B));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::C));
}

TEST(WTF_EnumSet, Set)
{
    using EnumSetType = EnumSet<EnumSetTestFlags>;
    EnumSetType set;

    set.set(EnumSetTestFlags::A, true);
    EXPECT_TRUE(set.contains(EnumSetTestFlags::A));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::B));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::C));

    set.set({ EnumSetTestFlags::B, EnumSetTestFlags::C }, true);
    EXPECT_TRUE(set.contains(EnumSetTestFlags::A));
    EXPECT_TRUE(set.contains(EnumSetTestFlags::B));
    EXPECT_TRUE(set.contains(EnumSetTestFlags::C));

    set.set(EnumSetTestFlags::B, false);
    EXPECT_TRUE(set.contains(EnumSetTestFlags::A));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::B));
    EXPECT_TRUE(set.contains(EnumSetTestFlags::C));

    set.set({ EnumSetTestFlags::A, EnumSetTestFlags::C }, false);
    EXPECT_FALSE(set.contains(EnumSetTestFlags::A));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::B));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::C));
}

TEST(WTF_EnumSet, ContainsTwoFlags)
{
    using EnumSetType = EnumSet<EnumSetTestFlags>;
    EnumSetType set { EnumSetTestFlags::A, EnumSetTestFlags::B };

    EXPECT_FALSE(set.isEmpty());
    EXPECT_TRUE(set.contains(EnumSetTestFlags::A));
    EXPECT_TRUE(set.contains(EnumSetTestFlags::B));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::C));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::D));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::E));
}

TEST(WTF_EnumSet, ContainsTwoFlags2)
{
    using EnumSetType = EnumSet<EnumSetTestFlags>;
    EnumSetType set { EnumSetTestFlags::A, EnumSetTestFlags::D };

    EXPECT_FALSE(set.isEmpty());
    EXPECT_TRUE(set.contains(EnumSetTestFlags::A));
    EXPECT_TRUE(set.contains(EnumSetTestFlags::D));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::B));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::C));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::E));
}

TEST(WTF_EnumSet, ContainsTwoFlags3)
{
    using EnumSetType = EnumSet<EnumSetTestFlags>;
    EnumSetType set { EnumSetTestFlags::D, EnumSetTestFlags::E };

    EXPECT_FALSE(set.isEmpty());
    EXPECT_TRUE(set.contains(EnumSetTestFlags::D));
    EXPECT_TRUE(set.contains(EnumSetTestFlags::E));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::A));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::B));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::C));
}

TEST(WTF_EnumSet, EmptyEnumSetToRawValueToEnumSet)
{
    using EnumSetType = EnumSet<EnumSetTestFlags>;
    EnumSetType set;
    EXPECT_TRUE(set.isEmpty());
    EXPECT_FALSE(set.contains(EnumSetTestFlags::A));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::B));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::C));

    auto set2 = EnumSetType::fromRaw(set.toRaw());
    EXPECT_TRUE(set2.isEmpty());
    EXPECT_FALSE(set2.contains(EnumSetTestFlags::A));
    EXPECT_FALSE(set2.contains(EnumSetTestFlags::B));
    EXPECT_FALSE(set2.contains(EnumSetTestFlags::C));
}

TEST(WTF_EnumSet, EnumSetThatContainsOneFlagToRawValueToEnumSet)
{
    using EnumSetType = EnumSet<EnumSetTestFlags>;
    EnumSetType set = EnumSetTestFlags::A;
    EXPECT_FALSE(set.isEmpty());
    EXPECT_TRUE(set.contains(EnumSetTestFlags::A));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::B));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::C));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::D));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::E));

    auto set2 = EnumSetType::fromRaw(set.toRaw());
    EXPECT_FALSE(set2.isEmpty());
    EXPECT_TRUE(set2.contains(EnumSetTestFlags::A));
    EXPECT_FALSE(set2.contains(EnumSetTestFlags::B));
    EXPECT_FALSE(set2.contains(EnumSetTestFlags::C));
    EXPECT_FALSE(set2.contains(EnumSetTestFlags::D));
    EXPECT_FALSE(set2.contains(EnumSetTestFlags::E));
}

TEST(WTF_EnumSet, EnumSetThatContainsOneFlagToRawValueToEnumSet2)
{
    using EnumSetType = EnumSet<EnumSetTestFlags>;
    EnumSetType set = EnumSetTestFlags::E;
    EXPECT_FALSE(set.isEmpty());
    EXPECT_TRUE(set.contains(EnumSetTestFlags::E));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::A));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::B));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::C));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::D));

    auto set2 = EnumSetType::fromRaw(set.toRaw());
    EXPECT_FALSE(set2.isEmpty());
    EXPECT_TRUE(set2.contains(EnumSetTestFlags::E));
    EXPECT_FALSE(set2.contains(EnumSetTestFlags::A));
    EXPECT_FALSE(set2.contains(EnumSetTestFlags::B));
    EXPECT_FALSE(set2.contains(EnumSetTestFlags::C));
    EXPECT_FALSE(set2.contains(EnumSetTestFlags::D));
}

TEST(WTF_EnumSet, EnumSetThatContainsTwoFlagsToRawValueToEnumSet)
{
    using EnumSetType = EnumSet<EnumSetTestFlags>;
    EnumSetType set { EnumSetTestFlags::A, EnumSetTestFlags::C };
    EXPECT_FALSE(set.isEmpty());
    EXPECT_TRUE(set.contains(EnumSetTestFlags::A));
    EXPECT_TRUE(set.contains(EnumSetTestFlags::C));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::B));

    auto set2 = EnumSetType::fromRaw(set.toRaw());
    EXPECT_FALSE(set2.isEmpty());
    EXPECT_TRUE(set2.contains(EnumSetTestFlags::A));
    EXPECT_TRUE(set2.contains(EnumSetTestFlags::C));
    EXPECT_FALSE(set2.contains(EnumSetTestFlags::B));
}

TEST(WTF_EnumSet, EnumSetThatContainsTwoFlagsToRawValueToEnumSet2)
{
    using EnumSetType = EnumSet<EnumSetTestFlags>;
    EnumSetType set { EnumSetTestFlags::D, EnumSetTestFlags::E };
    EXPECT_FALSE(set.isEmpty());
    EXPECT_TRUE(set.contains(EnumSetTestFlags::D));
    EXPECT_TRUE(set.contains(EnumSetTestFlags::E));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::A));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::B));
    EXPECT_FALSE(set.contains(EnumSetTestFlags::C));

    auto set2 = EnumSetType::fromRaw(set.toRaw());
    EXPECT_FALSE(set2.isEmpty());
    EXPECT_TRUE(set2.contains(EnumSetTestFlags::D));
    EXPECT_TRUE(set2.contains(EnumSetTestFlags::E));
    EXPECT_FALSE(set2.contains(EnumSetTestFlags::A));
    EXPECT_FALSE(set2.contains(EnumSetTestFlags::B));
    EXPECT_FALSE(set2.contains(EnumSetTestFlags::C));
}

TEST(WTF_EnumSet, TwoIteratorsIntoSameEnumSet)
{
    using EnumSetType = EnumSet<EnumSetTestFlags>;
    EnumSetType set { EnumSetTestFlags::C, EnumSetTestFlags::B };
    typename EnumSetType::iterator it1 = set.begin();
    typename EnumSetType::iterator it2 = it1;
    ++it1;
    EXPECT_STRONG_ENUM_EQ(EnumSetTestFlags::C, *it1);
    EXPECT_STRONG_ENUM_EQ(EnumSetTestFlags::B, *it2);
}

TEST(WTF_EnumSet, IterateOverEnumSetThatContainsTwoFlags)
{
    using EnumSetType = EnumSet<EnumSetTestFlags>;
    EnumSetType set { EnumSetTestFlags::A, EnumSetTestFlags::C };
    typename EnumSetType::iterator it = set.begin();
    typename EnumSetType::iterator end = set.end();
    EXPECT_TRUE(it != end);
    EXPECT_STRONG_ENUM_EQ(EnumSetTestFlags::A, *it);
    ++it;
    EXPECT_STRONG_ENUM_EQ(EnumSetTestFlags::C, *it);
    ++it;
    EXPECT_TRUE(it == end);
}

TEST(WTF_EnumSet, IterateOverEnumSetThatContainsFlags2)
{
    using EnumSetType = EnumSet<EnumSetTestFlags>;
    EnumSetType set { EnumSetTestFlags::D, EnumSetTestFlags::E };
    typename EnumSetType::iterator it = set.begin();
    typename EnumSetType::iterator end = set.end();
    EXPECT_TRUE(it != end);
    EXPECT_STRONG_ENUM_EQ(EnumSetTestFlags::D, *it);
    ++it;
    EXPECT_STRONG_ENUM_EQ(EnumSetTestFlags::E, *it);
    ++it;
    EXPECT_TRUE(it == end);
}

TEST(WTF_EnumSet, NextItemAfterLargestIn32BitFlagSet)
{
    enum class ThirtyTwoBitFlags : uint32_t {
        A = 31,
    };
    using EnumSetType = EnumSet<ThirtyTwoBitFlags>;
    EnumSetType set { ThirtyTwoBitFlags::A };
    typename EnumSetType::iterator it = set.begin();
    typename EnumSetType::iterator end = set.end();
    EXPECT_TRUE(it != end);
    ++it;
    EXPECT_TRUE(it == end);
}

TEST(WTF_EnumSet, NextItemAfterLargestIn64BitFlagSet)
{
    enum class SixtyFourBitFlags : uint32_t {
        A = 63,
    };
    using EnumSetType = EnumSet<SixtyFourBitFlags>;
    EnumSetType set { SixtyFourBitFlags::A };
    typename EnumSetType::iterator it = set.begin();
    typename EnumSetType::iterator end = set.end();
    EXPECT_TRUE(it != end);
    ++it;
    EXPECT_TRUE(it == end);
}

TEST(WTF_EnumSet, IterationOrderTheSameRegardlessOfInsertionOrder)
{
    using EnumSetType = EnumSet<EnumSetTestFlags>;
    EnumSetType set1 = EnumSetTestFlags::C;
    set1.add(EnumSetTestFlags::A);

    EnumSetType set2 = EnumSetTestFlags::A;
    set2.add(EnumSetTestFlags::C);

    typename EnumSetType::iterator it1 = set1.begin();
    typename EnumSetType::iterator it2 = set2.begin();

    EXPECT_TRUE(*it1 == *it2);
    ++it1;
    ++it2;
    EXPECT_TRUE(*it1 == *it2);
}

TEST(WTF_EnumSet, OperatorAnd)
{
    using EnumSetType = EnumSet<EnumSetTestFlags>;
    EnumSetType a { EnumSetTestFlags::A };
    EnumSetType ac { EnumSetTestFlags::A, EnumSetTestFlags::C };
    EnumSetType bc { EnumSetTestFlags::B, EnumSetTestFlags::C };
    {
        auto set = a & ac;
        EXPECT_TRUE(!!set);
        EXPECT_FALSE(set.isEmpty());
        EXPECT_TRUE(set.contains(EnumSetTestFlags::A));
        EXPECT_FALSE(set.contains(EnumSetTestFlags::B));
        EXPECT_FALSE(set.contains(EnumSetTestFlags::C));
    }
    {
        auto set = a & bc;
        EXPECT_FALSE(!!set);
        EXPECT_TRUE(set.isEmpty());
        EXPECT_FALSE(set.contains(EnumSetTestFlags::A));
        EXPECT_FALSE(set.contains(EnumSetTestFlags::B));
        EXPECT_FALSE(set.contains(EnumSetTestFlags::C));
    }
    {
        auto set = ac & bc;
        EXPECT_TRUE(!!set);
        EXPECT_FALSE(set.isEmpty());
        EXPECT_FALSE(set.contains(EnumSetTestFlags::A));
        EXPECT_FALSE(set.contains(EnumSetTestFlags::B));
        EXPECT_TRUE(set.contains(EnumSetTestFlags::C));
    }
    {
        auto set = EnumSetTestFlags::A & bc;
        EXPECT_FALSE(!!set);
        EXPECT_TRUE(set.isEmpty());
        EXPECT_FALSE(set.contains(EnumSetTestFlags::A));
        EXPECT_FALSE(set.contains(EnumSetTestFlags::B));
        EXPECT_FALSE(set.contains(EnumSetTestFlags::C));
    }
    {
        auto set = EnumSetTestFlags::A & ac;
        EXPECT_TRUE(!!set);
        EXPECT_FALSE(set.isEmpty());
        EXPECT_TRUE(set.contains(EnumSetTestFlags::A));
        EXPECT_FALSE(set.contains(EnumSetTestFlags::B));
        EXPECT_FALSE(set.contains(EnumSetTestFlags::C));
    }
    {
        auto set = bc & EnumSetTestFlags::A;
        EXPECT_FALSE(!!set);
        EXPECT_TRUE(set.isEmpty());
        EXPECT_FALSE(set.contains(EnumSetTestFlags::A));
        EXPECT_FALSE(set.contains(EnumSetTestFlags::B));
        EXPECT_FALSE(set.contains(EnumSetTestFlags::C));
    }
    {
        auto set = ac & EnumSetTestFlags::A;
        EXPECT_TRUE(!!set);
        EXPECT_FALSE(set.isEmpty());
        EXPECT_TRUE(set.contains(EnumSetTestFlags::A));
        EXPECT_FALSE(set.contains(EnumSetTestFlags::B));
        EXPECT_FALSE(set.contains(EnumSetTestFlags::C));
    }
}

TEST(WTF_EnumSet, OperatorXor)
{
    using EnumSetType = EnumSet<EnumSetTestFlags>;
    EnumSetType a { EnumSetTestFlags::A };
    EnumSetType ac { EnumSetTestFlags::A, EnumSetTestFlags::C };
    EnumSetType bc { EnumSetTestFlags::B, EnumSetTestFlags::C };
    {
        auto set = a ^ ac;
        EXPECT_FALSE(set.contains(EnumSetTestFlags::A));
        EXPECT_FALSE(set.contains(EnumSetTestFlags::B));
        EXPECT_TRUE(set.contains(EnumSetTestFlags::C));
    }
    {
        auto set = a ^ bc;
        EXPECT_TRUE(set.contains(EnumSetTestFlags::A));
        EXPECT_TRUE(set.contains(EnumSetTestFlags::B));
        EXPECT_TRUE(set.contains(EnumSetTestFlags::C));
    }
    {
        auto set = ac ^ bc;
        EXPECT_TRUE(set.contains(EnumSetTestFlags::A));
        EXPECT_TRUE(set.contains(EnumSetTestFlags::B));
        EXPECT_FALSE(set.contains(EnumSetTestFlags::C));
    }
}

TEST(WTF_EnumSet, ContainsAny)
{
    using EnumSetType = EnumSet<EnumSetTestFlags>;
    EnumSetType set { EnumSetTestFlags::A, EnumSetTestFlags::B };

    EXPECT_TRUE(set.containsAny({ EnumSetTestFlags::A }));
    EXPECT_TRUE(set.containsAny({ EnumSetTestFlags::B }));
    EXPECT_FALSE(set.containsAny({ EnumSetTestFlags::C }));
    EXPECT_FALSE(set.containsAny({ EnumSetTestFlags::C, EnumSetTestFlags::D }));
    EXPECT_TRUE(set.containsAny({ EnumSetTestFlags::A, EnumSetTestFlags::B }));
    EXPECT_TRUE(set.containsAny({ EnumSetTestFlags::B, EnumSetTestFlags::C }));
    EXPECT_TRUE(set.containsAny({ EnumSetTestFlags::A, EnumSetTestFlags::C }));
    EXPECT_TRUE(set.containsAny({ EnumSetTestFlags::A, EnumSetTestFlags::B, EnumSetTestFlags::C }));
}

TEST(WTF_EnumSet, ContainsAll)
{
    using EnumSetType = EnumSet<EnumSetTestFlags>;
    EnumSetType set { EnumSetTestFlags::A, EnumSetTestFlags::B };

    EXPECT_TRUE(set.containsAll({ EnumSetTestFlags::A }));
    EXPECT_TRUE(set.containsAll({ EnumSetTestFlags::B }));
    EXPECT_FALSE(set.containsAll({ EnumSetTestFlags::C }));
    EXPECT_FALSE(set.containsAll({ EnumSetTestFlags::C, EnumSetTestFlags::D }));
    EXPECT_TRUE(set.containsAll({ EnumSetTestFlags::A, EnumSetTestFlags::B }));
    EXPECT_FALSE(set.containsAll({ EnumSetTestFlags::B, EnumSetTestFlags::C }));
    EXPECT_FALSE(set.containsAll({ EnumSetTestFlags::A, EnumSetTestFlags::C }));
    EXPECT_FALSE(set.containsAll({ EnumSetTestFlags::A, EnumSetTestFlags::B, EnumSetTestFlags::C }));
}

TEST(WTF_EnumSet, ToSingleValue)
{
    using EnumSetType = EnumSet<EnumSetTestFlags>;
    EnumSetType set { EnumSetTestFlags::D };

    EXPECT_TRUE(set.toSingleValue());
    EXPECT_TRUE(set.toSingleValue().value() == EnumSetTestFlags::D);

    set.add(EnumSetTestFlags::A);

    EXPECT_FALSE(set.toSingleValue());

    set.remove(EnumSetTestFlags::D);

    EXPECT_TRUE(set.toSingleValue());
    EXPECT_TRUE(set.toSingleValue().value() == EnumSetTestFlags::A);

    set = { };

    EXPECT_FALSE(set.toSingleValue());
}

TEST(WTF_EnumSet, Size)
{
    using EnumSetType = EnumSet<EnumSetTestFlags>;
    EnumSetType set;

    EXPECT_EQ(set.size(), 0u);
    set.add({ EnumSetTestFlags::A, EnumSetTestFlags::D });
    EXPECT_EQ(set.size(), 2u);
    set.remove(EnumSetTestFlags::A);
    EXPECT_EQ(set.size(), 1u);
}

TEST(WTF_EnumSet, StorageSize)
{
    {
        enum class Enum : uint8_t {
            A,
        };
        EXPECT_EQ(sizeof(EnumSet<Enum>::StorageType), 8u);
    }
    {
        enum class Enum : uint8_t {
            A,
            HighestEnumValue = A
        };
        EXPECT_EQ(sizeof(EnumSet<Enum>::StorageType), 1u);
    }
    {
        enum class Enum : uint8_t {
            A = 7,
            HighestEnumValue = A
        };
        EXPECT_EQ(sizeof(EnumSet<Enum>::StorageType), 1u);
    }
    {
        enum class Enum : uint8_t {
            A = 8,
            HighestEnumValue = A
        };
        EXPECT_EQ(sizeof(EnumSet<Enum>::StorageType), 2u);
    }
    {
        enum class Enum : uint8_t {
            A = 15,
            HighestEnumValue = A
        };
        EXPECT_EQ(sizeof(EnumSet<Enum>::StorageType), 2u);
    }
    {
        enum class Enum : uint8_t {
            A = 16,
            HighestEnumValue = A
        };
        EXPECT_EQ(sizeof(EnumSet<Enum>::StorageType), 4u);
    }
    {
        enum class Enum : uint8_t {
            A = 31,
            HighestEnumValue = A
        };
        EXPECT_EQ(sizeof(EnumSet<Enum>::StorageType), 4u);
    }
    {
        enum class Enum : uint8_t {
            A = 32,
            HighestEnumValue = A
        };
        EXPECT_EQ(sizeof(EnumSet<Enum>::StorageType), 8u);
    }
    {
        enum class Enum : uint8_t {
            A = 63,
            HighestEnumValue = A
        };
        EXPECT_EQ(sizeof(EnumSet<Enum>::StorageType), 8u);
    }
}

} // namespace TestWebKitAPI
