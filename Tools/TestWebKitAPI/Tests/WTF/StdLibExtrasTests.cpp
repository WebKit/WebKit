/*
 * Copyright (C) 2020-2024 Apple Inc. All rights reserved.
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

#include <array>
#include <string>
#include <tuple>
#include <utility>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/MathExtras.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

#if !USE(SYSTEM_MALLOC)
#include <bmalloc/Algorithm.h>
#endif
namespace TestWebKitAPI {

template<typename WordType>
void testFindBitInWord()
{
    constexpr size_t bitsInWord = sizeof(WordType) * 8;
    constexpr size_t numberOfShiftValues = bitsInWord + 1;
    constexpr size_t testPermutationsPerShift = 100;

    constexpr size_t numberOfTestValues = numberOfShiftValues * testPermutationsPerShift;
    uint8_t startIndex[numberOfTestValues];
    uint8_t endIndex[numberOfTestValues];

    auto initTestValues = [&] () {
        // Set some internal and boundary cases.
        uint8_t specialCases[] = {
            0,
            bitsInWord / 2,
            bitsInWord - 1,
            bitsInWord,
        };

        constexpr size_t numberOfSpecialCases = sizeof(specialCases) / sizeof(specialCases[0]);

        size_t nextTestValue = 0;
        for (size_t i = 0; i < numberOfSpecialCases; ++i) {
            for (size_t j = 0; j < numberOfSpecialCases; ++j) {
                startIndex[nextTestValue] = specialCases[i];
                endIndex[nextTestValue++] = specialCases[j];
            }
        }

        // Fill in some random cases.
        for (size_t i = nextTestValue; i < numberOfTestValues; ++i) {
            startIndex[i] = static_cast<uint8_t>(cryptographicallyRandomUnitInterval() * bitsInWord);
            uint8_t remainingBits = bitsInWord - startIndex[i];
            endIndex[i] = static_cast<uint8_t>(cryptographicallyRandomUnitInterval() * remainingBits) + startIndex[i];
        }
    };

    auto expectedBitInWord = [] (WordType word, size_t& index, size_t endIndex, bool value) -> bool {
        word >>= index;
        while (index < endIndex) {
            if ((word & 1) == static_cast<WordType>(value))
                return true;
            index++;
            word >>= 1;
        }
        index = endIndex;
        return false;
    };

    auto test = [&] (bool value, size_t shift) {
        constexpr uint64_t baseWord = std::numeric_limits<uint64_t>::max();
        uint64_t word = (shift < bitsInWord) ? baseWord << shift : 0;

        for (size_t i = 0; i < numberOfTestValues; ++i) {
            size_t index = startIndex[i];
            bool result = findBitInWord(word, index, endIndex[i], value);

            size_t expectedIndex = startIndex[i];
            bool expectedResult = expectedBitInWord(word, expectedIndex, endIndex[i], value);

            ASSERT_EQ(result, expectedResult);
            ASSERT_EQ(index, expectedIndex);
        }
    };

    initTestValues();

    // Testing find a set bit.
    for (size_t i = 0; i < numberOfShiftValues; ++i)
        test(true, i);

    // Testing find a cleared bit.
    for (size_t i = 0; i < numberOfShiftValues; ++i)
        test(false, i);
}

TEST(WTF_StdLibExtras, findBitInWord_uint32_t) { testFindBitInWord<uint32_t>(); }
TEST(WTF_StdLibExtras, findBitInWord_uint64_t) { testFindBitInWord<uint64_t>(); }

// Tests that function-local types can be instantiated with makeUnique.
// Style check would complain about use of std::make_unique, enforcing use of
// makeUnique. The makeUnique needs WTF_DEPRECATED_..._DEPRECATED_MAKE_FAST_ALLOCATED.
// There used to be a warn-unused-typedef errors when using these.
TEST(WTF_StdLibExtras, MakeUniqueFunctionLocalTypeCompiles)
{
    struct LocalStruct {
        WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(LocalStruct);
    };
    IGNORE_CLANG_WARNINGS_BEGIN("unused-local-typedef")
    class LocalClass {
        WTF_DEPRECATED_MAKE_FAST_ALLOCATED(LocalClass);
    };
    IGNORE_CLANG_WARNINGS_END
    auto s = makeUnique<LocalStruct>();
    auto c = makeUnique<LocalClass>();
}

TEST(WTF_StdLibExtras, RoundUpToMultipleOfWorks)
{
    EXPECT_EQ(2u, roundUpToMultipleOf(static_cast<uint8_t>(2), static_cast<uint8_t>(1)));
    EXPECT_EQ(254u, roundUpToMultipleOf(static_cast<uint8_t>(2), static_cast<uint8_t>(254)));
}

TEST(WTF_StdLibExtras, RoundUpToMultipleOfNonPowerOfTwoWorks)
{
    {
        uint16_t x = 65534;
        uint16_t divisor = 7;
        EXPECT_EQ(x, roundUpToMultipleOfNonPowerOfTwo(divisor, x));
        EXPECT_EQ(x, roundUpToMultipleOfNonPowerOfTwo(CheckedUint16 { divisor }, CheckedUint16 { x }));
    }
    {
        uint32_t x = std::numeric_limits<uint32_t>::max() - 2;
        uint32_t divisor = 9241;
        EXPECT_EQ(x, roundUpToMultipleOfNonPowerOfTwo(divisor, x));
        EXPECT_EQ(x, roundUpToMultipleOfNonPowerOfTwo(CheckedUint32 { divisor }, CheckedUint32 { x }));
    }
    {
        uint64_t x = std::numeric_limits<uint64_t>::max() - 2;
        uint64_t divisor = 13;
        EXPECT_EQ(x, roundUpToMultipleOfNonPowerOfTwo(divisor, x));
        EXPECT_EQ(x, roundUpToMultipleOfNonPowerOfTwo(CheckedUint64 { divisor }, CheckedUint64 { x }));
    }

#if !USE(SYSTEM_MALLOC)
    // Test that bmalloc::roundUpToMultipleOfNonPowerOfTwo does not have the bug. The function uses size_t.
    {
        size_t x = std::numeric_limits<size_t>::max() - 2;
        size_t divisor = sizeof(size_t) == 4 ? 9241 : 13;
        EXPECT_EQ(x, roundUpToMultipleOfNonPowerOfTwo(divisor, x));
        EXPECT_EQ(x, roundUpToMultipleOfNonPowerOfTwo(CheckedSize { divisor }, CheckedSize { x }));
        EXPECT_EQ(x, bmalloc::roundUpToMultipleOfNonPowerOfTwo(divisor, x));
    }
#endif

    {
        size_t x = std::numeric_limits<size_t>::max();
        EXPECT_TRUE(roundUpToMultipleOfNonPowerOfTwo(CheckedSize { 2 }, CheckedSize { x }).hasOverflowed());
    }

    EXPECT_TRUE(roundUpToMultipleOfNonPowerOfTwo(CheckedSize { WTF::ResultOverflowed }, CheckedSize { 78 }).hasOverflowed());
    EXPECT_TRUE(roundUpToMultipleOfNonPowerOfTwo(CheckedSize { 78 }, CheckedSize { WTF::ResultOverflowed }).hasOverflowed());

}

TEST(WTF_StdLibExtras, ByteCast)
{
    uint8_t u8 = 0;
    const uint8_t cu8 = 0;
    std::span su8 = { &u8, 1 };
    std::span scu8 = { &cu8, 1 };
    static_assert(std::same_as<char, decltype(byteCast<char>(u8))>);
    static_assert(std::same_as<char8_t, decltype(byteCast<char8_t>(u8))>);
    static_assert(std::same_as<std::byte, decltype(byteCast<std::byte>(u8))>);
    static_assert(std::same_as<char, decltype(byteCast<char>(cu8))>);
    static_assert(std::same_as<char8_t, decltype(byteCast<char8_t>(cu8))>);
    static_assert(std::same_as<std::byte, decltype(byteCast<std::byte>(cu8))>);
    static_assert(std::same_as<char*, decltype(byteCast<char>(&u8))>);
    static_assert(std::same_as<char8_t*, decltype(byteCast<char8_t>(&u8))>);
    static_assert(std::same_as<std::byte*, decltype(byteCast<std::byte>(&u8))>);
    static_assert(std::same_as<const char*, decltype(byteCast<char>(&cu8))>);
    static_assert(std::same_as<const char8_t*, decltype(byteCast<char8_t>(&cu8))>);
    static_assert(std::same_as<const std::byte*, decltype(byteCast<std::byte>(&cu8))>);
    static_assert(std::same_as<std::span<char>, decltype(byteCast<char>(su8))>);
    static_assert(std::same_as<std::span<char8_t>, decltype(byteCast<char8_t>(su8))>);
    static_assert(std::same_as<std::span<std::byte>, decltype(byteCast<std::byte>(su8))>);
    static_assert(std::same_as<std::span<const char>, decltype(byteCast<char>(scu8))>);
    static_assert(std::same_as<std::span<const char8_t>, decltype(byteCast<char8_t>(scu8))>);
    static_assert(std::same_as<std::span<const std::byte>, decltype(byteCast<std::byte>(scu8))>);
}

TEST(WTF_StdLibExtras, SpanReinterpretCast_DynamicExtent)
{
    Vector<int32_t> signedInt { -3, -2, -1, 0, 1, 2 };
    auto signedIntSpan = signedInt.span();
    static_assert(std::same_as<std::span<const int32_t, std::dynamic_extent>, decltype(signedIntSpan)>);

    // Cast from 4 bytes to 1 byte per item.
    auto unsignedIntByteSpan = asByteSpan(signedIntSpan);
    static_assert(std::same_as<std::span<const uint8_t, std::dynamic_extent>, decltype(unsignedIntByteSpan)>);
    EXPECT_TRUE(!memcmp(signedIntSpan.data(), unsignedIntByteSpan.data(), unsignedIntByteSpan.size_bytes()));

    // Cast from 4 bytes to 4 bytes per item.
    auto unsignedIntSpan = spanReinterpretCast<const uint32_t>(signedIntSpan);
    static_assert(std::same_as<std::span<const uint32_t, std::dynamic_extent>, decltype(unsignedIntSpan)>);
    EXPECT_TRUE(!memcmp(signedIntSpan.data(), unsignedIntSpan.data(), unsignedIntSpan.size_bytes()));

    // Cast from 4 bytes to 8 bytes per item.
    auto unsignedLongSpan = spanReinterpretCast<const uint64_t>(signedIntSpan);
    static_assert(std::same_as<std::span<const uint64_t, std::dynamic_extent>, decltype(unsignedLongSpan)>);
    EXPECT_TRUE(!memcmp(signedIntSpan.data(), unsignedLongSpan.data(), unsignedLongSpan.size_bytes()));
}

/*
TEST(WTF_StdLibExtras, SpanReinterpretCast_DynamicExtent_ManualRuntimeErrors)
{
    // Manual tests to check this release assert in WTF::spanReinterpretCast<>.
    // if constexpr (sizeof(U) < sizeof(T) || sizeof(U) % sizeof(T))
    //     RELEASE_ASSERT_WITH_MESSAGE(!(span.size_bytes() % sizeof(T)), "spanReinterpretCast will not change size in bytes from source");

    // Cast from 1 byte per item to 4 bytes per item, with non-multiple of 4.
    // Test 'sizeof(U) < sizeof(T)' results in a runtime crash.
    //Vector<int8_t> signedIntByte { 1, 2, 3, 4, 5, 6, 7 };
    //auto signedIntByteSpan = signedIntByte.span();
    //(void)spanReinterpretCast<const int>(signedIntByteSpan);

    // Cast from 4 bytes per item to 3 bytes per item, with non-multiple of 3.
    // Test that '(sizeof(U) > sizeof(T)) && (sizeof(U) % sizeof(T))' results in a runtime crash.
    //Vector<int32_t> signedInt { 1, 2, 3, 4 };
    //auto signedIntSpan = signedInt.span();
    //(void)spanReinterpretCast<const int8_t[3]>(signedIntSpan);
}
*/

