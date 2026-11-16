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
#include <WebCore/TextIterator.h>
#include <wtf/text/StringBuilder.h>

namespace TestWebKitAPI {

using namespace WebCore;

// SearchBuffer's capacity is max(target.length() * 8, 8192) characters, so for a
// short target a document longer than 8192 characters spans more than one buffer
// fill. These cover both the in-buffer case and the multi-fill streaming case.

TEST(TextIteratorTest, ContainsPlainTextMatchWithinFirstBuffer)
{
    EXPECT_TRUE(containsPlainText("the quick brown fox"_s, "brown"_s, { }));
}

TEST(TextIteratorTest, ContainsPlainTextNoMatch)
{
    EXPECT_FALSE(containsPlainText("the quick brown fox"_s, "purple"_s, { }));
}

TEST(TextIteratorTest, ContainsPlainTextMatchPastFirstBufferFill)
{
    StringBuilder builder;
    for (unsigned i = 0; i < 20000; ++i)
        builder.append('a');
    builder.append("needle"_s);
    auto document = builder.toString();

    EXPECT_TRUE(containsPlainText(document, "needle"_s, { }));
    EXPECT_FALSE(containsPlainText(document, "haystack"_s, { }));
}

} // namespace TestWebKitAPI
