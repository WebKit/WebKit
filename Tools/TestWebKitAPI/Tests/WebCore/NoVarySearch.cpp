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

#include <WebCore/NoVarySearch.h>
#include <wtf/URL.h>

namespace TestWebKitAPI {

using WebCore::NoVarySearch;

static bool equivalent(ASCIILiteral header, ASCIILiteral a, ASCIILiteral b)
{
    return NoVarySearch::parse(header).equivalent(URL { String { a } }, URL { String { b } });
}

TEST(NoVarySearch, ParseDefault)
{
    EXPECT_TRUE(NoVarySearch::parse(""_s).isDefault());
    EXPECT_TRUE(NoVarySearch::parse("key-order=?0"_s).isDefault());
    EXPECT_TRUE(NoVarySearch::parse("params=()"_s).isDefault());
    EXPECT_TRUE(NoVarySearch::parse("unknown"_s).isDefault());
}

TEST(NoVarySearch, ParseInvalid)
{
    for (auto header : { "key-order=\"not a boolean\""_s, "params=\"not an inner list\""_s, "params=(not-a-string)"_s, "params=(\"a\"), except=(\"x\")"_s, "except=\"not an inner list\""_s, "except=(not-a-string)"_s, "except=?1"_s, "params(\"a\")"_s, "params=1"_s, "params=?0, except=(\"x\")"_s })
        EXPECT_TRUE(NoVarySearch::parse(header).isDefault()) << header.characters();
}

TEST(NoVarySearch, ParseValid)
{
    auto keyOrder = NoVarySearch::parse("key-order"_s);
    EXPECT_FALSE(keyOrder.varyOnKeyOrder());
    EXPECT_FALSE(keyOrder.noVaryParamsIsWildcard());
    EXPECT_TRUE(keyOrder.noVaryParams().isEmpty());

    auto paramsList = NoVarySearch::parse("params=(\"a\" \"b\")"_s);
    EXPECT_TRUE(paramsList.varyOnKeyOrder());
    EXPECT_EQ(2u, paramsList.noVaryParams().size());

    auto paramsAndExcept = NoVarySearch::parse("params, except=(\"c\")"_s);
    EXPECT_TRUE(paramsAndExcept.noVaryParamsIsWildcard());
    EXPECT_EQ(1u, paramsAndExcept.varyParams().size());

    auto exceptOnly = NoVarySearch::parse("except=(\"x\"), key-order"_s);
    EXPECT_TRUE(exceptOnly.noVaryParamsIsWildcard());
    EXPECT_FALSE(exceptOnly.varyOnKeyOrder());

    // Parameters on members are ignored.
    EXPECT_TRUE(NoVarySearch::parse("params;unknown"_s).noVaryParamsIsWildcard());
}

TEST(NoVarySearch, ParseKeyDecoding)
{
    auto config = NoVarySearch::parse("params=(\"%C3%A9+%E6%B0%97\")"_s);
    ASSERT_EQ(1u, config.noVaryParams().size());
    EXPECT_EQ(String::fromUTF8("\xC3\xA9 \xE6\xB0\x97"), config.noVaryParams()[0]);
}

TEST(NoVarySearch, DefaultComparisonIsExact)
{
    EXPECT_TRUE(equivalent(""_s, "https://example.com/a?x=1"_s, "https://example.com/a?x=1"_s));
    EXPECT_FALSE(equivalent(""_s, "https://example.com/a"_s, "https://example.com/a?"_s));
    EXPECT_FALSE(equivalent(""_s, "https://example.com/foo?a=b&&&c"_s, "https://example.com/foo?a=b&c="_s));
}

TEST(NoVarySearch, DifferentPathOrOrigin)
{
    EXPECT_FALSE(equivalent("params"_s, "https://example.com/a"_s, "https://example.com/b"_s));
    EXPECT_FALSE(equivalent("params"_s, "https://example.com/a"_s, "https://example.org/a"_s));
    EXPECT_FALSE(equivalent("params"_s, "https://example.com/a"_s, "http://example.com/a"_s));
    EXPECT_FALSE(equivalent("params"_s, "https://example.com/a"_s, "https://example.com:8443/a"_s));
}

TEST(NoVarySearch, KeyOrder)
{
    EXPECT_TRUE(equivalent("key-order"_s, "https://example.com/?a=1&b=2&c=3"_s, "https://example.com/?b=2&a=1&c=3"_s));
    EXPECT_FALSE(equivalent("key-order"_s, "https://example.com/?a=1&b=2&c=3"_s, "https://example.com/?b=2&a=1&c=3&d=4"_s));
    // Order of values for the same key still matters.
    EXPECT_FALSE(equivalent("key-order"_s, "https://example.com/?a=1&a=2"_s, "https://example.com/?a=2&a=1"_s));
    EXPECT_TRUE(equivalent("key-order"_s, "https://example.com/?b=5&a=3&a=4&b=3"_s, "https://example.com/?a=3&b=5&b=3&a=4"_s));
}

TEST(NoVarySearch, ParamsList)
{
    EXPECT_TRUE(equivalent("params=(\"id\")"_s, "https://example.com/users"_s, "https://example.com/users?id=345"_s));
    EXPECT_TRUE(equivalent("params=(\"id\")"_s, "https://example.com/users?id=1&b=2"_s, "https://example.com/users?id=3&b=2"_s));
    EXPECT_FALSE(equivalent("params=(\"id\")"_s, "https://example.com/users?b=1"_s, "https://example.com/users?b=2"_s));
}

TEST(NoVarySearch, ParamsWildcardAndExcept)
{
    EXPECT_TRUE(equivalent("params"_s, "https://example.com/a?x=1"_s, "https://example.com/a?y=2&z=3"_s));
    EXPECT_TRUE(equivalent("params, except=(\"id\")"_s, "https://example.com/a?id=1&x=1"_s, "https://example.com/a?id=1&y=2"_s));
    EXPECT_FALSE(equivalent("params, except=(\"id\")"_s, "https://example.com/a?id=1"_s, "https://example.com/a?id=2"_s));
    EXPECT_FALSE(equivalent("params, except=(\"id\")"_s, "https://example.com/a"_s, "https://example.com/a?id=2"_s));
    EXPECT_TRUE(equivalent("except=(\"id\")"_s, "https://example.com/a?x=1"_s, "https://example.com/a"_s));
}

TEST(NoVarySearch, CanonicalizationOfQueries)
{
    EXPECT_TRUE(equivalent("key-order"_s, "https://example.com/"_s, "https://example.com/?"_s));
    EXPECT_TRUE(equivalent("key-order"_s, "https://example.com/?a=x"_s, "https://example.com/?%61=%78"_s));
    EXPECT_TRUE(equivalent("key-order"_s, "https://example.com/?a=%f6"_s, "https://example.com/?a=%ef%bf%bd"_s));
    EXPECT_TRUE(equivalent("key-order"_s, "https://example.com/?a=x&&&&"_s, "https://example.com/?a=x"_s));
    EXPECT_TRUE(equivalent("key-order"_s, "https://example.com/?a="_s, "https://example.com/?a"_s));
    EXPECT_TRUE(equivalent("key-order"_s, "https://example.com/?a=%20"_s, "https://example.com/?a=+"_s));
    // No Unicode normalization.
    EXPECT_FALSE(equivalent("key-order"_s, "https://example.com/?a=%C3%A9"_s, "https://example.com/?a=e%CC%81"_s));
}

TEST(NoVarySearch, EncodedKeyInList)
{
    auto header = "params=(\"%C3%A9+%E6%B0%97\")"_s;
    EXPECT_TRUE(equivalent(header, "https://example.com/?%C3%A9%20%E6%B0%97=1"_s, "https://example.com/?%C3%A9+%E6%B0%97=2"_s));
    EXPECT_TRUE(equivalent(header, "https://example.com/"_s, "https://example.com/?%C3%A9+%E6%B0%97=4"_s));
}

} // namespace TestWebKitAPI
