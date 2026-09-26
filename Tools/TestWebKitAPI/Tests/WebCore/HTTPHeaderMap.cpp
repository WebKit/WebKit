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

using namespace WebCore;

namespace TestWebKitAPI {

template<typename Key> static Vector<String> headerValues(const HTTPHeaderMap& map, const Key& key)
{
    return Vector<String>(fromRange, map.getAll(key));
}

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
    Vector<String> expected { emptyString(), "one, two"_str, "one, two"_str, ", "_str, emptyString() };
    auto combined = ", one, two, one, two, , , "_str;
    for (auto& name : Vector<String> { "Cache-Control"_str, "X-Test"_str }) {
        EXPECT_TRUE(map.getAll(name).empty());
        map.add(name, emptyString());
        EXPECT_EQ((Vector<String> { emptyString() }), headerValues(map, name));
        for (auto& value : expected.span().subspan(1))
            map.add(name.convertToASCIILowercase(), value);
        EXPECT_EQ(combined, map.get(name));
        EXPECT_EQ(expected, headerValues(map, name));
    }
    EXPECT_EQ(expected, headerValues(map, HTTPHeaderName::CacheControl));
    EXPECT_TRUE(map.getAll(HTTPHeaderName::Date).empty());
    EXPECT_EQ(2, map.size());
    for (auto& header : map) {
        EXPECT_EQ(combined, header.value);
        EXPECT_EQ(map.get(header.key).impl(), header.value.impl());
    }
}

TEST(HTTPHeaderMap, KeyAndValueViews)
{
    HTTPHeaderMap map;
    EXPECT_TRUE(map.commonHeaderKeys().empty());
    EXPECT_TRUE(map.uncommonHeaderKeys().empty());
    EXPECT_TRUE(map.getAll("Missing"_s).empty());

    map.add("Accept"_str, "text/plain"_str);
    map.add("accept"_str, "text/html"_str);
    map.add("X-Test"_str, "first, second"_str);
    map.add("x-test"_str, emptyString());
    EXPECT_EQ((Vector<HTTPHeaderName> { HTTPHeaderName::Accept }), WTF::copyToVector(map.commonHeaderKeys()));
    EXPECT_EQ((Vector<String> { "X-Test"_str }), WTF::copyToVector(map.uncommonHeaderKeys()));
    EXPECT_EQ(headerValues(map, HTTPHeaderName::Accept), headerValues(map, "ACCEPT"_s));
    auto values = map.getAll("X-TEST"_s);
    EXPECT_EQ(2U, values.size());
    EXPECT_EQ("first, second"_str, values[0]);
    EXPECT_EQ(emptyString(), values[1]);
}

TEST(HTTPHeaderMap, ReplaceAndRemoveRepeatedFields)
{
    for (auto& name : Vector<String> { "Cache-Control"_str, "X-Test"_str }) {
        HTTPHeaderMap map;
        map.add(name, "first"_str);
        map.add(name.convertToASCIILowercase(), "second"_str);
        map.add("X-Keep"_str, "one"_str);
        map.add("X-Keep"_str, "two"_str);

        map.set(name.convertToASCIILowercase(), emptyString());
        EXPECT_EQ((Vector<String> { emptyString() }), headerValues(map, name));
        map.add(name, "third"_str);
        EXPECT_EQ((Vector<String> { emptyString(), "third"_str }), headerValues(map, name));
        if (name == "X-Test"_s)
            EXPECT_TRUE(map.remove(*map.uncommonHeaderKeys().begin()));
        else
            EXPECT_TRUE(map.remove(HTTPHeaderName::CacheControl));
        EXPECT_TRUE(map.getAll(name).empty());
        EXPECT_EQ((Vector<String> { "one"_str, "two"_str }), headerValues(map, "X-Keep"_s));

        map.add(name, "fresh"_str);
        EXPECT_EQ((Vector<String> { "fresh"_str }), headerValues(map, name));
        map.add(name, "repeated"_str);
        map.clear();
        EXPECT_TRUE(map.getAll(name).empty());
        map.append(name, "new"_str);
        EXPECT_EQ((Vector<String> { "new"_str }), headerValues(map, name));
    }
}

