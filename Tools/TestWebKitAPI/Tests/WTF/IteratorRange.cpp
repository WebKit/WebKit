/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include <wtf/IteratorRange.h>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

TEST(WTF_IteratorRange, MakeReversedRange)
{
    Vector<int> intVector { 10, 11, 12, 13 };

    auto reversedRange = WTF::makeReversedRange(intVector);

    static_assert(std::is_same<decltype(reversedRange.begin()), typename Vector<int>::reverse_iterator>::value, "IteratorRange has correct begin() iterator type");
    static_assert(std::is_same<decltype(reversedRange.end()), typename Vector<int>::reverse_iterator>::value, "IteratorRange has correct end() iterator type");

    EXPECT_EQ(reversedRange.begin(), intVector.rbegin());
    EXPECT_EQ(reversedRange.end(), intVector.rend());

    std::array<int, 4> expectedResults { { 13, 12, 11, 10 } };
    size_t index = 0;

    for (auto& value : reversedRange)
        EXPECT_EQ(value, expectedResults[index++]);
}

TEST(WTF_IteratorRange, MakeConstReversedRange)
{
    const Vector<int> intVector { 10, 11, 12, 13 };

    auto reversedRange = WTF::makeReversedRange(intVector);

    static_assert(std::is_same<decltype(reversedRange.begin()), typename Vector<int>::const_reverse_iterator>::value, "IteratorRange has correct begin() iterator type");
    static_assert(std::is_same<decltype(reversedRange.end()), typename Vector<int>::const_reverse_iterator>::value, "IteratorRange has correct end() iterator type");

    EXPECT_EQ(reversedRange.begin(), intVector.rbegin());
    EXPECT_EQ(reversedRange.end(), intVector.rend());

    std::array<int, 4> expectedResults { { 13, 12, 11, 10 } };
    size_t index = 0;

    for (auto& value : reversedRange)
        EXPECT_EQ(value, expectedResults[index++]);
}

TEST(WTF_IteratorRange, MakeReversedRangeFromRange)
{
    Vector<int> intVector { 10, 11, 12, 13 };

    auto range = IteratorRange { intVector.begin(), intVector.end() };

    auto reversedRange = makeReversedRange(range);

    EXPECT_EQ(reversedRange.begin(), intVector.rbegin());
    EXPECT_EQ(reversedRange.end(), intVector.rend());

    std::array<int, 4> expectedResults { { 13, 12, 11, 10 } };
    size_t index = 0;

    for (auto& value : reversedRange)
        EXPECT_EQ(value, expectedResults[index++]);
}

TEST(WTF_IteratorRange, OneWayIterator)
{
    struct OneWayIterator {
        int* ptr;

        bool operator!=(const OneWayIterator& other) { return ptr != other.ptr; }
        auto& operator*() const { return *ptr; }
        void operator++() { ++ptr; }
    };

    Vector<int> intVector { 10, 11, 12, 13 };

    auto range = IteratorRange { OneWayIterator { intVector.begin() }, OneWayIterator { intVector.end() } };

    std::array<int, 4> expectedResults { { 10, 11, 12, 13 } };
    size_t index = 0;

    for (auto& value : range)
        EXPECT_EQ(value, expectedResults[index++]);
}

} // namespace TestWebKitAPI
