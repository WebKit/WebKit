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
#include <wtf/BitVector.h>

#include <wtf/MathExtras.h>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

// Bug: mergeSlow iterates a.size() words but indexes into b which may have
// fewer words, causing an out-of-bounds read. This happens when `this` is
// already larger than `other` (both out-of-line) so ensureSize() is a no-op.
TEST(WTF_BitVector, MergeLargerIntoSmaller)
{
    // Create a large BitVector (256 bits = 4 words on 64-bit).
    BitVector large(256);
    large.quickSet(0);
    large.quickSet(200);

    // Create a smaller BitVector (128 bits = 2 words on 64-bit).
    BitVector small(128);
    small.quickSet(42);
    small.quickSet(100);

    // large.merge(small) should OR small's bits into large.
    // Bug: the loop uses large's word count as the bound but indexes into
    // small's word span, reading words out of bounds.
    large.merge(small);

    // Bits originally in large must survive.
    EXPECT_TRUE(large.get(0));
    EXPECT_TRUE(large.get(200));

    // Bits from small must be merged in.
    EXPECT_TRUE(large.get(42));
    EXPECT_TRUE(large.get(100));

    // Bits that were never set must remain clear.
    EXPECT_FALSE(large.get(1));
    EXPECT_FALSE(large.get(64));
    EXPECT_FALSE(large.get(150));
    EXPECT_FALSE(large.get(255));

    // Total set bits should be exactly 4.
    EXPECT_EQ(large.bitCount(), 4u);
}

TEST(WTF_BitVector, MoveConstructInline)
{
    BitVector a;
    a.set(0);
    a.set(5);
    a.set(62);

    BitVector b(WTF::move(a));

    EXPECT_TRUE(b.get(0));
    EXPECT_TRUE(b.get(5));
    EXPECT_TRUE(b.get(62));
    EXPECT_EQ(b.bitCount(), 3u);

    // Source should be empty after move.
    EXPECT_TRUE(a.isEmpty());
    EXPECT_EQ(a.bitCount(), 0u);
}

TEST(WTF_BitVector, MoveConstructOutOfLine)
{
    BitVector a(256);
    a.quickSet(0);
    a.quickSet(100);
    a.quickSet(200);

    BitVector b(WTF::move(a));

    EXPECT_TRUE(b.get(0));
    EXPECT_TRUE(b.get(100));
    EXPECT_TRUE(b.get(200));
    EXPECT_EQ(b.bitCount(), 3u);
    EXPECT_GE(b.size(), 256u);

    // Source should be empty inline after move.
    EXPECT_TRUE(a.isEmpty());
    EXPECT_EQ(a.bitCount(), 0u);
}

TEST(WTF_BitVector, MoveAssignInline)
{
    BitVector a;
    a.set(3);
    a.set(10);

    BitVector b;
    b.set(7);

    b = WTF::move(a);

    EXPECT_TRUE(b.get(3));
    EXPECT_TRUE(b.get(10));
    EXPECT_FALSE(b.get(7));
    EXPECT_EQ(b.bitCount(), 2u);

    EXPECT_TRUE(a.isEmpty());
}

TEST(WTF_BitVector, MoveAssignOutOfLine)
{
    BitVector a(256);
    a.quickSet(50);
    a.quickSet(150);

    BitVector b(128);
    b.quickSet(0);

    b = WTF::move(a);

    EXPECT_TRUE(b.get(50));
    EXPECT_TRUE(b.get(150));
    EXPECT_FALSE(b.get(0));
    EXPECT_EQ(b.bitCount(), 2u);
    EXPECT_GE(b.size(), 256u);

    EXPECT_TRUE(a.isEmpty());
}

TEST(WTF_BitVector, MoveAssignOutOfLineToOutOfLine)
{
    // Both source and destination are out-of-line. The destination's
    // allocation should be freed.
    BitVector a(200);
    a.quickSet(100);

    BitVector b(300);
    b.quickSet(250);

    b = WTF::move(a);

    EXPECT_TRUE(b.get(100));
    EXPECT_FALSE(b.get(250));
    EXPECT_EQ(b.bitCount(), 1u);

    EXPECT_TRUE(a.isEmpty());
}

TEST(WTF_BitVector, MoveInVector)
{
    Vector<BitVector> vec;
    for (unsigned i = 0; i < 100; ++i) {
        BitVector bv(256);
        bv.quickSet(i);
        vec.append(WTF::move(bv));
    }

    for (unsigned i = 0; i < 100; ++i) {
        EXPECT_TRUE(vec[i].get(i));
        EXPECT_EQ(vec[i].bitCount(), 1u);
    }
}

TEST(WTF_BitVector, MoveAssignSelf)
{
    BitVector a(256);
    a.quickSet(42);

    auto& ref = a;
    a = WTF::move(ref);

    // Should survive self-move-assignment.
    EXPECT_TRUE(a.get(42));
    EXPECT_EQ(a.bitCount(), 1u);
}

TEST(WTF_BitVector, FilterOutOfLineWithInline)
{
    // Create an out-of-line BitVector (256 bits = 4 words on 64-bit)
    // with bits set across multiple words.
    BitVector large(256);
    large.quickSet(5);
    large.quickSet(42);
    large.quickSet(100); // word 1 (bits 64-127).
    large.quickSet(200); // word 3 (bits 192-255).

    // Create an inline BitVector (fits in maxInlineBits = 63 bits)
    // with only bit 5 set.
    BitVector small;
    small.set(5);

    // filter is AND: only bits set in both should survive.
    // Bit 5 is in both, so it survives. Bits 42, 100, 200 are not in
    // small (which has no bits beyond 62), so they must be cleared.
    large.filter(small);

    EXPECT_TRUE(large.get(5));
    EXPECT_FALSE(large.get(42));
    EXPECT_FALSE(large.get(100));
    EXPECT_FALSE(large.get(200));

    EXPECT_EQ(large.bitCount(), 1u);
}

} // namespace TestWebKitAPI