TEST(WTF_StdLibExtras, SpanReinterpretCast_NonDynamicExtent)
{
    Vector<int32_t> signedInt { -3, -2, -1, 0, 1, 2 };
    std::span<const int32_t, 6> signedIntSpan { signedInt.span() };
    static_assert(std::same_as<std::span<const int32_t, 6>, decltype(signedIntSpan)>);

    // Cast from 4 bytes to 1 byte per item.
    auto unsignedIntByteSpan = asByteSpan(signedIntSpan);
    static_assert(std::same_as<std::span<const uint8_t>, decltype(unsignedIntByteSpan)>);
    EXPECT_TRUE(!memcmp(signedIntSpan.data(), unsignedIntByteSpan.data(), unsignedIntByteSpan.size_bytes()));

    // Cast from 4 bytes to 4 bytes per item.
    auto unsignedIntSpan = spanReinterpretCast<const uint32_t>(signedIntSpan);
    static_assert(std::same_as<std::span<const uint32_t, 6>, decltype(unsignedIntSpan)>);
    EXPECT_TRUE(!memcmp(signedIntSpan.data(), unsignedIntSpan.data(), unsignedIntSpan.size_bytes()));

    // Cast from 4 bytes to 8 bytes per item.
    auto unsignedLongSpan = spanReinterpretCast<const uint64_t>(signedIntSpan);
    static_assert(std::same_as<std::span<const uint64_t, 3>, decltype(unsignedLongSpan)>);
    EXPECT_TRUE(!memcmp(signedIntSpan.data(), unsignedLongSpan.data(), unsignedLongSpan.size_bytes()));
}

