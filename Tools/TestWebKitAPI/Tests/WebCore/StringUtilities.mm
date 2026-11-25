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

    RefPtr<StringImpl> wildcardString = "a*b"_si;
    EXPECT_TRUE(stringMatchesWildcardString("aaaabb"_si, "a*b"_si));
    EXPECT_TRUE(stringMatchesWildcardString("ab"_si, wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("ba"_si, wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString(emptyString(), wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("a"_si, wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("b"_si, wildcardString));

    wildcardString = "aabb"_si;
    EXPECT_TRUE(stringMatchesWildcardString("aabb"_si, wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("a*b"_si, wildcardString));

    wildcardString = "*apple*"_si;
    EXPECT_TRUE(stringMatchesWildcardString("freshapple"_si, wildcardString));
    EXPECT_TRUE(stringMatchesWildcardString("applefresh"_si, wildcardString));
    EXPECT_TRUE(stringMatchesWildcardString("freshapplefresh"_si, wildcardString));

    wildcardString = "a*b*c"_si;
    EXPECT_TRUE(stringMatchesWildcardString("aabbcc"_si, wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("bca"_si, wildcardString));

    wildcardString = "*.example*.com/*"_si;
    EXPECT_TRUE(stringMatchesWildcardString("food.examplehello.com/"_si, wildcardString));
    EXPECT_TRUE(stringMatchesWildcardString("food.example.com/bar.html"_si, wildcardString));
    EXPECT_TRUE(stringMatchesWildcardString("foo.bar.example.com/hello?query#fragment.html"_si, wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("food.example.com"_si, wildcardString));

    wildcardString = "a*b.b*c"_si;
    EXPECT_TRUE(stringMatchesWildcardString("aabb.bbcc"_si, wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("aabbcbbcc"_si, wildcardString));

    wildcardString = "a+b"_si;
    EXPECT_TRUE(stringMatchesWildcardString("a+b"_si, wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("ab"_si, wildcardString));

    wildcardString = "(a*)b aa"_si;
    EXPECT_TRUE(stringMatchesWildcardString("(aaa)b aa"_si, wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("aab aa"_si, wildcardString));

    wildcardString = "a|c"_si;
    EXPECT_TRUE(stringMatchesWildcardString("a|c"_si, wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("abc"_si, wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("a"_si, wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("c"_si, wildcardString));

    wildcardString = "(a+|b)*"_si;
    EXPECT_TRUE(stringMatchesWildcardString("(a+|b)acca"_si, "(a+|b)*"_si));
    EXPECT_FALSE(stringMatchesWildcardString("ab"_si, "(a+|b)*"_si));

    wildcardString = "a[-]?c"_si;
    EXPECT_TRUE(stringMatchesWildcardString("a[-]?c"_si, wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("ac"_si, wildcardString));

    EXPECT_FALSE(stringMatchesWildcardString("hello"_si, "^hello$"_si));
    EXPECT_TRUE(stringMatchesWildcardString(" "_si, " "_si));
    EXPECT_TRUE(stringMatchesWildcardString("^$"_si, "^$"_si));
    EXPECT_FALSE(stringMatchesWildcardString("a"_si, "a{1}"_si));

    // stringMatchesWildcardString should only match the entire string.
    EXPECT_FALSE(stringMatchesWildcardString("aabbaabb"_si, "aabb"_si));
    EXPECT_FALSE(stringMatchesWildcardString("aabb\naabb"_si, "aabb"_si));
    EXPECT_FALSE(stringMatchesWildcardString("bab"_si, "a*"_si));
    EXPECT_FALSE(stringMatchesWildcardString("bab"_si, "*a"_si));
    EXPECT_FALSE(stringMatchesWildcardString("bab"_si, "a*b"_si));
    EXPECT_FALSE(stringMatchesWildcardString("abcd"_si, "b*c"_si));
}

}
