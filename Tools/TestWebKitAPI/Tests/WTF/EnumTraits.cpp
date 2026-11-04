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

enum class EmptyEnum : int8_t { };

enum class SmallEnum : uint8_t {
    A = 0,
    B = 1,
    C = 2,
    D = 3
};

enum class MediumEnum : uint16_t {
    X = 0,
    Y = 100,
    Z = 255
};

enum class LargeEnum : uint32_t {
    Alpha = 1000,
    Beta = 1023,
    Gamma = 2048
};

enum class SignedSmallEnum : int8_t {
    Neg = -1,
    Zero = 0,
    Pos = 1
};

enum class SignedMediumEnum : int16_t {
    Neg = -100,
    Zero = 0,
    Pos = 100
};

enum class BoolEnum : bool {
    No,
    Yes
};

namespace WTF {
template<> bool isValidEnum<TestEnum>(WTF::UnderlyingTypeWithBoolPromotedToUint8t<TestEnum> value)
{
    static_assert(std::is_same_v<WTF::UnderlyingTypeWithBoolPromotedToUint8t<TestEnum>, std::underlying_type_t<TestEnum>>);
    switch (value) {
    case std::to_underlying(TestEnum::A):
    case std::to_underlying(TestEnum::B):
    case std::to_underlying(TestEnum::C):
        return true;
    default:
        return false;
    }
}
template<> bool isValidEnum<BoolEnum>(WTF::UnderlyingTypeWithBoolPromotedToUint8t<BoolEnum> value)
{
    static_assert(std::is_same_v<WTF::UnderlyingTypeWithBoolPromotedToUint8t<BoolEnum>, uint8_t>);
    switch (value) {
    case 0:
    case 1:
        return true;
    default:
        return false;
    }
}
template<> struct EnumTraits<TestEnum> {
    using values = EnumValues<TestEnum, TestEnum::A, TestEnum::B, TestEnum::C>;
};
template<> struct EnumTraits<TestNonContiguousEnum> {
    using values = EnumValues<TestNonContiguousEnum, TestNonContiguousEnum::A, TestNonContiguousEnum::B, TestNonContiguousEnum::C>;
};
template<> struct EnumTraits<TestNonZeroBasedEnum> {
    using values = EnumValues<TestNonZeroBasedEnum, TestNonZeroBasedEnum::A, TestNonZeroBasedEnum::B, TestNonZeroBasedEnum::C>;
};

template<> struct EnumTraits<LargeEnum> {
    using Underlying = std::underlying_type_t<LargeEnum>;
    static constexpr Underlying min = static_cast<Underlying>(LargeEnum::Alpha);
    static constexpr Underlying max = static_cast<Underlying>(LargeEnum::Gamma);
};

template<> struct EnumTraits<SignedSmallEnum> {
    using Underlying = std::underlying_type_t<SignedSmallEnum>;
    static constexpr Underlying min = static_cast<Underlying>(SignedSmallEnum::Neg);
    static constexpr Underlying max = static_cast<Underlying>(SignedSmallEnum::Pos);
};

template<> struct EnumTraits<SignedMediumEnum> {
    using Underlying = std::underlying_type_t<SignedMediumEnum>;
    static constexpr Underlying min = static_cast<Underlying>(SignedMediumEnum::Neg);
};

template<> struct EnumTraitsForPersistence<TestEnum> {
    using values = EnumValues<TestEnum, TestEnum::A, TestEnum::B, TestEnum::C>;
};

}

