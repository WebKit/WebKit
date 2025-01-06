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
        "_"_s,
        "a"_s,
        "c"_s,
        "delightful"_s,
        "q"_s,
        "q_"_s,
        "r/y"_s,
        "s-z"_s,
    };
    static constexpr SortedArraySet caseFoldingSet { caseFoldingArray };

    static constexpr ComparableLettersLiteral lettersArray[] = {
        "a"_s,
        "c"_s,
        "delightful"_s,
        "q"_s,
        "r/y"_s,
        "s-z"_s,
    };
    static constexpr SortedArraySet lettersSet { lettersArray };

    static constexpr ComparableLettersLiteral scriptTypesArray[] = {
        "application/ecmascript"_s,
        "application/javascript"_s,
        "application/x-ecmascript"_s,
        "application/x-javascript"_s,
        "text/ecmascript"_s,
        "text/javascript"_s,
        "text/javascript1.0"_s,
        "text/javascript1.1"_s,
        "text/javascript1.2"_s,
        "text/javascript1.3"_s,
        "text/javascript1.4"_s,
        "text/javascript1.5"_s,
        "text/jscript"_s,
        "text/livescript"_s,
        "text/x-ecmascript"_s,
        "text/x-javascript"_s,
    };
    static constexpr SortedArraySet scriptTypesSet { scriptTypesArray };

    EXPECT_FALSE(caseFoldingSet.contains(""_s));
    EXPECT_TRUE(caseFoldingSet.contains("_"_s));
    EXPECT_TRUE(caseFoldingSet.contains("c"_s));
    EXPECT_TRUE(caseFoldingSet.contains("delightful"_s));
    EXPECT_FALSE(caseFoldingSet.contains("d"_s));
    EXPECT_TRUE(caseFoldingSet.contains("q_"_s));
    EXPECT_FALSE(caseFoldingSet.contains("q__"_s));

    EXPECT_FALSE(lettersSet.contains(""_s));
    EXPECT_FALSE(lettersSet.contains("_"_s));
    EXPECT_TRUE(lettersSet.contains("c"_s));
    EXPECT_TRUE(lettersSet.contains("delightful"_s));
    EXPECT_FALSE(lettersSet.contains("d"_s));
    EXPECT_FALSE(lettersSet.contains("q_"_s));
    EXPECT_FALSE(lettersSet.contains("q__"_s));

    ASSERT_TRUE(scriptTypesSet.contains("text/javascript"_s));
    ASSERT_TRUE(scriptTypesSet.contains("TEXT/JAVASCRIPT"_s));
    ASSERT_TRUE(scriptTypesSet.contains("application/javascript"_s));
    ASSERT_TRUE(scriptTypesSet.contains("application/ecmascript"_s));
    ASSERT_TRUE(scriptTypesSet.contains("application/x-javascript"_s));
    ASSERT_TRUE(scriptTypesSet.contains("application/x-ecmascript"_s));
    ASSERT_FALSE(scriptTypesSet.contains("text/plain"_s));
    ASSERT_FALSE(scriptTypesSet.contains("application/json"_s));
    ASSERT_FALSE(scriptTypesSet.contains("foo/javascript"_s));
}
