/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "Helpers/Test.h"
#include <WebCore/HTTPHeaderMap.h>
#include <utility>

using namespace WebCore;

namespace TestWebKitAPI {

TEST(HTTPHeaderMap, ComparisonOperator)
{
    HTTPHeaderMap map1;
    HTTPHeaderMap map2;

    map1.add("Cache-Control"_str, "max-age:0"_str);
    map1.add("ETag"_str, "foo"_str);
    map1.add("Not-Common1"_str, "bar"_str);
    map1.add("Not-Common2"_str, "bar"_str);

    map2.add("ETag"_str, "foo"_str);
    map2.add("Cache-Control"_str, "max-age:0"_str);
    map2.add("Not-Common2"_str, "bar"_str);
    map2.add("Not-Common1"_str, "bar"_str);

    EXPECT_TRUE(map1 == map2);
    EXPECT_FALSE(map1 != map2);

    map1.add("Last-Modified"_str, "123455"_str);
    map2.add("Last-Modified"_str, "123456"_str);
    EXPECT_FALSE(map1 == map2);
    EXPECT_TRUE(map1 != map2);

    map1.remove("Last-Modified"_str);
    map2.remove("Last-Modified"_str);
    EXPECT_TRUE(map1 == map2);
    EXPECT_FALSE(map1 != map2);

    map1.add("Not-Common3"_str, "bar"_str);
    map1.add("Not-Common3"_str, "baz"_str);
    EXPECT_FALSE(map1 == map2);
    EXPECT_TRUE(map1 != map2);

    map1.remove("Not-Common3"_str);
    map2.remove("Not-Common3"_str);
    EXPECT_TRUE(map1 == map2);
    EXPECT_FALSE(map1 != map2);

    map1.add("NOT-COMMON4"_str, "foo"_s);
    map2.add("Not-Common4"_str, "foo"_s);
    EXPECT_TRUE(map1 == map2);
    EXPECT_FALSE(map1 != map2);
}

TEST(HTTPHeaderMap, RepeatedFields)
{
    HTTPHeaderMap map;
    map.add("X-Test"_str, emptyString());
    map.add("CACHE-Control"_str, "no-cache"_str);
    map.add("x-test"_str, "one, two"_str);
    map.add("cache-control"_str, "max-age=0"_str);
    map.add("X-TEST"_str, "one, two"_str);

    EXPECT_EQ(", one, two, one, two"_str, map.get("x-test"_s));
    EXPECT_EQ("no-cache, max-age=0"_str, map.get(HTTPHeaderName::CacheControl));
    EXPECT_EQ((Vector<String> { emptyString(), "one, two"_str, "one, two"_str }), map.getAll("x-TEST"_s));
    EXPECT_EQ((Vector<String> { "no-cache"_str, "max-age=0"_str }), map.getAll(HTTPHeaderName::CacheControl));
    EXPECT_EQ(map.getAll(HTTPHeaderName::CacheControl), map.getAll("CACHE-CONTROL"_s));
    EXPECT_TRUE(map.get("Missing"_s).isNull());
    EXPECT_TRUE(map.get(HTTPHeaderName::Date).isNull());
    EXPECT_TRUE(map.getAll("Missing"_s).isEmpty());
    EXPECT_TRUE(map.getAll(HTTPHeaderName::Date).isEmpty());

    HTTPHeaderMap::CommonHeadersVector commonHeaders {
        { HTTPHeaderName::CacheControl, "no-cache"_str },
        { HTTPHeaderName::CacheControl, "max-age=0"_str },
    };
    HTTPHeaderMap::UncommonHeadersVector uncommonHeaders {
        { "X-Test"_str, emptyString() },
        { "x-test"_str, "one, two"_str },
        { "X-TEST"_str, "one, two"_str },
    };
    EXPECT_EQ(commonHeaders, map.commonHeaders());
    EXPECT_EQ(uncommonHeaders, map.uncommonHeaders());

    Vector<std::pair<String, String>> expected {
        { "Cache-Control"_str, "no-cache"_str },
        { "Cache-Control"_str, "max-age=0"_str },
        { "X-Test"_str, emptyString() },
        { "x-test"_str, "one, two"_str },
        { "X-TEST"_str, "one, two"_str },
    };
    Vector<std::pair<String, String>> fields;
    for (auto& header : map)
        fields.append({ header.key, header.value });
    EXPECT_EQ(expected, fields);

    auto combined = map.combined();
    EXPECT_EQ((HTTPHeaderMap::CommonHeadersVector { { HTTPHeaderName::CacheControl, "no-cache, max-age=0"_str } }), combined.commonHeaders());
    EXPECT_EQ((HTTPHeaderMap::UncommonHeadersVector { { "X-Test"_str, ", one, two, one, two"_str } }), combined.uncommonHeaders());
    EXPECT_EQ(map, combined);
    combined.clear();
    EXPECT_EQ(commonHeaders, map.commonHeaders());
    EXPECT_EQ(uncommonHeaders, map.uncommonHeaders());

    HTTPHeaderMap copy = map;
    EXPECT_EQ(commonHeaders, copy.commonHeaders());
    EXPECT_EQ(uncommonHeaders, copy.uncommonHeaders());
    auto isolated = map.isolatedCopy();
    EXPECT_EQ(commonHeaders, isolated.commonHeaders());
    EXPECT_EQ(uncommonHeaders, isolated.uncommonHeaders());
    auto moved = WTF::move(isolated).isolatedCopy();
    EXPECT_EQ(commonHeaders, moved.commonHeaders());
    EXPECT_EQ(uncommonHeaders, moved.uncommonHeaders());
    HTTPHeaderMap reconstructed { WTF::move(commonHeaders), WTF::move(uncommonHeaders) };
    EXPECT_EQ(map.commonHeaders(), reconstructed.commonHeaders());
    EXPECT_EQ(map.uncommonHeaders(), reconstructed.uncommonHeaders());
}

TEST(HTTPHeaderMap, ReplaceAndRemoveRepeatedFields)
{
    HTTPHeaderMap map;
    map.add("X-Test"_str, "first"_str);
    map.add("Cache-Control"_str, "no-cache"_str);
    map.add("x-test"_str, "second"_str);
    map.add("cache-control"_str, "max-age=0"_str);

    map.set("X-TEST"_str, emptyString());
    EXPECT_TRUE(map.contains("x-test"_str));
    EXPECT_FALSE(map.get("x-test"_s).isNull());
    EXPECT_TRUE(map.get("x-test"_s).isEmpty());
    EXPECT_EQ((Vector<String> { emptyString() }), map.getAll("x-test"_s));
    EXPECT_EQ(3, map.size());

    map.set("CACHE-CONTROL"_str, emptyString());
    EXPECT_FALSE(map.get(HTTPHeaderName::CacheControl).isNull());
    EXPECT_TRUE(map.get(HTTPHeaderName::CacheControl).isEmpty());
    EXPECT_EQ((Vector<String> { emptyString() }), map.getAll(HTTPHeaderName::CacheControl));
    EXPECT_EQ("Cache-Control"_str, map.begin()->key);
    EXPECT_EQ("X-Test"_str, map.uncommonHeaders()[0].key);
    EXPECT_EQ(2, map.size());

    map.add("x-test"_str, emptyString());
    map.add("cache-control"_str, emptyString());
    EXPECT_EQ(", "_str, map.get("x-test"_s));
    EXPECT_EQ(", "_str, map.get(HTTPHeaderName::CacheControl));
    auto combined = map.combined();
    EXPECT_EQ((Vector<String> { ", "_str }), combined.getAll("x-test"_s));
    EXPECT_EQ((Vector<String> { ", "_str }), combined.getAll(HTTPHeaderName::CacheControl));
    EXPECT_TRUE(map.remove("x-test"_str));
    EXPECT_FALSE(map.remove("X-Test"_str));
    EXPECT_TRUE(map.getAll("x-test"_s).isEmpty());
    EXPECT_TRUE(map.remove(HTTPHeaderName::CacheControl));
    EXPECT_FALSE(map.remove(HTTPHeaderName::CacheControl));
    EXPECT_TRUE(map.get(HTTPHeaderName::CacheControl).isNull());
    EXPECT_TRUE(map.isEmpty());
}

TEST(HTTPHeaderMap, EffectiveComparisonWithRepeatedFields)
{
    HTTPHeaderMap map1;
    map1.add("X-Test"_str, "first"_str);
    map1.add("Cache-Control"_str, "no-cache"_str);
    map1.add("x-test"_str, "second"_str);

    HTTPHeaderMap map2;
    map2.add("cache-control"_str, "no-cache"_str);
    map2.add("X-TEST"_str, "first, second"_str);
    EXPECT_TRUE(map1 == map2);
    EXPECT_TRUE(map2 == map1);

    map2.set("X-Test"_str, "second, first"_str);
    EXPECT_FALSE(map1 == map2);
    EXPECT_FALSE(map2 == map1);

    map2.set("X-Test"_str, "first, second"_str);
    map1.add("X-Test"_str, "second"_str);
    EXPECT_FALSE(map1 == map2);
    map2.add("x-test"_str, "second"_str);
    EXPECT_TRUE(map1 == map2);
    EXPECT_TRUE(map2 == map1);
}

}
