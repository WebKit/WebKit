/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

TEST(WTF_IteratorEnumerate, Enumerate)
{
    Vector<int> intVector { 10, 20, 30, 40 };

    std::array<int, 4> expectedResults { { 10 + 0, 20 + 1, 30 + 2, 40 + 3 } };
    unsigned index = 0;

    for (auto [enumerator, intValue] : enumerate(intVector))
        EXPECT_EQ(static_cast<int>(enumerator) + intValue, expectedResults[index++]);

    EXPECT_EQ(index, 4u);
}

TEST(WTF_IteratorEnumerate, EnumerateMultiple)
{
    Vector<int> intVector { 10, 20, 30, 40 };
    Vector<double> doubleVector { 100.0, 200.0, 300.0, 400.0 };

    std::array<double, 4> expectedResults { { 110.0, 221.0, 332.0, 443.0 } };
    unsigned index = 0;

    for (auto [enumerator, intValue, doubleValue] : enumerate(intVector, doubleVector))
        EXPECT_EQ(static_cast<double>(enumerator) + static_cast<double>(intValue) + doubleValue, expectedResults[index++]);

    EXPECT_EQ(index, 4u);
}

TEST(WTF_IteratorEnumerate, Iota)
{
    std::array<size_t, 4> expectedResults { { 0, 1, 2, 3 } };
    unsigned index = 0;

    for (auto i : WTF::iota()) {
        if (i == 4)
            break;
        EXPECT_EQ(i, expectedResults[index++]);
    }

    EXPECT_EQ(index, 4u);
}

TEST(WTF_IteratorEnumerate, IotaRange)
{
    std::array<size_t, 4> expectedResults { { 3, 4, 5, 6 } };
    unsigned index = 0;

    for (auto i : WTF::iota(3, 7))
        EXPECT_EQ(i, expectedResults[index++]);

    EXPECT_EQ(index, 4u);
}

} // namespace TestWebKitAPI
