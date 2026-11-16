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

#include "Helpers/Test.h"
#include <WebCore/CSSValueAggregates.h>

namespace TestWebKitAPI {

template<typename T> static void testAnyOfAllOf()
{
    auto isPositive = [](int value) { return value > 0; };

    T bothMatch { 1, 2 };
    EXPECT_TRUE(bothMatch.anyOf(isPositive));
    EXPECT_TRUE(bothMatch.allOf(isPositive));

    T firstMatches { 1, -2 };
    EXPECT_TRUE(firstMatches.anyOf(isPositive));
    EXPECT_FALSE(firstMatches.allOf(isPositive));

    T secondMatches { -1, 2 };
    EXPECT_TRUE(secondMatches.anyOf(isPositive));
    EXPECT_FALSE(secondMatches.allOf(isPositive));

    T noneMatch { -1, -2 };
    EXPECT_FALSE(noneMatch.anyOf(isPositive));
    EXPECT_FALSE(noneMatch.allOf(isPositive));
}

TEST(CSSValueAggregates, SpaceSeparatedPairAnyOf)
{
    testAnyOfAllOf<WebCore::SpaceSeparatedPair<int>>();
}

TEST(CSSValueAggregates, MinimallySerializingSpaceSeparatedPairAnyOf)
{
    testAnyOfAllOf<WebCore::MinimallySerializingSpaceSeparatedPair<int>>();
}

TEST(CSSValueAggregates, SpaceSeparatedPointAnyOf)
{
    testAnyOfAllOf<WebCore::SpaceSeparatedPoint<int>>();
}

TEST(CSSValueAggregates, SpaceSeparatedSizeAnyOf)
{
    testAnyOfAllOf<WebCore::SpaceSeparatedSize<int>>();
}

TEST(CSSValueAggregates, MinimallySerializingSpaceSeparatedPointAnyOf)
{
    testAnyOfAllOf<WebCore::MinimallySerializingSpaceSeparatedPoint<int>>();
}

TEST(CSSValueAggregates, MinimallySerializingSpaceSeparatedSizeAnyOf)
{
    testAnyOfAllOf<WebCore::MinimallySerializingSpaceSeparatedSize<int>>();
}

} // namespace TestWebKitAPI
