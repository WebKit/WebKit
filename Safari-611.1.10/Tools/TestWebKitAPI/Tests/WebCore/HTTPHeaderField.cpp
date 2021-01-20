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

static String canonicalizeHTTPHeader(const String& string)
{
    size_t colonLocation = string.find(':');
    if (colonLocation == notFound)
        return { };
    auto field = WebCore::HTTPHeaderField::create(string.substring(0, colonLocation), string.substring(colonLocation + 1));
    if (!field)
        return { };
    return makeString(field->name(), ": ", field->value());
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
        "a: b",
        "a: b c",
        "!: b",
        "a: ()",
        "a: (\\ )",
        "a: (())",
        "a: \"\"",
        "a: \"aA?\t \"",
        "a: \"a\" (b) ((c))",
        nonASCIIComment,
        nonASCIIQuotedPairInComment,
        nonASCIIQuotedPairInQuote,
    });
    
    shouldBeInvalid({
        "a:",
        "a: ",
        "a: \rb",
        "a: \nb",
        "a: \vb",
        "\a: b",
        "?: b",
        "a: \ab",
        "a: (\\\a)",
        "a: (\\\a)",
        "a: (",
        "a: (()",
        "a: (\a)",
        "a: \"\a\"",
        "a: \"a\" (b)}((c))",
        delInQuote,
    });
    
    shouldBecome({
        { "a: b ", "a: b" },
        { " a: b", "a: b" },
        { "a: \tb", "a: b" },
        { " a: b c ", "a: b c" },
        { "a:  b ", "a: b" },
        { "a:\tb", "a: b" },
        { "\ta:\t b\t", "a: b" },
        { "a: \"a\" (b) ", "a: \"a\" (b)" },
    });
}
