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
#include "WebFoundTextRange.h"

#include <optional>
#include <wtf/HashMap.h>

namespace TestWebKitAPI {

using WebKit::WebFoundTextRange;

using ValidatedFoundRangeMap = HashMap<WebFoundTextRange, std::optional<unsigned>>;

static WebFoundTextRange zeroLengthMainFrameMatch()
{
    return WebFoundTextRange {
        WebFoundTextRange::DOMData { 0, 0 },
        Vector<uint64_t> { },
        0
    };
}

TEST(WebFoundTextRange, EmptyValueDoesNotAliasZeroLengthMainFrameMatch)
{
    auto emptyValue = WTF::HashTraits<WebFoundTextRange>::emptyValue();
    EXPECT_FALSE(emptyValue == zeroLengthMainFrameMatch());
}

TEST(WebFoundTextRange, InsertingZeroLengthMainFrameMatchDoesNotCrash)
{
    ValidatedFoundRangeMap map;
    auto key = zeroLengthMainFrameMatch();

    auto addResult = map.add(key, 42u);
    EXPECT_TRUE(addResult.isNewEntry);

    EXPECT_EQ(map.size(), 1u);
    EXPECT_TRUE(map.contains(key));
    EXPECT_EQ(map.get(key), std::optional<unsigned> { 42u });
}

TEST(WebFoundTextRange, ValidatedMapSupportsMultipleDOMMatches)
{
    ValidatedFoundRangeMap map;

    map.add(zeroLengthMainFrameMatch(), 100u);
    map.add(WebFoundTextRange { WebFoundTextRange::DOMData { 5, 3 }, Vector<uint64_t> { }, 1 }, 200u);
    map.add(WebFoundTextRange { WebFoundTextRange::DOMData { 0, 0 }, Vector<uint64_t> { 2 }, 0 }, 300u);

    EXPECT_EQ(map.size(), 3u);
    EXPECT_EQ(map.get(zeroLengthMainFrameMatch()), std::optional<unsigned> { 100u });

    EXPECT_TRUE(map.remove(zeroLengthMainFrameMatch()));
    EXPECT_EQ(map.size(), 2u);
    EXPECT_FALSE(map.contains(zeroLengthMainFrameMatch()));
}

} // namespace TestWebKitAPI
