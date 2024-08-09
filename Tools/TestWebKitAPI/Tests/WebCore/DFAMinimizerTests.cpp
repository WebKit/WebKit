/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#if ENABLE(CONTENT_EXTENSIONS)

#include "DFAHelpers.h"
#include <wtf/MainThread.h>

namespace TestWebKitAPI {

class DFAMinimizerTest : public testing::Test {
public:
    virtual void SetUp()
    {
        WTF::initializeMainThread();
    }
};

TEST_F(DFAMinimizerTest, BasicSearch)
{
    ContentExtensions::DFA dfa = buildDFAFromPatterns({ ".*foo"_s, ".*bar"_s, ".*bang"_s });
    EXPECT_EQ(static_cast<size_t>(8), countLiveNodes(dfa));
    dfa.minimize();
    EXPECT_EQ(static_cast<size_t>(7), countLiveNodes(dfa));
}

TEST_F(DFAMinimizerTest, MergeSuffixes)
{
    ContentExtensions::DFA dfa = buildDFAFromPatterns({ ".*aaa"_s, ".*aab"_s, ".*aba"_s, ".*abb"_s, ".*baa"_s, ".*bab"_s, ".*bba"_s, ".*bbb"_s });
    EXPECT_EQ(static_cast<size_t>(12), countLiveNodes(dfa));
    dfa.minimize();
    EXPECT_EQ(static_cast<size_t>(4), countLiveNodes(dfa));
}

TEST_F(DFAMinimizerTest, MergeInfixes)
{
    ContentExtensions::DFA dfa = buildDFAFromPatterns({ ".*aaakit"_s, ".*aabkit"_s, ".*abakit"_s, ".*abbkit"_s, ".*baakit"_s, ".*babkit"_s, ".*bbakit"_s, ".*bbbkit"_s });
    EXPECT_EQ(static_cast<size_t>(15), countLiveNodes(dfa));
    dfa.minimize();
    EXPECT_EQ(static_cast<size_t>(7), countLiveNodes(dfa));
}

TEST_F(DFAMinimizerTest, FallbackTransitionsWithDifferentiatorDoNotMerge1)
{
    ContentExtensions::DFA dfa = buildDFAFromPatterns({ "^a.a"_s, "^b.a"_s, "^bac"_s, "^bbc"_s, "^BCC"_s });
    EXPECT_EQ(static_cast<size_t>(6), countLiveNodes(dfa));
    dfa.minimize();
    EXPECT_EQ(static_cast<size_t>(6), countLiveNodes(dfa));
}

TEST_F(DFAMinimizerTest, FallbackTransitionsWithDifferentiatorDoNotMerge2)
{
    ContentExtensions::DFA dfa = buildDFAFromPatterns({ "^bbc"_s, "^BCC"_s, "^a.a"_s, "^b.a"_s });
    EXPECT_EQ(static_cast<size_t>(6), countLiveNodes(dfa));
    dfa.minimize();
    EXPECT_EQ(static_cast<size_t>(6), countLiveNodes(dfa));
}

TEST_F(DFAMinimizerTest, FallbackTransitionsWithDifferentiatorDoNotMerge3)
{
    ContentExtensions::DFA dfa = buildDFAFromPatterns({ "^a.c"_s, "^b.c"_s, "^baa"_s, "^bba"_s, "^BCA"_s });
    EXPECT_EQ(static_cast<size_t>(6), countLiveNodes(dfa));
    dfa.minimize();
    EXPECT_EQ(static_cast<size_t>(6), countLiveNodes(dfa));
}

TEST_F(DFAMinimizerTest, FallbackTransitionsWithDifferentiatorDoNotMerge4)
{
    ContentExtensions::DFA dfa = buildDFAFromPatterns({ "^baa"_s, "^bba"_s, "^BCA"_s, "^a.c"_s, "^b.c"_s });
    EXPECT_EQ(static_cast<size_t>(6), countLiveNodes(dfa));
    dfa.minimize();
    EXPECT_EQ(static_cast<size_t>(6), countLiveNodes(dfa));
}

TEST_F(DFAMinimizerTest, FallbackTransitionsToOtherNodeInSameGroupDoesNotDifferentiateGroup)
{
    ContentExtensions::DFA dfa = buildDFAFromPatterns({ "^aac"_s, "^a.c"_s, "^b.c"_s });
    EXPECT_EQ(static_cast<size_t>(5), countLiveNodes(dfa));
    dfa.minimize();
    EXPECT_EQ(static_cast<size_t>(4), countLiveNodes(dfa));
}

TEST_F(DFAMinimizerTest, SimpleFallBackTransitionDifferentiator1)
{
    ContentExtensions::DFA dfa = buildDFAFromPatterns({ "^a.bc.de"_s, "^a.bd.ef"_s });
    EXPECT_EQ(static_cast<size_t>(11), countLiveNodes(dfa));
    dfa.minimize();
    EXPECT_EQ(static_cast<size_t>(11), countLiveNodes(dfa));
}

TEST_F(DFAMinimizerTest, SimpleFallBackTransitionDifferentiator2)
{
    ContentExtensions::DFA dfa = buildDFAFromPatterns({ "^cb."_s, "^db.b"_s });
    EXPECT_EQ(static_cast<size_t>(7), countLiveNodes(dfa));
    dfa.minimize();
    EXPECT_EQ(static_cast<size_t>(7), countLiveNodes(dfa));
}

} // namespace TestWebKitAPI

#endif // ENABLE(CONTENT_EXTENSIONS)
