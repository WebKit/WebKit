/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#import "config.h"

#import <JavaScriptCore/InitializeThreading.h>
#import <WebCore/StringUtilities.h>
#import <wtf/text/WTFString.h>

using namespace WebCore;

namespace TestWebKitAPI {

TEST(WebCore, WildcardStringMatching)
{
    JSC::initialize();

    String wildcardString = "a*b"_s;
    EXPECT_TRUE(stringMatchesWildcardString("aaaabb"_s, "a*b"_s));
    EXPECT_TRUE(stringMatchesWildcardString("ab"_s, wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("ba"_s, wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString(emptyString(), wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("a"_s, wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("b"_s, wildcardString));

    wildcardString = "aabb"_s;
    EXPECT_TRUE(stringMatchesWildcardString("aabb"_s, wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("a*b"_s, wildcardString));

    wildcardString = "*apple*"_s;
    EXPECT_TRUE(stringMatchesWildcardString("freshapple"_s, wildcardString));
    EXPECT_TRUE(stringMatchesWildcardString("applefresh"_s, wildcardString));
    EXPECT_TRUE(stringMatchesWildcardString("freshapplefresh"_s, wildcardString));

    wildcardString = "a*b*c"_s;
    EXPECT_TRUE(stringMatchesWildcardString("aabbcc"_s, wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("bca"_s, wildcardString));

    wildcardString = "*.example*.com/*"_s;
    EXPECT_TRUE(stringMatchesWildcardString("food.examplehello.com/"_s, wildcardString));
    EXPECT_TRUE(stringMatchesWildcardString("food.example.com/bar.html"_s, wildcardString));
    EXPECT_TRUE(stringMatchesWildcardString("foo.bar.example.com/hello?query#fragment.html"_s, wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("food.example.com"_s, wildcardString));

    wildcardString = "a*b.b*c"_s;
    EXPECT_TRUE(stringMatchesWildcardString("aabb.bbcc"_s, wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("aabbcbbcc"_s, wildcardString));

    wildcardString = "a+b"_s;
    EXPECT_TRUE(stringMatchesWildcardString("a+b"_s, wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("ab"_s, wildcardString));

    wildcardString = "(a*)b aa"_s;
    EXPECT_TRUE(stringMatchesWildcardString("(aaa)b aa"_s, wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("aab aa"_s, wildcardString));

    wildcardString = "a|c"_s;
    EXPECT_TRUE(stringMatchesWildcardString("a|c"_s, wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("abc"_s, wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("a"_s, wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("c"_s, wildcardString));

    wildcardString = "(a+|b)*"_s;
    EXPECT_TRUE(stringMatchesWildcardString("(a+|b)acca"_s, "(a+|b)*"_s));
    EXPECT_FALSE(stringMatchesWildcardString("ab"_s, "(a+|b)*"_s));

    wildcardString = "a[-]?c"_s;
    EXPECT_TRUE(stringMatchesWildcardString("a[-]?c"_s, wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("ac"_s, wildcardString));

    EXPECT_FALSE(stringMatchesWildcardString("hello"_s, "^hello$"_s));
    EXPECT_TRUE(stringMatchesWildcardString(" "_s, " "_s));
    EXPECT_TRUE(stringMatchesWildcardString("^$"_s, "^$"_s));
    EXPECT_FALSE(stringMatchesWildcardString("a"_s, "a{1}"_s));

    // stringMatchesWildcardString should only match the entire string.
    EXPECT_FALSE(stringMatchesWildcardString("aabbaabb"_s, "aabb"_s));
    EXPECT_FALSE(stringMatchesWildcardString("aabb\naabb"_s, "aabb"_s));
    EXPECT_FALSE(stringMatchesWildcardString("bab"_s, "a*"_s));
    EXPECT_FALSE(stringMatchesWildcardString("bab"_s, "*a"_s));
    EXPECT_FALSE(stringMatchesWildcardString("bab"_s, "a*b"_s));
    EXPECT_FALSE(stringMatchesWildcardString("abcd"_s, "b*c"_s));
}

}
