/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include <array>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringCommon.h>
#include <wtf/text/WTFString.h>

TEST(WTF, CStringNullStringConstructor)
{
    CString string;
    constexpr size_t zeroLength = 0;
    ASSERT_TRUE(string.isNull());
    EXPECT_TRUE(string.isEmpty());
    ASSERT_EQ(string.data(), static_cast<const char*>(0));
    ASSERT_EQ(string.length(), zeroLength);

    CString stringFromCharPointer(static_cast<const char*>(0));
    ASSERT_TRUE(stringFromCharPointer.isNull());
    EXPECT_TRUE(stringFromCharPointer.isEmpty());
    ASSERT_EQ(stringFromCharPointer.data(), static_cast<const char*>(0));
    ASSERT_EQ(stringFromCharPointer.length(), zeroLength);

    CString stringFromCharAndLength(std::span { static_cast<const char*>(0), zeroLength });
    ASSERT_TRUE(stringFromCharAndLength.isNull());
    EXPECT_TRUE(stringFromCharAndLength.isEmpty());
    ASSERT_EQ(stringFromCharAndLength.data(), static_cast<const char*>(0));
    ASSERT_EQ(stringFromCharAndLength.length(), zeroLength);
}

TEST(WTF, CStringEmptyEmptyConstructor)
{
    const char* emptyString = "";

    CString stringFromEmptySpanWithNonNullPointer(unsafeMakeSpan(emptyString, 0));
    EXPECT_FALSE(stringFromEmptySpanWithNonNullPointer.isNull());
    EXPECT_TRUE(stringFromEmptySpanWithNonNullPointer.isEmpty());
    EXPECT_EQ(stringFromEmptySpanWithNonNullPointer.length(), 0UZ);

    CString string(emptyString);
    ASSERT_FALSE(string.isNull());
    EXPECT_TRUE(string.isEmpty());
    ASSERT_EQ(string.length(), static_cast<size_t>(0));
    ASSERT_EQ(string.data()[0], 0);

    CString stringWithLength(""_span);
    ASSERT_FALSE(stringWithLength.isNull());
    EXPECT_TRUE(stringWithLength.isEmpty());
    ASSERT_EQ(stringWithLength.length(), static_cast<size_t>(0));
    ASSERT_EQ(stringWithLength.data()[0], 0);
}

TEST(WTF, CStringEmptyRegularConstructor)
{
    const char* referenceString = "WebKit";

    CString string(referenceString);
    ASSERT_FALSE(string.isNull());
    ASSERT_EQ(string.length(), strlen(referenceString));
    ASSERT_STREQ(referenceString, string.data());

    CString stringWithLength(std::span { referenceString, 6 });
    ASSERT_FALSE(stringWithLength.isNull());
    ASSERT_EQ(stringWithLength.length(), strlen(referenceString));
    ASSERT_STREQ(referenceString, stringWithLength.data());
}

TEST(WTF, CStringOneByte)
{
    const char* referenceString = "W";

    CString string(referenceString);
    ASSERT_FALSE(string.isNull());
    ASSERT_FALSE(string.isEmpty());
    ASSERT_EQ(string.length(), strlen(referenceString));
    ASSERT_STREQ(referenceString, string.data());

    CString stringWithLength(std::span { referenceString, 1 });
    ASSERT_FALSE(stringWithLength.isNull());
    ASSERT_FALSE(stringWithLength.isEmpty());
    ASSERT_EQ(stringWithLength.length(), strlen(referenceString));
    ASSERT_STREQ(referenceString, stringWithLength.data());
}

TEST(WTF, CStringUninitializedConstructor)
{
    std::span<char> buffer;
    CString emptyString = CString::newUninitialized(0, buffer);
    ASSERT_FALSE(emptyString.isNull());
    ASSERT_EQ(buffer.data(), emptyString.data());
    ASSERT_TRUE(buffer.empty());

    const size_t length = 25;
    CString uninitializedString = CString::newUninitialized(length, buffer);
    ASSERT_FALSE(uninitializedString.isNull());
    ASSERT_EQ(buffer.data(), uninitializedString.data());
    ASSERT_EQ(uninitializedString.data()[length], 0);
}

TEST(WTF, CStringZeroTerminated)
{
    const char* referenceString = "WebKit";
    CString stringWithLength(std::span { referenceString, 3 });
    ASSERT_EQ(stringWithLength.data()[3], 0);
}

TEST(WTF, CStringCopyOnWrite)
{
    const char* initialString = "Webkit";
    CString string(initialString);
    CString copy = string;

    string.mutableSpan()[3] = 'K';
    ASSERT_TRUE(string != copy);
    ASSERT_STREQ(string.data(), "WebKit");
    ASSERT_STREQ(copy.data(), initialString);
}

