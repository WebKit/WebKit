/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "Region.h"

#include <gtest/gtest.h>

using namespace WebCore;

namespace {

#define TEST_INSIDE_RECT(r, x, y, w, h)                      \
    EXPECT_TRUE(r.contains(IntPoint(x, y)));                 \
    EXPECT_TRUE(r.contains(IntPoint(x + w - 1, y)));         \
    EXPECT_TRUE(r.contains(IntPoint(x, y + h - 1)));         \
    EXPECT_TRUE(r.contains(IntPoint(x + w - 1, y + h - 1))); \
    EXPECT_TRUE(r.contains(IntPoint(x, y + h / 2)));         \
    EXPECT_TRUE(r.contains(IntPoint(x + w - 1, y + h / 2))); \
    EXPECT_TRUE(r.contains(IntPoint(x + w / 2, y)));         \
    EXPECT_TRUE(r.contains(IntPoint(x + w / 2, y + h - 1))); \
    EXPECT_TRUE(r.contains(IntPoint(x + w / 2, y + h / 2))); \

#define TEST_LEFT_OF_RECT(r, x, y, w, h)                     \
    EXPECT_FALSE(r.contains(IntPoint(x - 1, y)));            \
    EXPECT_FALSE(r.contains(IntPoint(x - 1, y + h - 1)));    \

#define TEST_RIGHT_OF_RECT(r, x, y, w, h)                 \
    EXPECT_FALSE(r.contains(IntPoint(x + w, y)));         \
    EXPECT_FALSE(r.contains(IntPoint(x + w, y + h - 1))); \

#define TEST_TOP_OF_RECT(r, x, y, w, h)                   \
    EXPECT_FALSE(r.contains(IntPoint(x, y - 1)));         \
    EXPECT_FALSE(r.contains(IntPoint(x + w - 1, y - 1))); \

#define TEST_BOTTOM_OF_RECT(r, x, y, w, h)                \
    EXPECT_FALSE(r.contains(IntPoint(x, y + h)));         \
    EXPECT_FALSE(r.contains(IntPoint(x + w - 1, y + h))); \

TEST(RegionTest, containsPoint)
{
    Region r;

    EXPECT_FALSE(r.contains(IntPoint(0, 0)));

    r.unite(IntRect(35, 35, 1, 1));
    TEST_INSIDE_RECT(r, 35, 35, 1, 1);
    TEST_LEFT_OF_RECT(r, 35, 35, 1, 1);
    TEST_RIGHT_OF_RECT(r, 35, 35, 1, 1);
    TEST_TOP_OF_RECT(r, 35, 35, 1, 1);
    TEST_BOTTOM_OF_RECT(r, 35, 35, 1, 1);

    r.unite(IntRect(30, 30, 10, 10));
    TEST_INSIDE_RECT(r, 30, 30, 10, 10);
    TEST_LEFT_OF_RECT(r, 30, 30, 10, 10);
    TEST_RIGHT_OF_RECT(r, 30, 30, 10, 10);
    TEST_TOP_OF_RECT(r, 30, 30, 10, 10);
    TEST_BOTTOM_OF_RECT(r, 30, 30, 10, 10);

    r.unite(IntRect(31, 40, 10, 10));
    EXPECT_FALSE(r.contains(IntPoint(30, 40)));
    EXPECT_TRUE(r.contains(IntPoint(31, 40)));
    EXPECT_FALSE(r.contains(IntPoint(40, 39)));
    EXPECT_TRUE(r.contains(IntPoint(40, 40)));

    TEST_INSIDE_RECT(r, 30, 30, 10, 10);
    TEST_LEFT_OF_RECT(r, 30, 30, 10, 10);
    TEST_RIGHT_OF_RECT(r, 30, 30, 10, 10);
    TEST_TOP_OF_RECT(r, 30, 30, 10, 10);
    TEST_INSIDE_RECT(r, 31, 40, 10, 10);
    TEST_LEFT_OF_RECT(r, 31, 40, 10, 10);
    TEST_RIGHT_OF_RECT(r, 31, 40, 10, 10);
    TEST_BOTTOM_OF_RECT(r, 31, 40, 10, 10);

    r.unite(IntRect(42, 40, 10, 10));

    TEST_INSIDE_RECT(r, 42, 40, 10, 10);
    TEST_LEFT_OF_RECT(r, 42, 40, 10, 10);
    TEST_RIGHT_OF_RECT(r, 42, 40, 10, 10);
    TEST_TOP_OF_RECT(r, 42, 40, 10, 10);
    TEST_BOTTOM_OF_RECT(r, 42, 40, 10, 10);

    TEST_INSIDE_RECT(r, 30, 30, 10, 10);
    TEST_LEFT_OF_RECT(r, 30, 30, 10, 10);
    TEST_RIGHT_OF_RECT(r, 30, 30, 10, 10);
    TEST_TOP_OF_RECT(r, 30, 30, 10, 10);
    TEST_INSIDE_RECT(r, 31, 40, 10, 10);
    TEST_LEFT_OF_RECT(r, 31, 40, 10, 10);
    TEST_RIGHT_OF_RECT(r, 31, 40, 10, 10);
    TEST_BOTTOM_OF_RECT(r, 31, 40, 10, 10);
}

TEST(RegionTest, emptySpan)
{
    Region r;
    r.unite(IntRect(5, 0, 10, 10));
    r.unite(IntRect(0, 5, 10, 10));
    r.subtract(IntRect(7, 7, 10, 0));

    Vector<IntRect> rects = r.rects();
    for (size_t i = 0; i < rects.size(); ++i)
        EXPECT_FALSE(rects[i].isEmpty());
}

} // namespace
