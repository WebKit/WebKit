/*
 * Copyright (C) 2016-2024 Apple Inc. All rights reserved.
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

#include <wtf/Deque.h>
#include <wtf/EnumTraits.h>
#include <wtf/text/ASCIILiteral.h>

enum class TestEnum {
    A,
    B,
    C,
};

enum class TestNonContiguousEnum {
    A = 0,
    B = 1,
    C = 3,
};

enum class TestNonZeroBasedEnum {
    A = 1,
    B = 2,
    C = 3,
};

namespace WTF {
template<> struct EnumTraits<TestEnum> {
    using values = EnumValues<TestEnum, TestEnum::A, TestEnum::B, TestEnum::C>;
};
template<> struct EnumTraits<TestNonContiguousEnum> {
    using values = EnumValues<TestNonContiguousEnum, TestNonContiguousEnum::A, TestNonContiguousEnum::B, TestNonContiguousEnum::C>;
};
template<> struct EnumTraits<TestNonZeroBasedEnum> {
    using values = EnumValues<TestNonZeroBasedEnum, TestNonZeroBasedEnum::A, TestNonZeroBasedEnum::B, TestNonZeroBasedEnum::C>;
};
}

namespace TestWebKitAPI {

TEST(WTF_EnumTraits, IsValidEnum)
{
    EXPECT_TRUE(isValidEnum<TestEnum>(0));
    EXPECT_FALSE(isValidEnum<TestEnum>(-1));
    EXPECT_FALSE(isValidEnum<TestEnum>(3));
}

TEST(WTF_EnumTraits, ValuesTraits)
{
    EXPECT_EQ(WTF::EnumTraits<TestEnum>::values::max, TestEnum::C);
    EXPECT_EQ(WTF::EnumTraits<TestEnum>::values::min, TestEnum::A);
    EXPECT_EQ(WTF::EnumTraits<TestEnum>::values::count, 3UL);
    EXPECT_NE(WTF::EnumTraits<TestEnum>::values::max, TestEnum::A);
    EXPECT_NE(WTF::EnumTraits<TestEnum>::values::min, TestEnum::C);
    EXPECT_NE(WTF::EnumTraits<TestEnum>::values::count, 4UL);

    Deque<TestEnum> expectedValues = { TestEnum::A, TestEnum::B, TestEnum::C };
    WTF::EnumTraits<TestEnum>::values::forEach([&] (auto value) {
        EXPECT_EQ(value, expectedValues.takeFirst());
    });
    EXPECT_EQ(expectedValues.size(), 0UL);
}

TEST(WTF_EnumTraits, ZeroBasedContiguousEnum)
{
    static_assert(isZeroBasedContiguousEnum<TestEnum>());
    EXPECT_TRUE(isZeroBasedContiguousEnum<TestEnum>());
    static_assert(!isZeroBasedContiguousEnum<TestNonContiguousEnum>());
    EXPECT_FALSE(isZeroBasedContiguousEnum<TestNonContiguousEnum>());
    static_assert(!isZeroBasedContiguousEnum<TestNonZeroBasedEnum>());
    EXPECT_FALSE(isZeroBasedContiguousEnum<TestNonZeroBasedEnum>());
}

static bool isExpectedEnumString(const ASCIILiteral& expected, const std::span<const char>& result)
{
    // result won't have a null terminator.
    bool equal = true;
    for (size_t i = 0; i < result.size(); ++i)
        equal &= expected[i] == result[i];
    return equal;
}

enum NonClassMultiWord {
    FooBar,
    BazBloop,
    WordW1thNumb3rs,
    Hole = WordW1thNumb3rs + 2,
    Duplicate = Hole,
};

enum class ClassMultiWord {
    FooBar,
    BazBloop,
    WordW1thNumb3rs,
    Hole = WordW1thNumb3rs + 2,
    Duplicate = Hole,
};

TEST(WTF_EnumTraits, EnumNameTemplate)
{
    EXPECT_TRUE(isExpectedEnumString("NonClassMultiWord"_s, enumTypeName<NonClassMultiWord>()));
    EXPECT_TRUE(isExpectedEnumString("FooBar"_s, enumName<FooBar>()));
    EXPECT_TRUE(isExpectedEnumString("WordW1thNumb3rs"_s, enumName<WordW1thNumb3rs>()));
    EXPECT_TRUE(isExpectedEnumString("Hole"_s, enumName<Hole>()));
    EXPECT_TRUE(isExpectedEnumString("Hole"_s, enumName<Duplicate>()));

    EXPECT_TRUE(isExpectedEnumString("ClassMultiWord"_s, enumTypeName<ClassMultiWord>()));
    EXPECT_TRUE(isExpectedEnumString("FooBar"_s, enumName<ClassMultiWord::FooBar>()));
    EXPECT_TRUE(isExpectedEnumString("WordW1thNumb3rs"_s, enumName<ClassMultiWord::WordW1thNumb3rs>()));
    EXPECT_TRUE(isExpectedEnumString("Hole"_s, enumName<ClassMultiWord::Hole>()));
    EXPECT_TRUE(isExpectedEnumString("Hole"_s, enumName<ClassMultiWord::Duplicate>()));
}

TEST(WTF_EnumTraits, EnumNameArgument)
{
    EXPECT_TRUE(isExpectedEnumString("FooBar"_s, enumName(FooBar)));
    EXPECT_TRUE(isExpectedEnumString("WordW1thNumb3rs"_s, enumName(WordW1thNumb3rs)));
    EXPECT_TRUE(isExpectedEnumString("Hole"_s, enumName(Hole)));
    EXPECT_TRUE(isExpectedEnumString("Hole"_s, enumName(Duplicate)));

    EXPECT_TRUE(isExpectedEnumString("FooBar"_s, enumName(ClassMultiWord::FooBar)));
    EXPECT_TRUE(isExpectedEnumString("WordW1thNumb3rs"_s, enumName(ClassMultiWord::WordW1thNumb3rs)));
    EXPECT_TRUE(isExpectedEnumString("Hole"_s, enumName(ClassMultiWord::Hole)));
    EXPECT_TRUE(isExpectedEnumString("Hole"_s, enumName(ClassMultiWord::Duplicate)));
}

} // namespace TestWebKitAPI
