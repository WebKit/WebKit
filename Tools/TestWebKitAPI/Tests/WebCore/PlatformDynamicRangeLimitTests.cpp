/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <WebCore/PlatformDynamicRangeLimit.h>

#include <WebCore/StyleDynamicRangeLimit.h>

namespace TestWebKitAPI {

// Tests rely on these assumptions:
static_assert(WebCore::PlatformDynamicRangeLimit::standard().value() == 0.0f);
static_assert(WebCore::PlatformDynamicRangeLimit::constrainedHigh().value() == 0.5f);
static_assert(WebCore::PlatformDynamicRangeLimit::noLimit().value() == 1.0f);

TEST(PlatformDynamicRangeLimit, DefaultConstruction)
{
    auto def = WebCore::PlatformDynamicRangeLimit();
    EXPECT_EQ(def.value(), 1.0f);

    static_assert(WebCore::PlatformDynamicRangeLimit().value() == 1);
}

static WebCore::PlatformDynamicRangeLimit mix(float standard, float constrainedHigh, float noLimit)
{
    return WebCore::Style::DynamicRangeLimit { WebCore::Style::DynamicRangeLimitMixFunction { WebCore::Style::DynamicRangeLimitMixParameters { .standard = standard, .constrainedHigh = constrainedHigh, .noLimit = noLimit } } }.toPlatformDynamicRangeLimit();
}

TEST(PlatformDynamicRangeLimit, FromStyleDynamicRangeLimit)
{
    struct Test {
        WebCore::PlatformDynamicRangeLimit dynamicRangeLimit;
        float expectedValue;
    };
    Test tests[] = {
        // Keywords.
        { WebCore::Style::DynamicRangeLimit(WebCore::CSS::Keyword::Standard()).toPlatformDynamicRangeLimit(), 0 },
        { WebCore::Style::DynamicRangeLimit(WebCore::CSS::Keyword::ConstrainedHigh()).toPlatformDynamicRangeLimit(), 0.5 },
        { WebCore::Style::DynamicRangeLimit(WebCore::CSS::Keyword::NoLimit()).toPlatformDynamicRangeLimit(), 1 },

        // Mixes equivalent to keywords.
        { mix(100, 0, 0), 0 },
        { mix(0, 100, 0), 0.5 },
        { mix(0, 0, 100), 1 },

        // Other mixes.
        { mix(80, 20, 0), 0.1 },
        { mix(50, 0, 50), 0.5 },
        { mix(10, 10, 80), 0.85 },
    };

    int testIndex = 0;
    for (const auto& test : tests) {
        SCOPED_TRACE(testIndex++);
        auto dynamicRangeLimit = test.dynamicRangeLimit;
        EXPECT_FLOAT_EQ(dynamicRangeLimit.value(), test.expectedValue);
    }
}

TEST(PlatformDynamicRangeLimit, StaticValues)
{
    EXPECT_EQ(WebCore::PlatformDynamicRangeLimit::standard(), WebCore::Style::DynamicRangeLimit(WebCore::CSS::Keyword::Standard()).toPlatformDynamicRangeLimit());
    EXPECT_EQ(WebCore::PlatformDynamicRangeLimit::constrainedHigh(), WebCore::Style::DynamicRangeLimit(WebCore::CSS::Keyword::ConstrainedHigh()).toPlatformDynamicRangeLimit());
    EXPECT_EQ(WebCore::PlatformDynamicRangeLimit::noLimit(), WebCore::Style::DynamicRangeLimit(WebCore::CSS::Keyword::NoLimit()).toPlatformDynamicRangeLimit());
}

#if ASSERT_ENABLED
TEST(PlatformDynamicRangeLimit, DISABLED_FromNonsense)
#else
TEST(PlatformDynamicRangeLimit, FromNonsense)
#endif
{
    WebCore::PlatformDynamicRangeLimit tests[] = {
        mix(0, 0, 0),
        mix(-1, 0, 0),
        mix(-1, 0, 2),
        mix(1, 0, -2),
        mix(-1, 0, -2),
        mix(3, 0, -2),
        mix(0, -1, 3),
        mix(0, 0, std::numeric_limits<float>::max()),
        mix(0, 0, std::numeric_limits<float>::infinity()),
        mix(0, 0, std::numeric_limits<float>::quiet_NaN()),
    };

    int testIndex = 0;
    for (const auto& dynamicRangeLimit : tests) {
        SCOPED_TRACE(testIndex++);
        // The actual result doesn't matter, but it has to be valid.
        EXPECT_TRUE(std::isfinite(dynamicRangeLimit.value()));
        EXPECT_GE(dynamicRangeLimit.value(), 0.0f);
        EXPECT_LE(dynamicRangeLimit.value(), 1.0f);
    }
}

TEST(PlatformDynamicRangeLimit, Comparisons)
{
    WebCore::PlatformDynamicRangeLimit sortedTests[] = {
        WebCore::PlatformDynamicRangeLimit::standard(),
        WebCore::Style::DynamicRangeLimit(WebCore::CSS::Keyword::Standard()).toPlatformDynamicRangeLimit(),
        mix(1, 0, 0),

        mix(99, 1, 0),

        mix(80, 20, 0),

        mix(1, 99, 0),

        WebCore::PlatformDynamicRangeLimit::constrainedHigh(),
        WebCore::Style::DynamicRangeLimit(WebCore::CSS::Keyword::ConstrainedHigh()).toPlatformDynamicRangeLimit(),
        mix(0, 1, 0),
        mix(50, 0, 50),

        mix(0, 99, 1),

        mix(10, 10, 80),

        mix(0, 1, 99),

        WebCore::PlatformDynamicRangeLimit::noLimit(),
        WebCore::Style::DynamicRangeLimit(WebCore::CSS::Keyword::NoLimit()).toPlatformDynamicRangeLimit(),
        mix(0, 0, 1),
    };

    int lhsIndex = 0;
    for (const auto& lhs : sortedTests) {
        SCOPED_TRACE(lhsIndex++);
        SCOPED_TRACE(lhs.value());
        int rhsIndex = 0;
        for (const auto& rhs : sortedTests) {
            SCOPED_TRACE(rhsIndex++);
            SCOPED_TRACE(rhs.value());
            EXPECT_EQ(lhs == rhs, lhs.value() == rhs.value());
            EXPECT_EQ(lhs != rhs, lhs.value() != rhs.value());
            EXPECT_EQ(lhs < rhs, lhs.value() < rhs.value());
            EXPECT_EQ(lhs <= rhs, lhs.value() <= rhs.value());
            EXPECT_EQ(lhs >= rhs, lhs.value() >= rhs.value());
            EXPECT_EQ(lhs > rhs, lhs.value() > rhs.value());
            EXPECT_EQ(lhs <=> rhs, lhs.value() <=> rhs.value());

            if (rhsIndex == lhsIndex + 1) {
                EXPECT_LE(lhs, rhs);
                EXPECT_GE(rhs, lhs);
            }
        }
    }
}

}
