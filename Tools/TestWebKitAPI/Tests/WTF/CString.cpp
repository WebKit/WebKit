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
#include <concepts>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/StringPrintStream.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringCommon.h>
#include <wtf/text/StringConcatenate.h>
#include <wtf/text/WTFString.h>

TEST(WTF, CStringNullStringConstructor)
{
    CString string;
    constexpr size_t zeroLength = 0;
    ASSERT_TRUE(string.isNull());
    EXPECT_TRUE(string.isEmpty());
    ASSERT_EQ(string.data(), static_cast<const char*>(0));
    ASSERT_EQ(string.length(), zeroLength);

    ASCIICString stringFromCharPointer { static_cast<const char*>(0) };
    ASSERT_TRUE(stringFromCharPointer.isNull());
    EXPECT_TRUE(stringFromCharPointer.isEmpty());
    ASSERT_EQ(stringFromCharPointer.data(), static_cast<const char*>(0));
    ASSERT_EQ(stringFromCharPointer.length(), zeroLength);

    ASCIICString stringFromCharAndLength { std::span { static_cast<const char*>(0), zeroLength } };
    ASSERT_TRUE(stringFromCharAndLength.isNull());
    EXPECT_TRUE(stringFromCharAndLength.isEmpty());
    ASSERT_EQ(stringFromCharAndLength.data(), static_cast<const char*>(0));
    ASSERT_EQ(stringFromCharAndLength.length(), zeroLength);
}

TEST(WTF, CStringEmptyEmptyConstructor)
{
    const char* emptyString = "";

    ASCIICString stringFromEmptySpanWithNonNullPointer { unsafeMakeSpan(emptyString, 0) };
    EXPECT_FALSE(stringFromEmptySpanWithNonNullPointer.isNull());
    EXPECT_TRUE(stringFromEmptySpanWithNonNullPointer.isEmpty());
    EXPECT_EQ(stringFromEmptySpanWithNonNullPointer.length(), 0UZ);

    ASCIICString string { emptyString };
    ASSERT_FALSE(string.isNull());
    EXPECT_TRUE(string.isEmpty());
    ASSERT_EQ(string.length(), static_cast<size_t>(0));
    ASSERT_EQ(string.data()[0], 0);

    ASCIICString stringWithLength { ""_span };
    ASSERT_FALSE(stringWithLength.isNull());
    EXPECT_TRUE(stringWithLength.isEmpty());
    ASSERT_EQ(stringWithLength.length(), static_cast<size_t>(0));
    ASSERT_EQ(stringWithLength.data()[0], 0);
}

TEST(WTF, CStringEmptyRegularConstructor)
{
    const char* referenceString = "WebKit";

    ASCIICString string { referenceString };
    ASSERT_FALSE(string.isNull());
    ASSERT_EQ(string.length(), strlen(referenceString));
    ASSERT_STREQ(referenceString, string.data());

    ASCIICString stringWithLength { std::span { referenceString, 6 } };
    ASSERT_FALSE(stringWithLength.isNull());
    ASSERT_EQ(stringWithLength.length(), strlen(referenceString));
    ASSERT_STREQ(referenceString, stringWithLength.data());
}

TEST(WTF, CStringOneByte)
{
    const char* referenceString = "W";

    ASCIICString string { referenceString };
    ASSERT_FALSE(string.isNull());
    ASSERT_FALSE(string.isEmpty());
    ASSERT_EQ(string.length(), strlen(referenceString));
    ASSERT_STREQ(referenceString, string.data());

    ASCIICString stringWithLength { std::span { referenceString, 1 } };
    ASSERT_FALSE(stringWithLength.isNull());
    ASSERT_FALSE(stringWithLength.isEmpty());
    ASSERT_EQ(stringWithLength.length(), strlen(referenceString));
    ASSERT_STREQ(referenceString, stringWithLength.data());
}

TEST(WTF, ASCIICStringUninitializedConstructor)
{
    std::span<char> buffer;
    ASCIICString emptyString = ASCIICString::newUninitialized(0, buffer);
    ASSERT_FALSE(emptyString.isNull());
    ASSERT_EQ(buffer.data(), emptyString.data());
    ASSERT_TRUE(buffer.empty());

    const size_t length = 25;
    ASCIICString uninitializedString = ASCIICString::newUninitialized(length, buffer);
    ASSERT_FALSE(uninitializedString.isNull());
    ASSERT_EQ(buffer.data(), uninitializedString.data());
    ASSERT_EQ(uninitializedString.data()[length], 0);
}