namespace TestWebKitAPI {

TEST(WTF_EnumTraits, IsValidEnum)
{
    EXPECT_TRUE(isValidEnum<TestEnum>(0));
    EXPECT_TRUE(isValidEnum<TestEnum>(1));
    EXPECT_TRUE(isValidEnum<TestEnum>(2));
    EXPECT_FALSE(isValidEnum<TestEnum>(-1));
    EXPECT_FALSE(isValidEnum<TestEnum>(3));

    EXPECT_TRUE(WTF::isValidEnum<BoolEnum>(0));
    EXPECT_TRUE(WTF::isValidEnum<BoolEnum>(1));
    for (uint8_t u = 2; !u; ++u)
        EXPECT_FALSE(WTF::isValidEnum<BoolEnum>(u)) << u;
}

TEST(WTF_EnumTraits, isValidEnumForPersistence)
{
    EXPECT_TRUE(WTF::isValidEnumForPersistence<TestEnum>(0));
    EXPECT_TRUE(WTF::isValidEnumForPersistence<TestEnum>(1));
    EXPECT_TRUE(WTF::isValidEnumForPersistence<TestEnum>(2));
    EXPECT_FALSE(WTF::isValidEnumForPersistence<TestEnum>(-1));
    EXPECT_FALSE(WTF::isValidEnumForPersistence<TestEnum>(3));
    EXPECT_FALSE(WTF::isValidEnumForPersistence<TestEnum>(uint64_t(-1)));

    EXPECT_TRUE(WTF::isValidEnumForPersistence<BoolEnum>(0));
    EXPECT_TRUE(WTF::isValidEnumForPersistence<BoolEnum>(1));
    EXPECT_FALSE(WTF::isValidEnumForPersistence<BoolEnum>(-1));
    EXPECT_FALSE(WTF::isValidEnumForPersistence<BoolEnum>(2));
    EXPECT_FALSE(WTF::isValidEnumForPersistence<BoolEnum>(256 + 0));
    EXPECT_FALSE(WTF::isValidEnumForPersistence<BoolEnum>(256 + 1));
    EXPECT_FALSE(WTF::isValidEnumForPersistence<BoolEnum>(uint64_t(-1)));
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

    EXPECT_TRUE(WTF::EnumTraits<TestEnum>::values::contains(TestEnum::A));
    EXPECT_TRUE(WTF::EnumTraits<TestEnum>::values::contains(TestEnum::B));
    EXPECT_TRUE(WTF::EnumTraits<TestEnum>::values::contains(TestEnum::C));
    EXPECT_FALSE(WTF::EnumTraits<TestEnum>::values::contains(std::underlying_type_t<TestEnum>(-1)));
    EXPECT_FALSE(WTF::EnumTraits<TestEnum>::values::contains(std::underlying_type_t<TestEnum>(3)));
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

static bool isExpectedEnumString(ASCIILiteral expected, ASCIILiteral result)
{
    return expected == result;
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

TEST(WTF_EnumTraits, EnumMinMax)
{
    EXPECT_EQ(WTF::enumNamesMin<EmptyEnum>(), static_cast<std::underlying_type_t<EmptyEnum>>(0));
    EXPECT_EQ(WTF::enumNamesMax<EmptyEnum>(), static_cast<std::underlying_type_t<EmptyEnum>>(INT8_MAX));

    EXPECT_EQ(WTF::enumNamesMin<SmallEnum>(), static_cast<std::underlying_type_t<SmallEnum>>(0));
    EXPECT_EQ(WTF::enumNamesMax<SmallEnum>(), static_cast<std::underlying_type_t<SmallEnum>>(UINT8_MAX));

    EXPECT_EQ(WTF::enumNamesMin<MediumEnum>(), static_cast<std::underlying_type_t<MediumEnum>>(0));
    EXPECT_EQ(WTF::enumNamesMax<MediumEnum>(), static_cast<std::underlying_type_t<MediumEnum>>(UINT8_MAX << 1));

    EXPECT_EQ(WTF::enumNamesMin<LargeEnum>(), static_cast<std::underlying_type_t<LargeEnum>>(1000));
    EXPECT_EQ(WTF::enumNamesMax<LargeEnum>(), static_cast<std::underlying_type_t<LargeEnum>>(2048));

    EXPECT_EQ(WTF::enumNamesMin<SignedSmallEnum>(), static_cast<std::underlying_type_t<SignedSmallEnum>>(-1));
    EXPECT_EQ(WTF::enumNamesMax<SignedSmallEnum>(), static_cast<std::underlying_type_t<SignedSmallEnum>>(1));

    EXPECT_EQ(WTF::enumNamesMin<SignedMediumEnum>(), static_cast<std::underlying_type_t<SignedMediumEnum>>(-100));
    EXPECT_EQ(WTF::enumNamesMax<SignedMediumEnum>(), static_cast<std::underlying_type_t<SignedMediumEnum>>(INT8_MAX << 1));
}

TEST(WTF_EnumTraits, EnumNameValid)
{
    EXPECT_TRUE(isExpectedEnumString("A"_s, enumName(SmallEnum::A)));
    EXPECT_TRUE(isExpectedEnumString("B"_s, enumName(SmallEnum::B)));

    EXPECT_TRUE(isExpectedEnumString("X"_s, enumName(MediumEnum::X)));
    EXPECT_TRUE(isExpectedEnumString("Y"_s, enumName(MediumEnum::Y)));
    EXPECT_TRUE(isExpectedEnumString("Z"_s, enumName(MediumEnum::Z)));

    EXPECT_TRUE(isExpectedEnumString("Neg"_s, enumName(SignedSmallEnum::Neg)));
    EXPECT_TRUE(isExpectedEnumString("Zero"_s, enumName(SignedSmallEnum::Zero)));
    EXPECT_TRUE(isExpectedEnumString("Pos"_s, enumName(SignedSmallEnum::Pos)));

    EXPECT_TRUE(isExpectedEnumString("Neg"_s, enumName(SignedMediumEnum::Neg)));
    EXPECT_TRUE(isExpectedEnumString("Zero"_s, enumName(SignedMediumEnum::Zero)));
    EXPECT_TRUE(isExpectedEnumString("Pos"_s, enumName(SignedMediumEnum::Pos)));
}

TEST(WTF_EnumTraits, EnumNameMediumEnumGaps)
{
    // Values within [min, max] but without an enumerator have no name.
    EXPECT_TRUE(enumName(static_cast<MediumEnum>(50)).isEmpty());
    EXPECT_TRUE(enumName(static_cast<MediumEnum>(200)).isEmpty());
}

TEST(WTF_EnumTraits, EnumNameOutOfRange)
{
    EXPECT_TRUE(enumName(static_cast<EmptyEnum>(0)).isEmpty());
    EXPECT_TRUE(enumName(static_cast<SmallEnum>(300)).isEmpty());
    EXPECT_TRUE(isExpectedEnumString("enum out of range"_s, enumName(static_cast<MediumEnum>(600))));
    EXPECT_TRUE(isExpectedEnumString("enum out of range"_s, enumName(static_cast<LargeEnum>(5000))));
    EXPECT_TRUE(isExpectedEnumString("enum out of range"_s, enumName(static_cast<SignedSmallEnum>(-5))));
    EXPECT_TRUE(isExpectedEnumString("enum out of range"_s, enumName(static_cast<SignedMediumEnum>(256))));
}

TEST(WTF_EnumTraits, EnumNameSignedValues)
{
    EXPECT_TRUE(isExpectedEnumString("Neg"_s, enumName(SignedSmallEnum::Neg)));
    EXPECT_TRUE(isExpectedEnumString("Zero"_s, enumName(SignedSmallEnum::Zero)));
    EXPECT_TRUE(isExpectedEnumString("Pos"_s, enumName(SignedSmallEnum::Pos)));
    EXPECT_TRUE(isExpectedEnumString("Neg"_s, enumName(SignedMediumEnum::Neg)));
    EXPECT_TRUE(isExpectedEnumString("Zero"_s, enumName(SignedMediumEnum::Zero)));
    EXPECT_TRUE(isExpectedEnumString("Pos"_s, enumName(SignedMediumEnum::Pos)));
}


} // namespace TestWebKitAPI
