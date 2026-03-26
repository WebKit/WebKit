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

} // namespace TestWebKitAPI
