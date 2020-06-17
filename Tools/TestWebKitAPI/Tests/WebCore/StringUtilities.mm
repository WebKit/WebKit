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
    JSC::initializeThreading();

    String wildcardString = "a*b";
    EXPECT_TRUE(stringMatchesWildcardString("aaaabb", "a*b"));
    EXPECT_TRUE(stringMatchesWildcardString("ab", wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("ba", wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("", wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("a", wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("b", wildcardString));

    wildcardString = "aabb";
    EXPECT_TRUE(stringMatchesWildcardString("aabb", wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("a*b", wildcardString));

    wildcardString = "*apple*";
    EXPECT_TRUE(stringMatchesWildcardString("freshapple", wildcardString));
    EXPECT_TRUE(stringMatchesWildcardString("applefresh", wildcardString));
    EXPECT_TRUE(stringMatchesWildcardString("freshapplefresh", wildcardString));

    wildcardString = "a*b*c";
    EXPECT_TRUE(stringMatchesWildcardString("aabbcc", wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("bca", wildcardString));

    wildcardString = "*.example*.com/*";
    EXPECT_TRUE(stringMatchesWildcardString("food.examplehello.com/", wildcardString));
    EXPECT_TRUE(stringMatchesWildcardString("food.example.com/bar.html", wildcardString));
    EXPECT_TRUE(stringMatchesWildcardString("foo.bar.example.com/hello?query#fragment.html", wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("food.example.com", wildcardString));

    wildcardString = "a*b.b*c";
    EXPECT_TRUE(stringMatchesWildcardString("aabb.bbcc", wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("aabbcbbcc", wildcardString));

    wildcardString = "a+b";
    EXPECT_TRUE(stringMatchesWildcardString("a+b", wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("ab", wildcardString));

    wildcardString = "(a*)b aa";
    EXPECT_TRUE(stringMatchesWildcardString("(aaa)b aa", wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("aab aa", wildcardString));

    wildcardString = "a|c";
    EXPECT_TRUE(stringMatchesWildcardString("a|c", wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("abc", wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("a", wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("c", wildcardString));

    wildcardString = "(a+|b)*";
    EXPECT_TRUE(stringMatchesWildcardString("(a+|b)acca", "(a+|b)*"));
    EXPECT_FALSE(stringMatchesWildcardString("ab", "(a+|b)*"));

    wildcardString = "a[-]?c";
    EXPECT_TRUE(stringMatchesWildcardString("a[-]?c", wildcardString));
    EXPECT_FALSE(stringMatchesWildcardString("ac", wildcardString));

    EXPECT_FALSE(stringMatchesWildcardString("hello", "^hello$"));
    EXPECT_TRUE(stringMatchesWildcardString(" ", " "));
    EXPECT_TRUE(stringMatchesWildcardString("^$", "^$"));
    EXPECT_FALSE(stringMatchesWildcardString("a", "a{1}"));

    // stringMatchesWildcardString should only match the entire string.
    EXPECT_FALSE(stringMatchesWildcardString("aabbaabb", "aabb"));
    EXPECT_FALSE(stringMatchesWildcardString("aabb\naabb", "aabb"));
    EXPECT_FALSE(stringMatchesWildcardString("bab", "a*"));
    EXPECT_FALSE(stringMatchesWildcardString("bab", "*a"));
    EXPECT_FALSE(stringMatchesWildcardString("bab", "a*b"));
    EXPECT_FALSE(stringMatchesWildcardString("abcd", "b*c"));
}

}
