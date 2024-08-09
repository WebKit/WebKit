/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include <WebCore/RoundedRect.h>

using namespace WebCore;

namespace TestWebKitAPI {

static LayoutRect layoutRect(float x, float y, float width, float height)
{
    return { LayoutPoint(x, y), LayoutSize(width, height) };
}

TEST(WebCore, RoundedRectContainsRect)
{
    auto roundedRectBounds = layoutRect(-10, -40, 40, 80);
    RoundedRect roundedRect(roundedRectBounds, { 10, 10 }, { 10, 20 }, { 0, 0 }, { 20, 40 });

    // Cases where the other rect is outside of the rounded rect's bounds.
    EXPECT_FALSE(roundedRect.contains(layoutRect(-40, -10, 10, 10)));
    EXPECT_FALSE(roundedRect.contains(layoutRect(-40, 10, 100, 10)));
    EXPECT_FALSE(roundedRect.contains(layoutRect(20, 20, 60, 60)));
    EXPECT_FALSE(roundedRect.contains(layoutRect(10, -80, 10, 200)));
    EXPECT_FALSE(roundedRect.contains(layoutRect(-80, -80, 160, 160)));

    // Cases where the other rect doesn't intersect with any corners.
    EXPECT_TRUE(roundedRect.contains(layoutRect(-5, -5, 10, 10)));
    EXPECT_TRUE(roundedRect.contains(layoutRect(20, -10, 5, 5)));
    EXPECT_TRUE(roundedRect.contains(layoutRect(-5, 5, 5, 5)));
    EXPECT_TRUE(roundedRect.contains(layoutRect(0, 0, 10, 10)));
    EXPECT_TRUE(roundedRect.contains(layoutRect(0, -10, 10, 10)));

    // Cases where the other rect intersects at one of the rounded rect's corners.
    EXPECT_FALSE(roundedRect.contains(layoutRect(-9, -39, 20, 40)));
    EXPECT_TRUE(roundedRect.contains(layoutRect(-1, -31, 20, 40)));
    EXPECT_FALSE(roundedRect.contains(layoutRect(20, -30, 10, 10)));
    EXPECT_TRUE(roundedRect.contains(layoutRect(10, -30, 11, 11)));
    EXPECT_FALSE(roundedRect.contains(layoutRect(15, 25, 14, 14)));
    EXPECT_TRUE(roundedRect.contains(layoutRect(15, 25, 3, 3)));
    EXPECT_TRUE(roundedRect.contains(layoutRect(-10, 20, 10, 20)));

    // The other rect is equal to the rounded rect's bounds.
    EXPECT_FALSE(roundedRect.contains(roundedRectBounds));
    RoundedRect nonRoundedRect(roundedRectBounds, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 });
    EXPECT_TRUE(nonRoundedRect.contains(roundedRectBounds));
}

}
