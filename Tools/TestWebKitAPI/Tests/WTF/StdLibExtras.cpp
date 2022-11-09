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
#include <wtf/StdLibExtras.h>

#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/MathExtras.h>

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
    class LocalClass {
        WTF_MAKE_FAST_ALLOCATED;
    };
    auto s = makeUnique<LocalStruct>();
    auto c = makeUnique<LocalClass>();
}

} // namespace TestWebKitAPI