TEST(WTF, CStringZeroTerminated)
{
    const char* referenceString = "WebKit";
    ASCIICString stringWithLength { std::span { referenceString, 3 } };
    ASSERT_EQ(stringWithLength.data()[3], 0);
}

TEST(WTF, CStringLegacyCStringPointer)
{
    CString nullString;
    EXPECT_EQ(nullString.legacyCStringPointer(), static_cast<const char*>(nullptr));

    CString string = ASCIICString { "WebKit"_s };
    EXPECT_EQ(string.legacyCStringPointer(), string.data());
    EXPECT_STREQ(string.legacyCStringPointer(), "WebKit");
}

TEST(WTF, CStringCopyOnWrite)
{
    ASCIICString string { "Webkit"_s };
    ASCIICString copy = string;

    string.mutableSpan()[3] = 'K';
    ASSERT_TRUE(string != copy);
    ASSERT_STREQ(string.data(), "WebKit");
    ASSERT_STREQ(copy.data(), "Webkit");
}

TEST(WTF, CStringComparison)
{
    // Slicing a typed string is how an encoding-erased CString is spelled now. The comparison it
    // selects is the byte-wise one on CString, which CStringWithEncodingComparison does not cover.
    CString a;
    CString b;
    ASSERT_TRUE(a == b);
    ASSERT_FALSE(a != b);
    a = ASCIICString { "a"_s };
    b = CString();
    ASSERT_FALSE(a == b);
    ASSERT_TRUE(a != b);
    a = ASCIICString { "a"_s };
    b = ASCIICString { "b"_s };
    ASSERT_FALSE(a == b);
    ASSERT_TRUE(a != b);
    a = ASCIICString { "a"_s };
    b = ASCIICString { "a"_s };
    ASSERT_TRUE(a == b);
    ASSERT_FALSE(a != b);
    a = ASCIICString { "a"_s };
    b = ASCIICString { "aa"_s };
    ASSERT_FALSE(a == b);
    ASSERT_TRUE(a != b);
    a = ASCIICString { ""_s };
    b = ASCIICString { ""_s };
    ASSERT_TRUE(a == b);
    ASSERT_FALSE(a != b);
    a = ASCIICString { ""_s };
    b = CString();
    ASSERT_FALSE(a == b);
    ASSERT_TRUE(a != b);
    a = ASCIICString { "a"_s };
    b = ASCIICString { ""_s };
    ASSERT_FALSE(a == b);
    ASSERT_TRUE(a != b);

    // Comparison against an ASCII literal, which is valid in every encoding and so is the one raw
    // comparison CString still offers. It is a non-template overload, so an ASCIICString on the left
    // picks it too: the typed operator cannot deduce its parameter from a literal.
    ASCIICString c;
    ASCIILiteral d;
    ASSERT_TRUE(c == d);
    ASSERT_FALSE(c != d);
    c = "c"_s;
    d = nullptr;
    ASSERT_FALSE(c == d);
    ASSERT_TRUE(c != d);
    c = ASCIICString();
    d = "d"_s;
    ASSERT_FALSE(c == d);
    ASSERT_TRUE(c != d);
    c = "c"_s;
    d = "d"_s;
    ASSERT_FALSE(c == d);
    ASSERT_TRUE(c != d);
    c = "c"_s;
    d = "c"_s;
    ASSERT_TRUE(c == d);
    ASSERT_FALSE(c != d);
    c = "c"_s;
    d = "cc"_s;
    ASSERT_FALSE(c == d);
    ASSERT_TRUE(c != d);
    c = "cc"_s;
    d = "c"_s;
    ASSERT_FALSE(c == d);
    ASSERT_TRUE(c != d);
    c = ""_s;
    d = ""_s;
    ASSERT_TRUE(c == d);
    ASSERT_FALSE(c != d);
    c = ""_s;
    d = nullptr;
    ASSERT_FALSE(c == d);
    ASSERT_TRUE(c != d);
    c = ASCIICString();
    d = ""_s;
    ASSERT_FALSE(c == d);
    ASSERT_TRUE(c != d);
    c = "a"_s;
    d = ""_s;
    ASSERT_FALSE(c == d);
    ASSERT_TRUE(c != d);
    c = ""_s;
    d = "b"_s;
    ASSERT_FALSE(c == d);
    ASSERT_TRUE(c != d);
}