TEST(WTF, CStringComparison)
{
    // Comparison with another CString.
    CString a;
    CString b;
    ASSERT_TRUE(a == b);
    ASSERT_FALSE(a != b);
    a = "a";
    b = CString();
    ASSERT_FALSE(a == b);
    ASSERT_TRUE(a != b);
    a = "a";
    b = "b";
    ASSERT_FALSE(a == b);
    ASSERT_TRUE(a != b);
    a = "a";
    b = "a";
    ASSERT_TRUE(a == b);
    ASSERT_FALSE(a != b);
    a = "a";
    b = "aa";
    ASSERT_FALSE(a == b);
    ASSERT_TRUE(a != b);
    a = "";
    b = "";
    ASSERT_TRUE(a == b);
    ASSERT_FALSE(a != b);
    a = "";
    b = CString();
    ASSERT_FALSE(a == b);
    ASSERT_TRUE(a != b);
    a = "a";
    b = "";
    ASSERT_FALSE(a == b);
    ASSERT_TRUE(a != b);

    // Comparison with a const char*.
    CString c;
    const char* d = 0;
    ASSERT_TRUE(c == d);
    ASSERT_FALSE(c != d);
    c = "c";
    d = 0;
    ASSERT_FALSE(c == d);
    ASSERT_TRUE(c != d);
    c = CString();
    d = "d";
    ASSERT_FALSE(c == d);
    ASSERT_TRUE(c != d);
    c = "c";
    d = "d";
    ASSERT_FALSE(c == d);
    ASSERT_TRUE(c != d);
    c = "c";
    d = "c";
    ASSERT_TRUE(c == d);
    ASSERT_FALSE(c != d);
    c = "c";
    d = "cc";
    ASSERT_FALSE(c == d);
    ASSERT_TRUE(c != d);
    c = "cc";
    d = "c";
    ASSERT_FALSE(c == d);
    ASSERT_TRUE(c != d);
    c = "";
    d = "";
    ASSERT_TRUE(c == d);
    ASSERT_FALSE(c != d);
    c = "";
    d = 0;
    ASSERT_FALSE(c == d);
    ASSERT_TRUE(c != d);
    c = CString();
    d = "";
    ASSERT_FALSE(c == d);
    ASSERT_TRUE(c != d);
    c = "a";
    d = "";
    ASSERT_FALSE(c == d);
    ASSERT_TRUE(c != d);
    c = "";
    d = "b";
    ASSERT_FALSE(c == d);
    ASSERT_TRUE(c != d);
}

TEST(WTF, CStringStdStringInterop)
{
    // Null CString round-trip is lossy: null CStrings convert to empty std::strings that convert to empty CStrings.
    {
        CString a;
        EXPECT_TRUE(a.isNull());
        std::string stda;
        EXPECT_EQ(a.toStdString(), stda);
        CString b = stda;
        EXPECT_NE(a, b);
        EXPECT_EQ(b.length(), 0u);
        EXPECT_FALSE(b.isNull());
    }

    // Non-null string round-trip is exact.
    constexpr ASCIILiteral inputs[] = {
        ""_s,
        "some thing"_s,
        "some\0thing"_s
    };
    for (auto& input : inputs) {
        SCOPED_TRACE(::testing::Message() << "input: " << (input.characters() ? input.characters() : "nullptr"));
        // As const char*.
        {
            CString a { input.characters() };
            std::string stda { input.characters() };
            EXPECT_EQ(a.toStdString(), stda);
            CString b = stda;
            EXPECT_EQ(a, b);
        }
        // As ASCIILiteral / span.
        {
            CString a { input };
            auto inputSpan = input.span();
            std::string stda { inputSpan.begin(), inputSpan.end() };
            EXPECT_EQ(a.toStdString(), stda);
            CString b = stda;
            EXPECT_EQ(a, b);
        }
    }

    // Explict length strings, i.e. strings with nul chars inside, are exact.
    {
        auto inputSpan = unsafeMakeSpan("some\0thing", 10);
        CString a { inputSpan };
        EXPECT_EQ(a.length(), 10u);
        std::string stda { inputSpan.begin(), inputSpan.end() };
        EXPECT_EQ(stda.length(), 10u);
        EXPECT_EQ(a.toStdString(), stda);
    }
}

