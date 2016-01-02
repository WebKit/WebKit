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

#include "DFAHelpers.h"
#include <WebCore/DFACombiner.h>
#include <wtf/MainThread.h>

using namespace WebCore;
using namespace ContentExtensions;

namespace TestWebKitAPI {

class DFACombinerTest : public testing::Test {
public:
    virtual void SetUp()
    {
        WTF::initializeMainThread();
    }
};

Vector<DFA> combine(Vector<DFA> dfas, unsigned minimumSize)
{
    DFACombiner combiner;
    for (DFA& dfa : dfas)
        combiner.addDFA(WTFMove(dfa));

    Vector<DFA> output;
    combiner.combineDFAs(minimumSize, [&output](DFA&& dfa) {
        output.append(dfa);
    });
    return output;
}

TEST_F(DFACombinerTest, Basic)
{
    Vector<DFA> dfas = { buildDFAFromPatterns({ "foo"}), buildDFAFromPatterns({ "bar"}) };
    Vector<DFA> combinedDFAs = combine(dfas, 10000);
    EXPECT_EQ(static_cast<size_t>(1), combinedDFAs.size());

    DFA reference = buildDFAFromPatterns({ "foo", "bar"});
    reference.minimize();
    EXPECT_EQ(countLiveNodes(reference), countLiveNodes(combinedDFAs.first()));
}


TEST_F(DFACombinerTest, IdenticalDFAs)
{
    Vector<DFA> dfas = { buildDFAFromPatterns({ "foo"}), buildDFAFromPatterns({ "foo"}) };
    Vector<DFA> combinedDFAs = combine(dfas, 10000);
    EXPECT_EQ(static_cast<size_t>(1), combinedDFAs.size());

    // The result should have the exact same size as the minimized input.
    DFA reference = buildDFAFromPatterns({ "foo"});
    reference.minimize();
    EXPECT_EQ(countLiveNodes(reference), countLiveNodes(combinedDFAs.first()));
}

TEST_F(DFACombinerTest, NoInput)
{
    DFACombiner combiner;
    unsigned counter = 0;
    combiner.combineDFAs(100000, [&counter](DFA&& dfa) {
        ++counter;
    });
    EXPECT_EQ(static_cast<unsigned>(0), counter);
}

TEST_F(DFACombinerTest, SingleInput)
{
    Vector<DFA> dfas = { buildDFAFromPatterns({ "WebKit"}) };
    Vector<DFA> combinedDFAs = combine(dfas, 10000);
    EXPECT_EQ(static_cast<size_t>(1), combinedDFAs.size());

    DFA reference = buildDFAFromPatterns({ "WebKit"});
    reference.minimize();
    EXPECT_EQ(countLiveNodes(reference), countLiveNodes(combinedDFAs.first()));
}

TEST_F(DFACombinerTest, InputTooLargeForMinimumSize)
{
    Vector<DFA> dfas = { buildDFAFromPatterns({ "foo"}), buildDFAFromPatterns({ "bar"}) };
    Vector<DFA> combinedDFAs = combine(dfas, 2);
    EXPECT_EQ(static_cast<size_t>(2), combinedDFAs.size());
    EXPECT_EQ(static_cast<size_t>(4), countLiveNodes(combinedDFAs[0]));
    EXPECT_EQ(static_cast<size_t>(4), countLiveNodes(combinedDFAs[1]));
}

TEST_F(DFACombinerTest, CombinedInputReachesMinimumSize)
{
    Vector<DFA> dfas = { buildDFAFromPatterns({ "foo"}), buildDFAFromPatterns({ "bar"}), buildDFAFromPatterns({ "WebKit"}) };
    Vector<DFA> combinedDFAs = combine(dfas, 5);
    EXPECT_EQ(static_cast<size_t>(2), combinedDFAs.size());
    EXPECT_EQ(static_cast<size_t>(7), countLiveNodes(combinedDFAs[0]));
    EXPECT_EQ(static_cast<size_t>(6), countLiveNodes(combinedDFAs[1]));
}

} // namespace TestWebKitAPI
