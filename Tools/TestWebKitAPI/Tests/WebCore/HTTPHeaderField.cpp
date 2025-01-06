/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "Test.h"
#include <WebCore/HTTPHeaderField.h>
#include <wtf/text/MakeString.h>

static String canonicalizeHTTPHeader(const String& string)
{
    size_t colonLocation = string.find(':');
    if (colonLocation == notFound)
        return { };
    auto field = WebCore::HTTPHeaderField::create(string.left(colonLocation), string.substring(colonLocation + 1));
    if (!field)
        return { };
    return makeString(field->name(), ": "_s, field->value());
}

static void shouldRemainUnchanged(Vector<String>&& strings)
{
    for (const auto& string : strings)
        EXPECT_TRUE(canonicalizeHTTPHeader(string) == string);
}

static void shouldBeInvalid(Vector<String>&& strings)
{
    for (const auto& string : strings)
        EXPECT_TRUE(canonicalizeHTTPHeader(string) == String());
}

static void shouldBecome(Vector<std::pair<String, String>>&& pairs)
{
    for (const auto& pair : pairs)
        EXPECT_TRUE(canonicalizeHTTPHeader(pair.first) == pair.second);
}

TEST(HTTPHeaderField, Parser)
{
    static const char nonASCIIComment[8] = {'a', ':', ' ', '(', 0x21, static_cast<char>(0xFF), ')', '\0'};
    static const char nonASCIIQuotedPairInComment[8] = {'a', ':', ' ', '(', '\\', static_cast<char>(0xFF), ')', '\0'};
    static const char nonASCIIQuotedPairInQuote[8] = {'a', ':', ' ', '"', '\\', static_cast<char>(0xFF), '"', '\0'};
    static const char delInQuote[7] = {'a', ':', ' ', '"', static_cast<char>(0x7F), '"', '\0'};

    shouldRemainUnchanged({
        "a: b"_s,
        "a: b c"_s,
        "!: b"_s,
        "a: ()"_s,
        "a: (\\ )"_s,
        "a: (())"_s,
        "a: \"\""_s,
        "a: \"aA?\t \""_s,
        "a: \"a\" (b) ((c))"_s,
        String::fromLatin1(nonASCIIComment),
        String::fromLatin1(nonASCIIQuotedPairInComment),
        String::fromLatin1(nonASCIIQuotedPairInQuote),
    });
    
    shouldBeInvalid({
        "a:"_s,
        "a: "_s,
        "a: \rb"_s,
        "a: \nb"_s,
        "a: \vb"_s,
        "\a: b"_s,
        "?: b"_s,
        "a: \ab"_s,
        "a: (\\\a)"_s,
        "a: (\\\a)"_s,
        "a: ("_s,
        "a: (()"_s,
        "a: (\a)"_s,
        "a: \"\a\""_s,
        "a: \"a\" (b)}((c))"_s,
        String::fromLatin1(delInQuote),
    });
    
    shouldBecome({
        { "a: b "_s, "a: b"_s },
        { " a: b"_s, "a: b"_s },
        { "a: \tb"_s, "a: b"_s },
        { " a: b c "_s, "a: b c"_s },
        { "a:  b "_s, "a: b"_s },
        { "a:\tb"_s, "a: b"_s },
        { "\ta:\t b\t"_s, "a: b"_s },
        { "a: \"a\" (b) "_s, "a: \"a\" (b)"_s },
    });
}
