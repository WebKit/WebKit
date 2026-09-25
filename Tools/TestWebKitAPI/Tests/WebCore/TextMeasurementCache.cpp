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

#include <WebCore/FontCascadeFonts.h>
#include <WebCore/TextMeasurementCache.h>

namespace TestWebKitAPI {

using namespace WebCore;

namespace {

// InitialInterval = MinInterval = MaxInterval = 0 disables the sampling countdown, so every add() reaches addSlowCase() immediately.
constexpr unsigned testMaxTextLength = 32;
constexpr unsigned testInlineKeyCapacity = 8;
constexpr unsigned testMaxSize = 4;
using TestCache = TextMeasurementCache<int, 0, 0, 0, testMaxTextLength, testInlineKeyCapacity, testMaxSize>;

String stringOfLength(unsigned length, char16_t fill = 'a')
{
    Vector<char16_t> characters(length);
    characters.fill(fill);
    return String(std::span<const char16_t> { characters });
}

}

TEST(TextMeasurementCacheTest, SingleCharacterIsCached)
{
    TestCache cache;
    EXPECT_TRUE(cache.isEmpty());

    auto* entry = cache.add("a"_s, 1);
    ASSERT_TRUE(entry);
    EXPECT_FALSE(cache.isEmpty());

    auto* sameEntry = cache.add("a"_s, 2);
    EXPECT_EQ(entry, sameEntry);
}

TEST(TextMeasurementCacheTest, ShortTextUsesInlineTable)
{
    TestCache cache;
    String text = stringOfLength(testInlineKeyCapacity);

    auto* entry = cache.add(text, 1);
    ASSERT_TRUE(entry);
    EXPECT_FALSE(cache.isEmpty());

    auto* sameEntry = cache.add(text, 2);
    EXPECT_EQ(entry, sameEntry);

    cache.clear();
    EXPECT_TRUE(cache.isEmpty());
}

TEST(TextMeasurementCacheTest, LongTextUsesOutOfLineTable)
{
    TestCache cache;
    String text = stringOfLength(testInlineKeyCapacity + 1);
    ASSERT_LE(text.length(), testMaxTextLength);

    auto* entry = cache.add(text, 1);
    ASSERT_TRUE(entry);
    EXPECT_FALSE(cache.isEmpty());

    // A repeated add() with the same out-of-line key must hit the existing entry, not silently re-insert.
    auto* sameEntry = cache.add(text, 2);
    EXPECT_EQ(entry, sameEntry);
    EXPECT_EQ(1, *entry);

    cache.clear();
    EXPECT_TRUE(cache.isEmpty());
}

TEST(TextMeasurementCacheTest, TextLongerThanMaxTextLengthIsRejected)
{
    TestCache cache;
    String text = stringOfLength(testMaxTextLength + 1);

    auto* entry = cache.add(text, 1);
    EXPECT_FALSE(entry);
    EXPECT_TRUE(cache.isEmpty());
}

TEST(TextMeasurementCacheTest, ClearAndIsEmptyCoverAllThreeTables)
{
    TestCache cache;
    EXPECT_TRUE(cache.isEmpty());

    cache.add("a"_s, 1);
    EXPECT_FALSE(cache.isEmpty());
    cache.clear();
    EXPECT_TRUE(cache.isEmpty());

    cache.add(stringOfLength(testInlineKeyCapacity), 1);
    EXPECT_FALSE(cache.isEmpty());
    cache.clear();
    EXPECT_TRUE(cache.isEmpty());

    cache.add(stringOfLength(testInlineKeyCapacity + 1), 1);
    EXPECT_FALSE(cache.isEmpty());
    cache.clear();
    EXPECT_TRUE(cache.isEmpty());
}

TEST(TextMeasurementCacheTest, PathologicalGrowthFullyFlushes)
{
    TestCache cache;
    for (unsigned i = 0; i < testMaxSize - 1; ++i) {
        String text = stringOfLength(testInlineKeyCapacity + 1, static_cast<char16_t>('a' + i));
        auto* entry = cache.add(text, static_cast<int>(i));
        ASSERT_TRUE(entry);
    }
    EXPECT_FALSE(cache.isEmpty());

    // The MaxSize-th distinct entry crosses the ceiling: addSlowCase() rejects it and fully flushes the cache.
    String overflowText = stringOfLength(testInlineKeyCapacity + 1, static_cast<char16_t>('a' + testMaxSize));
    auto* entry = cache.add(overflowText, -1);
    EXPECT_FALSE(entry);
    EXPECT_TRUE(cache.isEmpty());
}

}