/*
TEST(WTF_StdLibExtras, SpanReinterpretCast_NonDynamicExtent_CompileTimeErros)
{
    // Manual tests to check this static assert in WTF::spanReinterpretCast<>.
    //     static_assert(!((sizeof(U) * Extent) % sizeof(T)), "spanReinterpretCast will not change size in bytes from source");

    // Cast from 1 byte per item to 4 bytes per item, with non-multiple of 4.
    //Vector<int8_t> signedIntByte { 1, 2, 3, 4, 5, 6, 7 };
    //std::span<const int8_t, 7> signedIntByteSpan { signedIntByte.data(), signedIntByte.size() };
    //(void)spanReinterpretCast<const int>(signedIntByteSpan);

    // Cast from 4 bytes per item to 3 bytes per item, with non-multiple of 3.
    //Vector<int32_t> signedInt { 1, 2, 3, 4 };
    //std::span<const int32_t, 4> signedIntSpan { signedInt.data(), signedInt.size() };
    //(void)spanReinterpretCast<const int8_t[3]>(signedIntSpan);
}
*/

TEST(WTF_StdLibExtras, HoldsAlternativeByIndex)
{
    using V = Variant<int, double, float>;

    V intVariant = 42;
    EXPECT_TRUE((WTF::holdsAlternative<0>(intVariant)));
    EXPECT_FALSE((WTF::holdsAlternative<1>(intVariant)));
    EXPECT_FALSE((WTF::holdsAlternative<2>(intVariant)));

    V doubleVariant = 3.14;
    EXPECT_FALSE((WTF::holdsAlternative<0>(doubleVariant)));
    EXPECT_TRUE((WTF::holdsAlternative<1>(doubleVariant)));
    EXPECT_FALSE((WTF::holdsAlternative<2>(doubleVariant)));

    V floatVariant = 1.0f;
    EXPECT_FALSE((WTF::holdsAlternative<0>(floatVariant)));
    EXPECT_FALSE((WTF::holdsAlternative<1>(floatVariant)));
    EXPECT_TRUE((WTF::holdsAlternative<2>(floatVariant)));
}

