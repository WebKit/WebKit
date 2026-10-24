/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include <wtf/FastBitVector.h>

namespace TestWebKitAPI {

TEST(WTF_FastBitVector, SetAllDoesNotSetPaddingBits)
{
    FastBitVector vector;
    vector.resize(5);
    vector.setAll();
    EXPECT_EQ(5u, vector.bitCount());
}

TEST(WTF_FastBitVector, SetAllForEachSetBitStaysInRange)
{
    FastBitVector vector;
    vector.resize(5);
    vector.setAll();
    size_t count = 0;
    size_t maxIndex = 0;
    vector.forEachSetBit([&](size_t index) {
        ++count;
        maxIndex = std::max(maxIndex, index);
    });
    EXPECT_EQ(5u, count);
    EXPECT_EQ(4u, maxIndex);
}

TEST(WTF_FastBitVector, SetAllEqualsBitByBitSet)
{
    FastBitVector a;
    a.resize(5);
    a.setAll();
    FastBitVector b;
    b.resize(5);
    for (size_t i = 0; i < 5; ++i)
        b.at(i) = true;
    EXPECT_TRUE(a == b);
}

TEST(WTF_FastBitVector, GrowAfterSetAllDoesNotExposePaddingBits)
{
    FastBitVector vector;
    vector.resize(5);
    vector.setAll();
    vector.resize(40);
    EXPECT_EQ(5u, vector.bitCount());
    for (size_t i = 5; i < 40; ++i)
        EXPECT_FALSE(vector.at(i));
}

TEST(WTF_FastBitVector, ShrinkThenGrowWithinWordDoesNotResurrectBits)
{
    FastBitVector vector;
    vector.resize(20);
    vector.setAll();
    vector.resize(5);
    EXPECT_EQ(5u, vector.bitCount());
    vector.resize(20);
    EXPECT_EQ(5u, vector.bitCount());
}

} // namespace TestWebKitAPI
