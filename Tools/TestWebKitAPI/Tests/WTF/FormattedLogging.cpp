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
#include <wtf/FormattedLogging.h>

#include <wtf/URL.h>

namespace TestWebKitAPI {

struct CustomStreamable {
    int x;
    int y;
};

TextStream& operator<<(TextStream& ts, const CustomStreamable& value)
{
    ts << "(" << value.x << ", " << value.y << ")";
    return ts;
}

static_assert(WTF::TextStreamable<CustomStreamable>);
static_assert(WTF::TextStreamable<URL>);
static_assert(!WTF::TextStreamable<int>);
static_assert(!WTF::TextStreamable<float>);
static_assert(!WTF::TextStreamable<bool>);
static_assert(!WTF::TextStreamable<int*>);
static_assert(!WTF::TextStreamable<const char*>);
static_assert(!WTF::TextStreamable<char[6]>);

TEST(WTF_FormattedLogging, BasicTypes)
{
    EXPECT_EQ(std::format("{}", 42), "42");
    EXPECT_EQ(std::format("{}", 3.14), "3.14");
    EXPECT_EQ(std::format("{}", "hello"), "hello");
}

TEST(WTF_FormattedLogging, CustomStreamableType)
{
    CustomStreamable point { 10, 20 };
    EXPECT_EQ(std::format("{}", point), "(10, 20)");
}

TEST(WTF_FormattedLogging, CustomStreamableInFormatString)
{
    CustomStreamable point { 5, 7 };
    EXPECT_EQ(std::format("point: {}", point), "point: (5, 7)");
}

TEST(WTF_FormattedLogging, MultipleCustomStreamableArgs)
{
    CustomStreamable a { 1, 2 };
    CustomStreamable b { 3, 4 };
    EXPECT_EQ(std::format("{} and {}", a, b), "(1, 2) and (3, 4)");
}

TEST(WTF_FormattedLogging, MixedArgs)
{
    CustomStreamable point { 42, 99 };
    EXPECT_EQ(std::format("val={} point={} flag={}", 123, point, true), "val=123 point=(42, 99) flag=true");
}

TEST(WTF_FormattedLogging, URLType)
{
    URL url { "https://webkit.org/test"_s };
    EXPECT_EQ(std::format("{}", url), "https://webkit.org/test");
}

TEST(WTF_FormattedLogging, URLInFormatString)
{
    URL url { "https://webkit.org"_s };
    EXPECT_EQ(std::format("visiting: {}", url), "visiting: https://webkit.org/");
}

} // namespace TestWebKitAPI
