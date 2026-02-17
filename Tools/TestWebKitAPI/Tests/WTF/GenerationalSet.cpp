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
#include <wtf/GenerationalSet.h>

#include "Test.h"

namespace TestWebKitAPI {

TEST(WTF_GenerationalSet, Basic)
{
    GenerationalSet<uint8_t> set(100);

    EXPECT_FALSE(set.contains(0));
    EXPECT_FALSE(set.contains(42));

    set.add(42);
    EXPECT_TRUE(set.contains(42));
    EXPECT_FALSE(set.contains(0));

    set.add(0);
    EXPECT_TRUE(set.contains(0));
    EXPECT_TRUE(set.contains(42));

    set.clear();
    EXPECT_FALSE(set.contains(0));
    EXPECT_FALSE(set.contains(42));

    set.add(0);
    EXPECT_TRUE(set.contains(0));
    EXPECT_FALSE(set.contains(42));
}

TEST(WTF_GenerationalSet, GenerationWrapAround)
{
    // Use uint8_t so wrap-around happens after 255 clears
    GenerationalSet<uint8_t> set(10);

    set.add(0);
    set.add(5);
    set.add(9);
    EXPECT_TRUE(set.contains(0));
    EXPECT_TRUE(set.contains(5));
    EXPECT_TRUE(set.contains(9));
    EXPECT_FALSE(set.contains(1));

    // Do 300 clear cycles to ensure we wrap around at least once
    for (unsigned cycle = 0; cycle < 300; ++cycle) {
        set.clear();
        // After clear, elements should be not present (and remain not present even after the generation counter wraps)
        EXPECT_FALSE(set.contains(0));
        EXPECT_FALSE(set.contains(5));
        EXPECT_FALSE(set.contains(9));
        EXPECT_FALSE(set.contains(1));
    }
}

TEST(WTF_GenerationalSet, Resize)
{
    GenerationalSet<uint8_t> set(10);

    set.add(5);
    EXPECT_TRUE(set.contains(5));
    EXPECT_EQ(set.size(), 10u);

    // Grow the set
    set.resize(20);
    EXPECT_EQ(set.size(), 20u);

    // Existing element still present
    EXPECT_TRUE(set.contains(5));

    // Can add to new indices
    set.add(15);
    EXPECT_TRUE(set.contains(15));

    // Clear works after resize
    set.clear();
    EXPECT_FALSE(set.contains(5));
    EXPECT_FALSE(set.contains(15));

    // Shrink the set
    set.add(3);
    set.add(7);
    EXPECT_TRUE(set.contains(3));
    EXPECT_TRUE(set.contains(7));

    set.resize(10);
    EXPECT_EQ(set.size(), 10u);

    // Elements within new size still present
    EXPECT_TRUE(set.contains(3));
    EXPECT_TRUE(set.contains(7));

    // Clear works after shrinking
    set.clear();
    EXPECT_FALSE(set.contains(3));
    EXPECT_FALSE(set.contains(7));
}

TEST(WTF_GenerationalSet, DifferentGenerationTypes)
{
    GenerationalSet<uint16_t> set16(50);
    set16.add(25);
    EXPECT_TRUE(set16.contains(25));
    set16.clear();
    EXPECT_FALSE(set16.contains(25));

    GenerationalSet<uint64_t> set64(50);
    set64.add(25);
    EXPECT_TRUE(set64.contains(25));
    set64.clear();
    EXPECT_FALSE(set64.contains(25));
}

} // namespace TestWebKitAPI