TEST(WTF_StdLibExtras, HoldsAlternativeByType)
{
    using V = Variant<int, double, float>;

    V intVariant = 42;
    EXPECT_TRUE((WTF::holdsAlternative<int>(intVariant)));
    EXPECT_FALSE((WTF::holdsAlternative<double>(intVariant)));
    EXPECT_FALSE((WTF::holdsAlternative<float>(intVariant)));

    V doubleVariant = 3.14;
    EXPECT_FALSE((WTF::holdsAlternative<int>(doubleVariant)));
    EXPECT_TRUE((WTF::holdsAlternative<double>(doubleVariant)));
    EXPECT_FALSE((WTF::holdsAlternative<float>(doubleVariant)));
}

TEST(WTF_StdLibExtras, SwitchOnTupleAtIndex)
{
    std::tuple<int, double, float> tuple { 42, 3.14, 1.5f };

    // Visit index 0 (int).
    bool visitedInt = false;
    WTF::switchOnTupleAtIndex(0, tuple,
        [&](const int& value) { visitedInt = true; EXPECT_EQ(42, value); },
        [&](const double&) { FAIL(); },
        [&](const float&) { FAIL(); }
    );
    EXPECT_TRUE(visitedInt);

    // Visit index 1 (double).
    bool visitedDouble = false;
    WTF::switchOnTupleAtIndex(1, tuple,
        [&](const int&) { FAIL(); },
        [&](const double& value) { visitedDouble = true; EXPECT_DOUBLE_EQ(3.14, value); },
        [&](const float&) { FAIL(); }
    );
    EXPECT_TRUE(visitedDouble);

    // Visit index 2 (float).
    bool visitedFloat = false;
    WTF::switchOnTupleAtIndex(2, tuple,
        [&](const int&) { FAIL(); },
        [&](const double&) { FAIL(); },
        [&](const float& value) { visitedFloat = true; EXPECT_FLOAT_EQ(1.5f, value); }
    );
    EXPECT_TRUE(visitedFloat);
}