TEST(WTF, CStringStdStringInterop)
{
    // Null round-trip is lossy: null strings convert to empty std::strings that convert to empty strings.
    {
        ASCIICString a;
        EXPECT_TRUE(a.isNull());
        std::string stda;
        EXPECT_EQ(a.toStdString(), stda);
        ASCIICString b { stda };
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
            ASCIICString a { input.characters() };
            std::string stda { input.characters() };
            EXPECT_EQ(a.toStdString(), stda);
            ASCIICString b { stda };
            EXPECT_EQ(a, b);
        }
        // As ASCIILiteral / span.
        {
            ASCIICString a { input };
            auto inputSpan = input.span();
            std::string stda { inputSpan.begin(), inputSpan.end() };
            EXPECT_EQ(a.toStdString(), stda);
            ASCIICString b { stda };
            EXPECT_EQ(a, b);
        }
    }

    // Explict length strings, i.e. strings with nul chars inside, are exact.
    {
        auto inputSpan = unsafeMakeSpan("some\0thing", 10);
        ASCIICString a { inputSpan };
        EXPECT_EQ(a.length(), 10u);
        std::string stda { inputSpan.begin(), inputSpan.end() };
        EXPECT_EQ(stda.length(), 10u);
        EXPECT_EQ(a.toStdString(), stda);
    }
}

TEST(WTF, CStringViewASCIICaseConversions)
{
    EXPECT_EQ(WTF::convertToASCIILowercase(u8"Test"_span), UTF8CString { u8"test"_span });
    EXPECT_EQ(WTF::convertToASCIIUppercase(u8"Test"_span), UTF8CString { u8"TEST"_span });
    EXPECT_EQ(WTF::convertToASCIILowercase(u8"Water🍉Melon"_span), UTF8CString { u8"water🍉melon"_span });
    EXPECT_EQ(WTF::convertToASCIIUppercase(u8"Water🍉Melon"_span), UTF8CString { u8"WATER🍉MELON"_span });
    EXPECT_EQ(WTF::convertToASCIILowercase(std::span<const char8_t>()), UTF8CString { u8""_span });
    EXPECT_EQ(WTF::convertToASCIIUppercase(std::span<const char8_t>()), UTF8CString { u8""_span });
    EXPECT_EQ(WTF::convertToASCIILowercase(u8""_span), UTF8CString { u8""_span });
    EXPECT_EQ(WTF::convertToASCIIUppercase(u8""_span), UTF8CString { u8""_span });
}