TEST(HTTPHeaderMap, EffectiveComparisonWithRepeatedFields)
{
    for (auto& name : Vector<String> { "Cache-Control"_str, "X-Test"_str }) {
        HTTPHeaderMap repeated;
        repeated.add(name, "first"_str);
        repeated.add(name, "second"_str);
        HTTPHeaderMap combined;
        combined.add(name, "first, second"_str);
        EXPECT_EQ(repeated, combined);
        combined.set(name, "second, first"_str);
        EXPECT_NE(repeated, combined);
    }
}

TEST(HTTPHeaderMap, UnicodeBoundariesAndRetainedSlices)
{
    auto latin1 = String::fromUTF8("caf\xc3\xa9, ");
    auto unicode = String::fromUTF8("\xc4\x80 \xf0\x9f\x8d\x8e");
    Vector<String> expected { latin1, latin1, unicode, emptyString() };
    Vector<String> retained;
    {
        HTTPHeaderMap map;
        for (auto& name : Vector<String> { "ETag"_str, "X-Test"_str }) {
            for (auto& value : expected)
                map.add(name, value);
        }
        EXPECT_EQ(expected, headerValues(map, HTTPHeaderName::ETag));
        retained = headerValues(map, "X-Test"_s);
    }
    EXPECT_EQ(expected, retained);
}

TEST(HTTPHeaderMap, MetadataCopies)
{
    HTTPHeaderMap original;
    Vector<String> expected { "first"_str, "one, two"_str };
    for (auto& name : Vector<String> { "Cache-Control"_str, "X-Test"_str }) {
        for (auto& value : expected)
            original.add(name, value);
    }
    auto isolated = original.isolatedCopy();
    Vector<HTTPHeaderMap> copies { original, original.isolatedCopy(), WTF::move(isolated).isolatedCopy() };
    original.clear();
    for (auto& copy : copies) {
        EXPECT_EQ(expected, headerValues(copy, HTTPHeaderName::CacheControl));
        EXPECT_EQ(expected, headerValues(copy, "X-Test"_s));
        copy.add("X-Test"_str, "copy"_str);
        EXPECT_EQ(3U, copy.getAll("X-Test"_s).size());
    }
}

TEST(HTTPHeaderMap, SerializedBoundaryStorage)
{
    HTTPHeaderMap::CommonHeadersVector common {
        { HTTPHeaderName::CacheControl, "no-cache, max-age=0"_str },
    };
    String combined = ", one, two, one, two"_str;
    HTTPHeaderMap::UncommonHeadersVector uncommon { { "X-Test"_str, combined } };
    HTTPHeaderMap::RepeatedValueOffsetsMap offsetsForHeader;
    offsetsForHeader.add("Cache-Control"_str, HTTPHeaderMap::RepeatedValueOffsets { 10 });
    offsetsForHeader.add("X-Test"_str, HTTPHeaderMap::RepeatedValueOffsets { 2, 12 });

    auto decoded = HTTPHeaderMap::fromIPCData(WTF::move(common), WTF::move(uncommon), WTF::move(offsetsForHeader));
    ASSERT_TRUE(decoded);
    EXPECT_EQ((Vector<String> { "no-cache"_str, "max-age=0"_str }), headerValues(*decoded, HTTPHeaderName::CacheControl));
    EXPECT_EQ((Vector<String> { emptyString(), "one, two"_str, "one, two"_str }), headerValues(*decoded, "X-Test"_s));
    EXPECT_EQ(combined.impl(), decoded->get("X-Test"_s).impl());

    Vector<HTTPHeaderMap::RepeatedValueOffsets> invalidOffsets {
        { },
        { 0 },
        { 3 },
        { 4 },
        { 2, 1 },
        { 2, 3 },
    };
    for (auto& offsets : invalidOffsets) {
        HTTPHeaderMap::UncommonHeadersVector headers { { "X-Test"_str, ", ,"_str } };
        HTTPHeaderMap::RepeatedValueOffsetsMap metadata;
        metadata.add("X-Test"_str, offsets);
        EXPECT_FALSE(HTTPHeaderMap::fromIPCData({ }, WTF::move(headers), WTF::move(metadata)));
    }
    HTTPHeaderMap::RepeatedValueOffsetsMap missingHeader;
    missingHeader.add("X-Missing"_str, HTTPHeaderMap::RepeatedValueOffsets { 2 });
    EXPECT_FALSE(HTTPHeaderMap::fromIPCData({ }, { }, WTF::move(missingHeader)));
}

}