TEST(WTF, CStringViewASCIICaseConversions)
{
    EXPECT_EQ(WTF::convertToASCIILowercase(u8"Test"_span), CString("test"));
    EXPECT_EQ(WTF::convertToASCIIUppercase(u8"Test"_span), CString("TEST"));
    EXPECT_EQ(WTF::convertToASCIILowercase(u8"Water🍉Melon"_span), CString("water🍉melon"));
    EXPECT_EQ(WTF::convertToASCIIUppercase(u8"Water🍉Melon"_span), CString("WATER🍉MELON"));
    EXPECT_EQ(WTF::convertToASCIILowercase(std::span<const char8_t>()), CString(""_s));
    EXPECT_EQ(WTF::convertToASCIIUppercase(std::span<const char8_t>()), CString(""_s));
    EXPECT_EQ(WTF::convertToASCIILowercase(u8""_span), CString(""_s));
    EXPECT_EQ(WTF::convertToASCIIUppercase(u8""_span), CString(""_s));
}

// The encoding survives into the span's element type, which is what makes the rest of WTF do the right thing.
static_assert(std::same_as<decltype(std::declval<const UTF8CString&>().span())::element_type, const char8_t>);
static_assert(std::same_as<decltype(std::declval<const Latin1CString&>().span())::element_type, const Latin1Character>);
static_assert(std::same_as<decltype(std::declval<UTF8CString&>().mutableSpan())::element_type, char8_t>);
static_assert(std::same_as<decltype(std::declval<const UTF8CString&>().data()), const char8_t*>);
static_assert(std::same_as<decltype(std::declval<const Latin1CString&>().data()), const Latin1Character*>);
static_assert(std::same_as<decltype(std::declval<const UTF8CString&>().characters()), const char*>);
// ASCII is spelled with char, as in ASCIILiteral, so its accessors match the untyped ones.
static_assert(std::same_as<decltype(std::declval<const ASCIICString&>().data()), const char*>);
static_assert(std::same_as<decltype(std::declval<const ASCIICString&>().span())::element_type, const char>);
// Erasing the encoding gives back the untyped CString span.
static_assert(std::same_as<decltype(std::declval<const CString&>().span())::element_type, const char>);
// Slicing to CString is allowed, but nothing implicitly converts the other way or between encodings.
static_assert(std::is_convertible_v<UTF8CString, CString>);
static_assert(!std::is_convertible_v<CString, UTF8CString>);
static_assert(!std::is_convertible_v<Latin1CString, UTF8CString>);
// Comparing bytes across encodings is meaningless and must not compile. These have to go through a concept:
// with concrete types, selecting a deleted overload is a hard error rather than an unsatisfied requirement.
template<typename A, typename B> concept IsEqualityComparable = requires(const A& a, const B& b)
{
    a == b;
};
template<typename A, typename B> concept IsLessThanComparable = requires(const A& a, const B& b)
{
    a < b;
};
static_assert(IsEqualityComparable<UTF8CString, UTF8CString>);
static_assert(IsEqualityComparable<Latin1CString, Latin1CString>);
static_assert(!IsEqualityComparable<UTF8CString, Latin1CString>);
static_assert(!IsEqualityComparable<Latin1CString, UTF8CString>);
static_assert(!IsLessThanComparable<UTF8CString, Latin1CString>);
// ASCII is a subset of both, so comparing it against either is well-defined and stays available.
static_assert(IsEqualityComparable<ASCIICString, UTF8CString>);
static_assert(IsEqualityComparable<ASCIICString, Latin1CString>);
// The conversions that have been migrated report their encoding in the type.
static_assert(std::same_as<decltype(std::declval<const String&>().ascii()), ASCIICString>);
static_assert(std::same_as<decltype(std::declval<const String&>().latin1()), Latin1CString>);
static_assert(std::same_as<decltype(std::declval<const String&>().tryGetUTF8()), std::expected<UTF8CString, UTF8ConversionError>>);
static_assert(std::same_as<decltype(std::declval<const StringView&>().tryGetUTF8()), std::expected<UTF8CString, UTF8ConversionError>>);
static_assert(std::same_as<decltype(WTF::convertToASCIILowercase(u8""_span)), UTF8CString>);
// The String conversions that have been migrated report their encoding in the type.
static_assert(std::same_as<decltype(std::declval<const String&>().ascii()), ASCIICString>);
static_assert(std::same_as<decltype(std::declval<const String&>().latin1()), Latin1CString>);
static_assert(std::same_as<decltype(std::declval<const String&>().tryGetUTF8()), std::expected<UTF8CString, UTF8ConversionError>>);
static_assert(std::same_as<decltype(std::declval<const StringView&>().tryGetUTF8()), std::expected<UTF8CString, UTF8ConversionError>>);
static_assert(std::same_as<decltype(WTF::convertToASCIILowercase(u8""_span)), UTF8CString>);
// Comparing against a plain CString stays available: it means "unknown encoding", so it is the deliberate escape hatch.
static_assert(IsEqualityComparable<UTF8CString, CString>);
// An ASCII literal is valid in every encoding, so this stays available too.
static_assert(IsEqualityComparable<UTF8CString, ASCIILiteral>);

