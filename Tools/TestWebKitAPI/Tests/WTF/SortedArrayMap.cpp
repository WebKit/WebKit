/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include <wtf/SortedArrayMap.h>

TEST(WTF, SortedArraySet)
{
    static constexpr ComparableCaseFoldingASCIILiteral caseFoldingArray[] = {
        "_",
        "a",
        "c",
        "delightful",
        "q",
        "q_",
        "r/y",
        "s-z",
    };
    static constexpr SortedArraySet caseFoldingSet { caseFoldingArray };

    static constexpr ComparableLettersLiteral lettersArray[] = {
        "a",
        "c",
        "delightful",
        "q",
        "r/y",
        "s-z",
    };
    static constexpr SortedArraySet lettersSet { lettersArray };

    static constexpr ComparableLettersLiteral scriptTypesArray[] = {
        "application/ecmascript",
        "application/javascript",
        "application/x-ecmascript",
        "application/x-javascript",
        "text/ecmascript",
        "text/javascript",
        "text/javascript1.0",
        "text/javascript1.1",
        "text/javascript1.2",
        "text/javascript1.3",
        "text/javascript1.4",
        "text/javascript1.5",
        "text/jscript",
        "text/livescript",
        "text/x-ecmascript",
        "text/x-javascript",
    };
    static constexpr SortedArraySet scriptTypesSet { scriptTypesArray };

    EXPECT_FALSE(caseFoldingSet.contains(""));
    EXPECT_TRUE(caseFoldingSet.contains("_"));
    EXPECT_TRUE(caseFoldingSet.contains("c"));
    EXPECT_TRUE(caseFoldingSet.contains("delightful"));
    EXPECT_FALSE(caseFoldingSet.contains("d"));
    EXPECT_TRUE(caseFoldingSet.contains("q_"));
    EXPECT_FALSE(caseFoldingSet.contains("q__"));

    EXPECT_FALSE(lettersSet.contains(""));
    EXPECT_FALSE(lettersSet.contains("_"));
    EXPECT_TRUE(lettersSet.contains("c"));
    EXPECT_TRUE(lettersSet.contains("delightful"));
    EXPECT_FALSE(lettersSet.contains("d"));
    EXPECT_FALSE(lettersSet.contains("q_"));
    EXPECT_FALSE(lettersSet.contains("q__"));

    ASSERT_TRUE(scriptTypesSet.contains("text/javascript"));
    ASSERT_TRUE(scriptTypesSet.contains("TEXT/JAVASCRIPT"));
    ASSERT_TRUE(scriptTypesSet.contains("application/javascript"));
    ASSERT_TRUE(scriptTypesSet.contains("application/ecmascript"));
    ASSERT_TRUE(scriptTypesSet.contains("application/x-javascript"));
    ASSERT_TRUE(scriptTypesSet.contains("application/x-ecmascript"));
    ASSERT_FALSE(scriptTypesSet.contains("text/plain"));
    ASSERT_FALSE(scriptTypesSet.contains("application/json"));
    ASSERT_FALSE(scriptTypesSet.contains("foo/javascript"));
}
