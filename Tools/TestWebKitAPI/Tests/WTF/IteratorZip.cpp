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

TEST(WTF_IteratorZip, Zip2)
{
    Vector<int> intVector { 1, 2, 3, 4 };
    Vector<double> doubleVector { 10.0, 20.0, 30.0, 40.0 };

    std::array<double, 4> expectedResults { { 11.0, 22.0, 33.0, 44.0 } };
    unsigned index = 0;

    for (auto [i, d] : zip(intVector, doubleVector))
        EXPECT_EQ(static_cast<double>(i) + d, expectedResults[index++]);

    EXPECT_EQ(index, 4u);
}

TEST(WTF_IteratorZip, Zip2FirstShorter)
{
    Vector<int> intVector { 1, 2, 3, 4 };
    Vector<double> doubleVector { 10.0, 20.0, 30.0, 40.0, 50.0 };

    std::array<double, 4> expectedResults { { 11.0, 22.0, 33.0, 44.0 } };
    unsigned index = 0;

    for (auto [i, d] : zip(intVector, doubleVector))
        EXPECT_EQ(static_cast<double>(i) + d, expectedResults[index++]);

    EXPECT_EQ(index, 4u);
}

TEST(WTF_IteratorZip, Zip2SecondShorter)
{
    Vector<int> intVector { 1, 2, 3, 4, 5 };
    Vector<double> doubleVector { 10.0, 20.0, 30.0, 40.0 };

    std::array<double, 4> expectedResults { { 11.0, 22.0, 33.0, 44.0 } };
    unsigned index = 0;

    for (auto [i, d] : zip(intVector, doubleVector))
        EXPECT_EQ(static_cast<double>(i) + d, expectedResults[index++]);

    EXPECT_EQ(index, 4u);
}

TEST(WTF_IteratorZip, Zip3)
{
    Vector<int> intVectorA { 1, 2, 3, 4 };
    Vector<int> intVectorB { 10, 20, 30, 40 };
    Vector<int> intVectorC { 100, 200, 300, 400 };

    std::array<int, 4> expectedResults { { 111, 222, 333, 444 } };
    unsigned index = 0;

    for (auto [a, b, c] : zip(intVectorA, intVectorB, intVectorC))
        EXPECT_EQ(a + b + c, expectedResults[index++]);

    EXPECT_EQ(index, 4u);
}

TEST(WTF_IteratorZip, ZipFirst)
{
    Vector<int> intVector { 1, 2, 3, 4 };
    Vector<double> doubleVector { 10.0, 20.0, 30.0, 40.0, 50.0 };

    std::array<double, 4> expectedResults { { 11.0, 22.0, 33.0, 44.0 } };
    unsigned index = 0;

    for (auto [i, d] : WTF::zip_first(intVector, doubleVector))
        EXPECT_EQ(static_cast<double>(i) + d, expectedResults[index++]);

    EXPECT_EQ(index, 4u);
}

TEST(WTF_IteratorZip, ZipLongest)
{
    Vector<int> intVector { 1, 2, 3, 4 };
    Vector<double> doubleVector { 10.0, 20.0, 30.0, 40.0, 50.0 };

    std::array<double, 5> expectedResults { { 11.0, 22.0, 33.0, 44.0, 50.0 } };
    unsigned index = 0;

    for (auto [i, d] : WTF::zip_longest(intVector, doubleVector)) {
        if (index < 4) {
            EXPECT_TRUE(i.has_value());
            EXPECT_TRUE(d.has_value());
            EXPECT_EQ(static_cast<double>(*i) + *d, expectedResults[index++]);
        } else {
            EXPECT_FALSE(i.has_value());
            EXPECT_TRUE(d.has_value());
            EXPECT_EQ(*d, expectedResults[index++]);
        }
    }

    EXPECT_EQ(index, 5u);
}

} // namespace TestWebKitAPI