// The encoding survives into the span's element type, which is what makes the rest of WTF do the right thing.
static_assert(std::same_as<decltype(std::declval<const UTF8CString&>().span())::element_type, const char8_t>);
static_assert(std::same_as<decltype(std::declval<const Latin1CString&>().span())::element_type, const Latin1Character>);
static_assert(std::same_as<decltype(std::declval<UTF8CString&>().mutableSpan())::element_type, char8_t>);
static_assert(std::same_as<decltype(std::declval<const UTF8CString&>().data()), const char8_t*>);
static_assert(std::same_as<decltype(std::declval<const Latin1CString&>().data()), const Latin1Character*>);
static_assert(std::same_as<decltype(std::declval<const UTF8CString&>().legacyCStringPointer()), const char*>);
// ASCII is spelled with char, as in ASCIILiteral, so its accessors match the untyped ones.
static_assert(std::same_as<decltype(std::declval<const ASCIICString&>().data()), const char*>);
static_assert(std::same_as<decltype(std::declval<const ASCIICString&>().span())::element_type, const char>);
// Erasing the encoding gives back the untyped CString span.
static_assert(std::same_as<decltype(std::declval<const CString&>().span())::element_type, const char>);
static_assert(std::same_as<decltype(std::declval<const CString&>().legacyCStringPointer()), const char*>);
// Only UTF-8 needs the escape hatch, so the constrained override has to keep hiding
// CString::legacyCStringPointer() for the other encodings: Latin-1 bytes are not a C string, and
// ASCIICString::data() is already a const char*.
template<typename StringType> concept HasLegacyCStringPointer = requires(const StringType& string)
{
    string.legacyCStringPointer();
};
static_assert(HasLegacyCStringPointer<CString>);
static_assert(HasLegacyCStringPointer<UTF8CString>);
static_assert(!HasLegacyCStringPointer<ASCIICString>);
static_assert(!HasLegacyCStringPointer<Latin1CString>);
// printf-style formatting reads the bytes back as UTF-8 or ASCII, so Latin-1 is kept away from it for
// the same reason. The encoding-erased CString stays accepted while its producers are migrated.
template<typename StringType> concept HasSafePrintfType = requires(const StringType& string)
{
    safePrintfType(string);
};
static_assert(HasSafePrintfType<CString>);
static_assert(HasSafePrintfType<UTF8CString>);
static_assert(HasSafePrintfType<ASCIICString>);
static_assert(!HasSafePrintfType<Latin1CString>);
// Slicing to CString is allowed, but nothing implicitly converts the other way or between encodings.
static_assert(std::is_convertible_v<UTF8CString, CString>);
static_assert(!std::is_convertible_v<CString, UTF8CString>);
static_assert(!std::is_convertible_v<Latin1CString, UTF8CString>);
// Bytes get into a CString only through CStringWithEncoding: the encoding-erased base cannot be
// built from a literal, a span or a std::string, and cannot hand out a buffer to write into.
// ASCIILiteral converts to const char*, but that is a worse match than CString(ASCIILiteral), so
// a literal is rejected outright rather than quietly losing its length, and any embedded null, to
// a strlen. A std::string reaches the span constructor only through a user-defined conversion, so
// with that constructor gone it has no way in at all.
static_assert(!std::constructible_from<CString, ASCIILiteral>);
static_assert(std::constructible_from<UTF8CString, ASCIILiteral>);
static_assert(!std::constructible_from<CString, std::span<const char>>);
static_assert(std::constructible_from<ASCIICString, std::span<const char>>);
static_assert(!std::constructible_from<CString, std::string>);
static_assert(std::constructible_from<UTF8CString, std::string>);
#if PLATFORM(COCOA)
static_assert(!std::constructible_from<CString, const char*>);
#endif
static_assert(std::constructible_from<ASCIICString, const char*>);
template<typename StringType> concept HasMutableSpan = requires(StringType& string)
{
    string.mutableSpan();
    string.mutableSpanIncludingNullTerminator();
    string.grow(1);
};
static_assert(!HasMutableSpan<CString>);
static_assert(HasMutableSpan<UTF8CString>);
// The storage cannot be taken out of a CString either, which is what closes the last way to build
// one from bytes that never named an encoding.
template<typename StringType> concept HasBuffer = requires(const StringType& string)
{
    string.buffer();
};
static_assert(!HasBuffer<CString>);
static_assert(!HasBuffer<UTF8CString>);
// Ordering across encodings must not compile. This has to go through a concept: with concrete
// types, selecting a deleted overload is a hard error rather than an unsatisfied requirement.
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
// Latin-1 and UTF-8 are compared by code point rather than by byte.
static_assert(IsEqualityComparable<UTF8CString, Latin1CString>);
static_assert(IsEqualityComparable<Latin1CString, UTF8CString>);
static_assert(!IsLessThanComparable<UTF8CString, Latin1CString>);
// ASCII is Latin-1 restricted to 0..127: byte comparison against Latin-1, code point comparison against UTF-8.
static_assert(IsEqualityComparable<ASCIICString, UTF8CString>);
static_assert(IsEqualityComparable<ASCIICString, Latin1CString>);
static_assert(!IsLessThanComparable<ASCIICString, UTF8CString>);
static_assert(IsLessThanComparable<ASCIICString, Latin1CString>);
// The conversions that have been migrated report their encoding in the type.
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
    EXPECT_EQ(nullString.legacyCStringPointer(), static_cast<const char*>(nullptr));
    EXPECT_EQ(nullString.length(), 0UZ);

    UTF8CString fromSpan { u8"Water🍉Melon"_span };
    EXPECT_FALSE(fromSpan.isNull());
    EXPECT_EQ(fromSpan.length(), 14UZ);
    EXPECT_TRUE(equalSpans(fromSpan.span(), u8"Water🍉Melon"_span));
    EXPECT_STREQ(fromSpan.legacyCStringPointer(), "Water🍉Melon");

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

TEST(WTF, CStringWithEncodingCrossEncodingComparison)
{
    constexpr auto latin1Cafe = WTF::toArray<Latin1Character>({ 'c', 'a', 'f', 0xE9 });
    Latin1CString latin1 { std::span<const Latin1Character> { latin1Cafe } };
    UTF8CString utf8 { u8"café"_span };

    // Same text, different bytes: 4 Latin-1 bytes against 5 UTF-8 bytes.
    EXPECT_EQ(latin1.length(), 4UZ);
    EXPECT_EQ(utf8.length(), 5UZ);
    EXPECT_EQ(latin1, utf8);
    EXPECT_EQ(utf8, latin1);

    EXPECT_NE(latin1, UTF8CString { u8"cafe"_span });
    EXPECT_NE(utf8, Latin1CString { "cafe"_s });

    // Null and empty stay distinct, as they do within a single encoding.
    EXPECT_NE(Latin1CString { }, UTF8CString { u8""_span });
    EXPECT_EQ(Latin1CString { }, UTF8CString { });

    // ASCII is a subset of both, so it matches its own bytes in either.
    ASCIICString ascii { String("cafe"_s).ascii() };
    EXPECT_EQ(ascii, Latin1CString { "cafe"_s });
    EXPECT_EQ(ascii, UTF8CString { u8"cafe"_span });
    EXPECT_NE(ascii, latin1);
    EXPECT_NE(ascii, utf8);
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
    EXPECT_EQ(UTF8CString { u8"key"_span }.hash(), CString { ASCIICString { "key"_s } }.hash());
}