TEST(WTF_StdLibExtras, VisitTupleElementAtIndex)
{
    std::tuple<int, double, float> tuple { 10, 2.5, 0.5f };

    // Verify return value works through visitTupleElementAtIndex.
    auto result = WTF::visitTupleElementAtIndex(
        [](const auto& value) -> double { return static_cast<double>(value); },
        1, tuple
    );
    EXPECT_DOUBLE_EQ(2.5, result);
}

namespace {

struct ZipTestPair {
    int first;
    double second;
};

template<std::size_t I> auto& get(ZipTestPair& pair)
{
    if constexpr (!I)
        return pair.first;
    else
        return pair.second;
}

template<std::size_t I> const auto& get(const ZipTestPair& pair)
{
    if constexpr (!I)
        return pair.first;
    else
        return pair.second;
}

} // namespace

} // namespace TestWebKitAPI

template<> struct std::tuple_size<TestWebKitAPI::ZipTestPair> : std::integral_constant<std::size_t, 2> {
};

template<> struct std::tuple_element<0, TestWebKitAPI::ZipTestPair> {
    using type = int;
};

template<> struct std::tuple_element<1, TestWebKitAPI::ZipTestPair> {
    using type = double;
};

namespace TestWebKitAPI {

TEST(WTF_StdLibExtras, TupleZipTransposes)
{
    std::tuple<int, std::string, double> a { 1, "hello", 1.5 };
    std::tuple<double, char, float> b { 0.5, 'i', 0.1f };

    auto zipped = WTF::tuple_zip(a, b);
    static_assert(std::tuple_size_v<decltype(zipped)> == 3);

    EXPECT_EQ(1, std::get<0>(std::get<0>(zipped)));
    EXPECT_EQ(0.5, std::get<1>(std::get<0>(zipped)));
    EXPECT_EQ("hello", std::get<0>(std::get<1>(zipped)));
    EXPECT_EQ('i', std::get<1>(std::get<1>(zipped)));
    EXPECT_EQ(1.5, std::get<0>(std::get<2>(zipped)));
    EXPECT_EQ(0.1f, std::get<1>(std::get<2>(zipped)));
}

TEST(WTF_StdLibExtras, TupleZipBindsReferencesToLvalues)
{
    std::tuple<int, std::string> a { 1, "a" };
    std::tuple<int, std::string> b { 2, "b" };

    auto zipped = WTF::tuple_zip(a, b);
    static_assert(std::is_same_v<decltype(zipped), std::tuple<std::tuple<int&, int&>, std::tuple<std::string&, std::string&>>>);

    // Elements alias the sources rather than copying them.
    EXPECT_EQ(&std::get<0>(a), &std::get<0>(std::get<0>(zipped)));
    EXPECT_EQ(&std::get<0>(b), &std::get<1>(std::get<0>(zipped)));
    EXPECT_EQ(&std::get<1>(a), &std::get<0>(std::get<1>(zipped)));
    EXPECT_EQ(&std::get<1>(b), &std::get<1>(std::get<1>(zipped)));

    // Writes through the zipped view are visible in the sources.
    std::get<0>(std::get<0>(zipped)) = 10;
    std::get<1>(std::get<1>(zipped)) = "changed";
    EXPECT_EQ(10, std::get<0>(a));
    EXPECT_EQ("changed", std::get<1>(b));
}

TEST(WTF_StdLibExtras, TupleZipPreservesConst)
{
    const std::tuple<int, double> a { 1, 2.0 };
    std::tuple<int, double> b { 3, 4.0 };

    auto zipped = WTF::tuple_zip(a, b);
    static_assert(std::is_same_v<decltype(zipped), std::tuple<std::tuple<const int&, int&>, std::tuple<const double&, double&>>>);
    EXPECT_EQ(&std::get<0>(a), &std::get<0>(std::get<0>(zipped)));
    EXPECT_EQ(&std::get<0>(b), &std::get<1>(std::get<0>(zipped)));
}

TEST(WTF_StdLibExtras, TupleZipSingleElement)
{
    std::tuple<int> a { 1 };
    std::tuple<int> b { 2 };

    // A one-element input must still produce a one-element outer tuple.
    auto zipped = WTF::tuple_zip(a, b);
    static_assert(std::is_same_v<decltype(zipped), std::tuple<std::tuple<int&, int&>>>);
    EXPECT_EQ(&std::get<0>(a), &std::get<0>(std::get<0>(zipped)));
    EXPECT_EQ(&std::get<0>(b), &std::get<1>(std::get<0>(zipped)));
}

TEST(WTF_StdLibExtras, TupleZipSingleOperand)
{
    std::tuple<int, double> a { 1, 2.0 };

    auto zipped = WTF::tuple_zip(a);
    static_assert(std::is_same_v<decltype(zipped), std::tuple<std::tuple<int&>, std::tuple<double&>>>);
    EXPECT_EQ(&std::get<0>(a), &std::get<0>(std::get<0>(zipped)));
    EXPECT_EQ(&std::get<1>(a), &std::get<0>(std::get<1>(zipped)));
}

TEST(WTF_StdLibExtras, TupleZipMixedTupleLikes)
{
    std::pair<int, double> pair { 1, 2.0 };
    std::array<int, 2> array { 3, 4 };

    auto zipped = WTF::tuple_zip(pair, array);
    static_assert(std::tuple_size_v<decltype(zipped)> == 2);
    EXPECT_EQ(&pair.first, &std::get<0>(std::get<0>(zipped)));
    EXPECT_EQ(&array[0], &std::get<1>(std::get<0>(zipped)));
    EXPECT_EQ(&array[1], &std::get<1>(std::get<1>(zipped)));
}

TEST(WTF_StdLibExtras, TupleZipUserDefinedTupleLike)
{
    ZipTestPair a { 1, 2.0 };
    const ZipTestPair b { 3, 4.0 };

    auto zipped = WTF::tuple_zip(a, b);
    static_assert(std::is_same_v<decltype(zipped), std::tuple<std::tuple<int&, const int&>, std::tuple<double&, const double&>>>);
    EXPECT_EQ(&a.first, &std::get<0>(std::get<0>(zipped)));
    EXPECT_EQ(&b.first, &std::get<1>(std::get<0>(zipped)));
    EXPECT_EQ(&a.second, &std::get<0>(std::get<1>(zipped)));
    EXPECT_EQ(&b.second, &std::get<1>(std::get<1>(zipped)));
}

TEST(WTF_StdLibExtras, TupleZipWithApply)
{
    std::tuple<int, int> a { 1, 2 };
    std::tuple<int, int> b { 10, 20 };

    // This is how the Style blending helpers consume the result.
    auto sum = WTF::apply([](auto&&... zippedElements) {
        return (WTF::apply([](auto& x, auto& y) {
            return x + y;
        }, zippedElements) + ...);
    }, WTF::tuple_zip(a, b));
    EXPECT_EQ(33, sum);
}

} // namespace TestWebKitAPI
