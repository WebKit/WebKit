/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/MathExtras.h>
#include <wtf/StdLibExtras.h>

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
// makeUnique. The makeUnique needs WTF_..._MAKE_FAST_ALLOCATED.
// There used to be a warn-unused-typedef errors when using these.
TEST(WTF_StdLibExtras, MakeUniqueFunctionLocalTypeCompiles)
{
    struct LocalStruct {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
    };
    IGNORE_CLANG_WARNINGS_BEGIN("unused-local-typedef")
    class LocalClass {
        WTF_MAKE_FAST_ALLOCATED;
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

} // namespace TestWebKitAPI