template<typename StringType> concept AdaptableToString = std::constructible_from<WTF::StringTypeAdapter<StringType>, const StringType&>;

TEST(WTF, CStringWithEncodingMakeString)
{
    // makeString picks its adapter off the span's element type, so a UTF8CString is decoded as UTF-8.
    // An untyped CString has no encoding to decode from, so it has no adapter at all and erasing the
    // encoding does not compile, rather than silently reinterpreting the bytes as Latin-1.
    static_assert(AdaptableToString<UTF8CString>);
    static_assert(AdaptableToString<Latin1CString>);
    static_assert(AdaptableToString<ASCIICString>);
    static_assert(!AdaptableToString<CString>);

    UTF8CString utf8String { u8"Water🍉Melon"_span };
    EXPECT_EQ(makeString(utf8String), String::fromUTF8(u8"Water🍉Melon"_span));
    EXPECT_EQ(makeString(utf8String).length(), 12U);

    constexpr auto latin1Cafe = WTF::toArray<Latin1Character>({ 'c', 'a', 'f', 0xE9 });
    Latin1CString latin1String { std::span<const Latin1Character> { latin1Cafe } };
    EXPECT_EQ(makeString(latin1String), String::fromUTF8(u8"café"_span));
    EXPECT_EQ(makeString(latin1String).length(), 4U);
}

template<typename StringType> concept PrintableToStream = requires(StringPrintStream& out, const StringType& string) {
    WTF::printInternal(out, string);
};

TEST(WTF, CStringWithEncodingPrintStream)
{
    // A PrintStream holds UTF-8, so printing transcodes whatever it is given. An untyped CString
    // has no encoding to transcode from, so printing one does not compile.
    static_assert(PrintableToStream<UTF8CString>);
    static_assert(PrintableToStream<Latin1CString>);
    static_assert(PrintableToStream<ASCIICString>);
    static_assert(!PrintableToStream<CString>);

    auto print = [](const auto& string) {
        StringPrintStream out;
        out.print(string);
        return out.toString();
    };

    UTF8CString utf8String { u8"Water🍉Melon"_span };
    EXPECT_EQ(print(utf8String), String::fromUTF8(u8"Water🍉Melon"_span));

    constexpr auto latin1Cafe = WTF::toArray<Latin1Character>({ 'c', 'a', 'f', 0xE9 });
    Latin1CString latin1String { std::span<const Latin1Character> { latin1Cafe } };
    EXPECT_EQ(print(latin1String), String::fromUTF8(u8"café"_span));

    ASCIICString asciiString { "cafe"_s };
    EXPECT_EQ(print(asciiString), "cafe"_s);
}

TEST(WTF, CStringWithEncodingFromPrintStream)
{
    // A PrintStream can be read back as either encoding. toUTF8CString() reports the bytes it holds,
    // while toASCIICString() is for streams that only ever print ASCII, where const char* is wanted.
    EXPECT_EQ(toUTF8CString("P", 1), UTF8CString { u8"P1"_span });
    EXPECT_EQ(toASCIICString("P", 1), ASCIICString { "P1"_s });

    // ASCII is a subset of UTF-8, so the two agree byte for byte and compare equal across encodings.
    EXPECT_EQ(toASCIICString("cafe"), toUTF8CString("cafe"));

    // ASCIICString::data() is already a const char*, which is the point of the encoding: no escape
    // hatch is needed to hand it to a C string interface, unlike UTF8CString::legacyCStringPointer().
    ASCIICString asciiString = toASCIICString("a", 1, "b", 2);
    static_assert(std::same_as<decltype(asciiString.data()), const char*>);
    EXPECT_EQ(asciiString, ASCIICString { "a1b2"_s });
    EXPECT_EQ(asciiString.length(), 4U);

    // An empty stream reads back as empty rather than null, matching toUTF8CString().
    StringPrintStream empty;
    EXPECT_TRUE(empty.toASCIICString().isEmpty());
    EXPECT_FALSE(empty.toASCIICString().isNull());
    EXPECT_EQ(empty.toASCIICString(), ASCIICString { ""_s });
}