TEST(WTF, CStringWithEncodingConstruction)
{
    UTF8CString nullString;
    EXPECT_TRUE(nullString.isNull());
    EXPECT_TRUE(nullString.isEmpty());
    EXPECT_EQ(nullString.data(), static_cast<const char8_t*>(nullptr));
    EXPECT_EQ(nullString.characters(), static_cast<const char*>(nullptr));
    EXPECT_EQ(nullString.length(), 0UZ);

    UTF8CString fromSpan { u8"Water🍉Melon"_span };
    EXPECT_FALSE(fromSpan.isNull());
    EXPECT_EQ(fromSpan.length(), 14UZ);
    EXPECT_TRUE(equalSpans(fromSpan.span(), u8"Water🍉Melon"_span));
    EXPECT_STREQ(fromSpan.characters(), "Water🍉Melon");

    UTF8CString fromLiteral { "test"_s };
    EXPECT_EQ(fromLiteral, UTF8CString { u8"test"_span });
    EXPECT_EQ(fromLiteral, "test"_s);

    constexpr auto latin1Cafe = WTF::toArray<Latin1Character>({ 'c', 'a', 'f', 0xE9 });
    Latin1CString latin1String { std::span<const Latin1Character> { latin1Cafe } };
    EXPECT_EQ(latin1String.length(), 4UZ);
    EXPECT_TRUE(equalSpans(latin1String.span(), std::span<const Latin1Character> { latin1Cafe }));
}

TEST(WTF, CStringWithEncodingNewUninitialized)
{
    std::span<char8_t> characters;
    auto string = UTF8CString::newUninitialized(4, characters);
    EXPECT_EQ(characters.size(), 4UZ);
    memcpySpan(characters, u8"test"_span);
    EXPECT_EQ(string, UTF8CString { u8"test"_span });
    EXPECT_EQ(string.spanIncludingNullTerminator()[4], u8'\0');
}

TEST(WTF, CStringWithEncodingComparison)
{
    UTF8CString a { u8"abc"_span };
    UTF8CString b { u8"abd"_span };
    UTF8CString nullString;

    EXPECT_EQ(a, UTF8CString { u8"abc"_span });
    EXPECT_NE(a, b);
    EXPECT_NE(a, nullString);
    EXPECT_EQ(nullString, UTF8CString { });
    EXPECT_TRUE(a < b);
    EXPECT_FALSE(b < a);
    EXPECT_TRUE(nullString < a);
}

TEST(WTF, CStringWithEncodingHashing)
{
    HashSet<UTF8CString> set;
    EXPECT_TRUE(set.add(UTF8CString { u8"Water🍉Melon"_span }).isNewEntry);
    EXPECT_FALSE(set.add(UTF8CString { u8"Water🍉Melon"_span }).isNewEntry);
    EXPECT_TRUE(set.add(UTF8CString { u8"other"_span }).isNewEntry);
    EXPECT_EQ(set.size(), 2U);
    EXPECT_TRUE(set.contains(UTF8CString { u8"other"_span }));
    EXPECT_FALSE(set.contains(UTF8CString { u8"missing"_span }));

    HashMap<UTF8CString, int> map;
    map.add(UTF8CString { u8"key"_span }, 1);
    EXPECT_EQ(map.get(UTF8CString { u8"key"_span }), 1);

    // Hashing is over the raw bytes, so an equal untyped CString agrees.
    EXPECT_EQ(UTF8CString { u8"key"_span }.hash(), CString("key").hash());
}

TEST(WTF, CStringWithEncodingMakeString)
{
    // makeString picks its adapter off the span's element type, so a UTF8CString is decoded as UTF-8.
    UTF8CString utf8String { u8"Water🍉Melon"_span };
    EXPECT_EQ(makeString(utf8String), String::fromUTF8(u8"Water🍉Melon"_span));
    EXPECT_EQ(makeString(utf8String).length(), 12U);

    // Erasing the encoding falls back to reinterpreting the bytes as Latin-1.
    EXPECT_EQ(makeString(static_cast<const CString&>(utf8String)).length(), 14U);

    constexpr auto latin1Cafe = WTF::toArray<Latin1Character>({ 'c', 'a', 'f', 0xE9 });
    Latin1CString latin1String { std::span<const Latin1Character> { latin1Cafe } };
    EXPECT_EQ(makeString(latin1String), String::fromUTF8(u8"café"_span));
    EXPECT_EQ(makeString(latin1String).length(), 4U);
}
