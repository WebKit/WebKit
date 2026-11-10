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

// Expected values for StringFunctions.cpp, one entry per sample string (see sampleStrings()).
// These were generated from the String and AtomString classes before their removal, so that the free functions
// keep their behavior. N is a null string; nf is notFound.

#pragma once

#include <limits>
#include <optional>
#include <string_view>
#include <vector>

namespace TestWebKitAPI::StringFunctionsExpectations {

using std::literals::string_view_literals::operator""sv;

using ExpectedString = std::optional<std::u16string_view>;
inline constexpr std::nullopt_t N = std::nullopt;
inline constexpr size_t nf = static_cast<size_t>(-1);

inline const std::vector<ExpectedString> sampleContents {
    N,
    u""sv,
    u"Hello, World"sv,
    u"Caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv,
    u"\x0421\x0442\x0440\x043E\x043A\x0430 Hello \x00E9 World"sv,
    u"Atomic Hello World"sv,
    u"\x0130stanbul ISTANBUL istanbul"sv,
    u"  Leading and trailing \x0009 whitespace \x000A "sv,
    u"a,b,,c,"sv,
    u"1.5"sv,
    u"  -42.25e1junk"sv,
    u"\xD83D\xDE00 emoji \xD83D\xDE00"sv,
    u"lowercase only"sv,
    u"UPPERCASE ONLY"sv,
};

struct Properties {
    bool isEmpty;
    unsigned length;
    bool is8Bit;
    unsigned sizeInBytes;
    unsigned hash;
    bool containsOnlyASCII;
    bool containsOnlyLatin1;
    bool containsOnlySpaces;
    int defaultWritingDirection; // -1 for std::nullopt
    std::u32string_view codePointsAt; // codePointAt(i) for i in [0, length); 0 beyond
};

inline const std::vector<Properties> properties {
    { true, 0, true, 0, 0U, true, true, true, -1, U""sv },
    { true, 0, true, 0, 15648162U, true, true, true, -1, U""sv },
    { false, 12, true, 12, 6673091U, true, true, false, 0, U"Hello, World"sv },
    { false, 14, true, 14, 16733129U, false, true, false, 0, U"Caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv },
    { false, 20, false, 40, 3940260U, false, false, false, 0, U"\x0421\x0442\x0440\x043E\x043A\x0430 Hello \x00E9 World"sv },
    { false, 18, true, 18, 12226567U, true, true, false, 0, U"Atomic Hello World"sv },
    { false, 26, false, 52, 8936668U, false, false, false, 0, U"\x0130stanbul ISTANBUL istanbul"sv },
    { false, 38, true, 38, 9498626U, true, true, false, 0, U"  Leading and trailing \x0009 whitespace \x000A "sv },
    { false, 7, true, 7, 16672726U, true, true, false, 0, U"a,b,,c,"sv },
    { false, 3, true, 3, 11749649U, true, true, false, -1, U"1.5"sv },
    { false, 14, true, 14, 11207151U, true, true, false, 0, U"  -42.25e1junk"sv },
    { false, 11, false, 22, 10991832U, false, false, false, 0, U"\x1F600\xDE00 emoji \x1F600\xDE00"sv },
    { false, 14, true, 14, 8181427U, true, true, false, 0, U"lowercase only"sv },
    { false, 14, true, 14, 7751889U, true, true, false, 0, U"UPPERCASE ONLY"sv },
};

struct CStrings {
    std::string_view ascii;
    std::string_view latin1;
    std::string_view utf8;
    std::string_view utf8Strict;
};

inline const std::vector<CStrings> cStrings {
    { ""sv, ""sv, ""sv, ""sv },
    { ""sv, ""sv, ""sv, ""sv },
    { "Hello, World"sv, "Hello, World"sv, "Hello, World"sv, "Hello, World"sv },
    { "Caf? cr?me ?t?"sv, "Caf\xE9 cr\xE8me \xC9t\xE9"sv, "Caf\xC3\xA9 cr\xC3\xA8me \xC3\x89t\xC3\xA9"sv, "Caf\xC3\xA9 cr\xC3\xA8me \xC3\x89t\xC3\xA9"sv },
    { "?????? Hello ? World"sv, "?????? Hello \xE9 World"sv, "\xD0\xA1\xD1\x82\xD1\x80\xD0\xBE\xD0\xBA\xD0\xB0 Hello \xC3\xA9 World"sv, "\xD0\xA1\xD1\x82\xD1\x80\xD0\xBE\xD0\xBA\xD0\xB0 Hello \xC3\xA9 World"sv },
    { "Atomic Hello World"sv, "Atomic Hello World"sv, "Atomic Hello World"sv, "Atomic Hello World"sv },
    { "?stanbul ISTANBUL istanbul"sv, "?stanbul ISTANBUL istanbul"sv, "\xC4\xB0stanbul ISTANBUL istanbul"sv, "\xC4\xB0stanbul ISTANBUL istanbul"sv },
    { "  Leading and trailing ? whitespace ? "sv, "  Leading and trailing \x09 whitespace \x0A "sv, "  Leading and trailing \x09 whitespace \x0A "sv, "  Leading and trailing \x09 whitespace \x0A "sv },
    { "a,b,,c,"sv, "a,b,,c,"sv, "a,b,,c,"sv, "a,b,,c,"sv },
    { "1.5"sv, "1.5"sv, "1.5"sv, "1.5"sv },
    { "  -42.25e1junk"sv, "  -42.25e1junk"sv, "  -42.25e1junk"sv, "  -42.25e1junk"sv },
    { "?? emoji ??"sv, "?? emoji ??"sv, "\xF0\x9F\x98\x80 emoji \xF0\x9F\x98\x80"sv, "\xF0\x9F\x98\x80 emoji \xF0\x9F\x98\x80"sv },
    { "lowercase only"sv, "lowercase only"sv, "lowercase only"sv, "lowercase only"sv },
    { "UPPERCASE ONLY"sv, "UPPERCASE ONLY"sv, "UPPERCASE ONLY"sv, "UPPERCASE ONLY"sv },
};

inline const std::vector<ExpectedString> searchTargets {
    N,
    u""sv,
    u"o"sv,
    u"World"sv,
    u"world"sv,
    u"HELLO"sv,
    u"\x00E9\x0000"sv,
    u","sv,
    u"zzz"sv,
};

inline constexpr unsigned searchStarts[] { 0, 1, 5, 100 };
inline constexpr unsigned infixPositions[] { 0, 1, 7, 12 };

struct SearchFromStart {
    size_t findO;
    size_t findEAcute;
    size_t findWorld;
    size_t findUpper;
    size_t findComma;
    size_t reverseFindO;
    size_t reverseFindOLiteral;
};

struct SearchOther {
    size_t reverseFindO;
    bool startsWithH;
    bool endsWithD;
    bool endsWithComma;
    bool containsComma;
    bool containsHello;
    bool containsDigit;
};

struct SearchForTarget {
    size_t find;
    size_t findFrom3;
    size_t findIgnoringASCIICase;
    size_t findIgnoringASCIICaseFrom2;
    size_t reverseFind;
    size_t reverseFindFrom4;
    bool contains;
    bool containsIgnoringASCIICase;
    bool containsIgnoringASCIICaseFrom3;
    bool startsWith;
    bool startsWithIgnoringASCIICase;
    bool endsWith;
    bool endsWithIgnoringASCIICase;
    bool hasInfixStartingAt[4];
    bool hasInfixEndingAt[4];
};

struct Search {
    SearchFromStart fromStart[4];
    SearchOther other;
    SearchForTarget forTarget[9];
};

inline const std::vector<Search> search {
    {
        {
            { nf, nf, nf, nf, nf, nf, nf },
            { nf, nf, nf, nf, nf, nf, nf },
            { nf, nf, nf, nf, nf, nf, nf },
            { nf, nf, nf, nf, nf, nf, nf },
        },
        { nf, false, false, false, false, false, false }, {
            { nf, nf, nf, nf, nf, nf, false, false, false, true, true, true, true, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, true, true, true, true, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
        },
    }, {
        {
            { nf, nf, nf, nf, nf, nf, nf },
            { nf, nf, nf, nf, nf, nf, nf },
            { nf, nf, nf, nf, nf, nf, nf },
            { nf, nf, nf, nf, nf, nf, nf },
        },
        { nf, false, false, false, false, false, false }, {
            { nf, nf, nf, nf, nf, nf, false, false, false, true, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { 0, nf, 0, 0, 0, 0, true, true, true, true, true, true, true, { true, false, false, false }, { true, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
        },
    }, {
        {
            { 4, nf, 7, 0, 5, nf, nf },
            { 4, nf, 7, 7, 5, nf, nf },
            { 8, nf, 7, 7, 5, 4, 4 },
            { nf, nf, nf, nf, nf, 8, 8 },
        },
        { 8, true, true, false, true, true, false }, {
            { nf, nf, nf, nf, nf, nf, false, false, false, true, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { 0, 3, 0, 2, 12, 4, true, true, true, true, true, true, true, { true, true, true, true }, { true, true, true, true } },
            { 4, 4, 4, 4, 8, 4, true, true, true, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { 7, 7, 7, 7, 7, nf, true, true, true, false, false, true, true, { false, false, true, false }, { false, false, false, true } },
            { nf, nf, 7, 7, nf, nf, false, true, true, false, false, false, true, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, 0, nf, nf, nf, false, true, false, false, true, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { 5, 5, 5, 5, 5, nf, true, true, true, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
        },
    }, {
        {
            { nf, 3, nf, 0, nf, nf, nf },
            { nf, 3, nf, nf, nf, nf, nf },
            { nf, 13, nf, nf, nf, nf, nf },
            { nf, nf, nf, nf, nf, nf, nf },
        },
        { nf, false, false, false, false, false, false }, {
            { nf, nf, nf, nf, nf, nf, false, false, false, true, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { 0, 3, 0, 2, 14, 4, true, true, true, true, true, true, true, { true, true, true, true }, { true, true, true, true } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
        },
    }, {
        {
            { 11, 13, 15, 7, nf, nf, nf },
            { 11, 13, 15, 7, nf, nf, nf },
            { 11, 13, 15, 7, nf, nf, nf },
            { nf, nf, nf, nf, nf, 16, 16 },
        },
        { 16, false, true, false, false, true, false }, {
            { nf, nf, nf, nf, nf, nf, false, false, false, true, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { 0, 3, 0, 2, 20, 4, true, true, true, true, true, true, true, { true, true, true, true }, { true, true, true, true } },
            { 11, 11, 11, 11, 16, nf, true, true, true, false, false, false, false, { false, false, false, false }, { false, false, false, true } },
            { 15, 15, 15, 15, 15, nf, true, true, true, false, false, true, true, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, 15, 15, nf, nf, false, true, true, false, false, false, true, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, 7, 7, nf, nf, false, true, true, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
        },
    }, {
        {
            { 2, nf, 13, 0, nf, nf, nf },
            { 2, nf, 13, 7, nf, nf, nf },
            { 11, nf, 13, 7, nf, 2, 2 },
            { nf, nf, nf, nf, nf, 14, 14 },
        },
        { 14, false, true, false, false, true, false }, {
            { nf, nf, nf, nf, nf, nf, false, false, false, true, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { 0, 3, 0, 2, 18, 4, true, true, true, true, true, true, true, { true, true, true, true }, { true, true, true, true } },
            { 2, 11, 2, 2, 14, 2, true, true, true, false, false, false, false, { false, false, false, false }, { false, false, false, true } },
            { 13, 13, 13, 13, 13, nf, true, true, true, false, false, true, true, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, 13, 13, nf, nf, false, true, true, false, false, false, true, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, 7, 7, nf, nf, false, true, true, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
        },
    }, {
        {
            { nf, nf, nf, 9, nf, nf, nf },
            { nf, nf, nf, 9, nf, nf, nf },
            { nf, nf, nf, 9, nf, nf, nf },
            { nf, nf, nf, nf, nf, nf, nf },
        },
        { nf, false, false, false, false, false, false }, {
            { nf, nf, nf, nf, nf, nf, false, false, false, true, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { 0, 3, 0, 2, 26, 4, true, true, true, true, true, true, true, { true, true, true, true }, { true, true, true, true } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
        },
    }, {
        {
            { nf, nf, nf, 2, nf, nf, nf },
            { nf, nf, nf, 2, nf, nf, nf },
            { nf, nf, nf, nf, nf, nf, nf },
            { nf, nf, nf, nf, nf, nf, nf },
        },
        { nf, false, false, false, false, false, false }, {
            { nf, nf, nf, nf, nf, nf, false, false, false, true, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { 0, 3, 0, 2, 38, 4, true, true, true, true, true, true, true, { true, true, true, true }, { true, true, true, true } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
        },
    }, {
        {
            { nf, nf, nf, nf, 1, nf, nf },
            { nf, nf, nf, nf, 1, nf, nf },
            { nf, nf, nf, nf, 6, nf, nf },
            { nf, nf, nf, nf, nf, nf, nf },
        },
        { nf, false, false, true, true, false, false }, {
            { nf, nf, nf, nf, nf, nf, false, false, false, true, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { 0, 3, 0, 2, 7, 4, true, true, true, true, true, true, true, { true, true, true, false }, { true, true, true, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { 1, 3, 1, 3, 6, 4, true, true, true, false, false, true, true, { false, true, false, false }, { false, false, true, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
        },
    }, {
        {
            { nf, nf, nf, nf, nf, nf, nf },
            { nf, nf, nf, nf, nf, nf, nf },
            { nf, nf, nf, nf, nf, nf, nf },
            { nf, nf, nf, nf, nf, nf, nf },
        },
        { nf, false, false, false, false, false, true }, {
            { nf, nf, nf, nf, nf, nf, false, false, false, true, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { 0, 3, 0, 2, 3, 3, true, true, true, true, true, true, true, { true, true, false, false }, { true, true, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
        },
    }, {
        {
            { nf, nf, nf, nf, nf, nf, nf },
            { nf, nf, nf, nf, nf, nf, nf },
            { nf, nf, nf, nf, nf, nf, nf },
            { nf, nf, nf, nf, nf, nf, nf },
        },
        { nf, false, false, false, false, false, true }, {
            { nf, nf, nf, nf, nf, nf, false, false, false, true, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { 0, 3, 0, 2, 14, 4, true, true, true, true, true, true, true, { true, true, true, true }, { true, true, true, true } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
        },
    }, {
        {
            { 5, nf, nf, nf, nf, nf, nf },
            { 5, nf, nf, nf, nf, nf, nf },
            { 5, nf, nf, nf, nf, 5, 5 },
            { nf, nf, nf, nf, nf, 5, 5 },
        },
        { 5, false, false, false, false, false, false }, {
            { nf, nf, nf, nf, nf, nf, false, false, false, true, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { 0, 3, 0, 2, 11, 4, true, true, true, true, true, true, true, { true, true, true, false }, { true, true, true, false } },
            { 5, 5, 5, 5, 5, nf, true, true, true, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
        },
    }, {
        {
            { 1, nf, nf, nf, nf, nf, nf },
            { 1, nf, nf, nf, nf, 1, 1 },
            { 10, nf, nf, nf, nf, 1, 1 },
            { nf, nf, nf, nf, nf, 10, 10 },
        },
        { 10, false, false, false, false, false, false }, {
            { nf, nf, nf, nf, nf, nf, false, false, false, true, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { 0, 3, 0, 2, 14, 4, true, true, true, true, true, true, true, { true, true, true, true }, { true, true, true, true } },
            { 1, 10, 1, 10, 10, 1, true, true, true, false, false, false, false, { false, true, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
        },
    }, {
        {
            { nf, nf, nf, 0, nf, nf, nf },
            { nf, nf, nf, 1, nf, nf, nf },
            { nf, nf, nf, 5, nf, nf, nf },
            { nf, nf, nf, nf, nf, nf, nf },
        },
        { nf, false, false, false, false, false, false }, {
            { nf, nf, nf, nf, nf, nf, false, false, false, true, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { 0, 3, 0, 2, 14, 4, true, true, true, true, true, true, true, { true, true, true, true }, { true, true, true, true } },
            { nf, nf, 10, 10, nf, nf, false, true, true, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
            { nf, nf, nf, nf, nf, nf, false, false, false, false, false, false, false, { false, false, false, false }, { false, false, false, false } },
        },
    },
};

inline constexpr unsigned transformPositions[] { 0, 1, 3, 100 };
inline constexpr unsigned substringLengths[] { 0, 2, 5, std::numeric_limits<unsigned>::max() };
inline constexpr unsigned removalPositions[] { 0, 2, 11, 50 };
inline constexpr unsigned removalLengths[] { 0, 1, 4, 100 };

struct Transforms {
    ExpectedString substring[4][4]; // [position][length]
    ExpectedString substringFrom[4];
    ExpectedString left[4];
    ExpectedString right[4]; // N where position > length
    ExpectedString asciiLowercase;
    ExpectedString asciiUppercase;
    ExpectedString lowercaseWithoutLocale;
    ExpectedString uppercaseWithoutLocale;
    ExpectedString lowercaseWithLocale[5]; // null, empty, tr, lt-LT, el
    ExpectedString uppercaseWithLocale[5];
    ExpectedString simplifiedWhiteSpace;
    ExpectedString trimmed;
    ExpectedString withoutSpaces;
    ExpectedString onlySpaces;
    ExpectedString foldedCase;
    ExpectedString replacedOWithZero;
    ExpectedString replacedOWithOO;
    ExpectedString replacedLWithEAcuteAMacron;
    ExpectedString replacedWorldWithThere;
    ExpectedString removing[4][4]; // [position][length]
    ExpectedString replacing[4][4]; // [position][length], N where not applicable
    bool is8BitAfterConvertTo16Bit;
};

inline const std::vector<Transforms> transforms {
    {
        {
            { N, N, N, N, },
            { N, N, N, N, },
            { N, N, N, N, },
            { N, N, N, N, },
        },
        { N, N, N, N, },
        { N, N, N, N, },
        { N, N, N, N, },
        N, N, N, N,
        { N, N, N, N, N, },
        { N, N, N, N, N, },
        N, N, N, N, N,
        N, N, N, N, {
            { N, N, N, N, },
            { N, N, N, N, },
            { N, N, N, N, },
            { N, N, N, N, },
        }, {
            { N, N, N, N, },
            { N, N, N, N, },
            { N, N, N, N, },
            { N, N, N, N, },
        },
        true,
    }, {
        {
            { u""sv, u""sv, u""sv, u""sv, },
            { u""sv, u""sv, u""sv, u""sv, },
            { u""sv, u""sv, u""sv, u""sv, },
            { u""sv, u""sv, u""sv, u""sv, },
        },
        { u""sv, u""sv, u""sv, u""sv, },
        { u""sv, u""sv, u""sv, u""sv, },
        { u""sv, N, N, N, },
        u""sv, u""sv, u""sv, u""sv,
        { u""sv, u""sv, u""sv, u""sv, u""sv, },
        { u""sv, u""sv, u""sv, u""sv, u""sv, },
        u""sv, u""sv, u""sv, u""sv, u""sv,
        u""sv, u""sv, u""sv, u""sv, {
            { u""sv, u""sv, u""sv, u""sv, },
            { u""sv, u""sv, u""sv, u""sv, },
            { u""sv, u""sv, u""sv, u""sv, },
            { u""sv, u""sv, u""sv, u""sv, },
        }, {
            { u"<>"sv, u"<>"sv, u"<>"sv, u"<>"sv, },
            { N, N, N, N, },
            { N, N, N, N, },
            { N, N, N, N, },
        },
        true,
    }, {
        {
            { u""sv, u"He"sv, u"Hello"sv, u"Hello, World"sv, },
            { u""sv, u"el"sv, u"ello,"sv, u"ello, World"sv, },
            { u""sv, u"lo"sv, u"lo, W"sv, u"lo, World"sv, },
            { u""sv, u""sv, u""sv, u""sv, },
        },
        { u"Hello, World"sv, u"ello, World"sv, u"lo, World"sv, u""sv, },
        { u""sv, u"H"sv, u"Hel"sv, u"Hello, World"sv, },
        { u""sv, u"d"sv, u"rld"sv, N, },
        u"hello, world"sv, u"HELLO, WORLD"sv, u"hello, world"sv, u"HELLO, WORLD"sv,
        { u"hello, world"sv, u"hello, world"sv, u"hello, world"sv, u"hello, world"sv, u"hello, world"sv, },
        { u"HELLO, WORLD"sv, u"HELLO, WORLD"sv, u"HELLO, WORLD"sv, u"HELLO, WORLD"sv, u"HELLO, WORLD"sv, },
        u"Hello, World"sv, u"Hello, World"sv, u"Hello,World"sv, u" "sv, u"hello, world"sv,
        u"Hell0, W0rld"sv, u"HellOO, WOOrld"sv, u"He\x00E9\x0100\x00E9\x0100o, Wor\x00E9\x0100" u"d"sv, u"Hello, There"sv, {
            { u"Hello, World"sv, u"ello, World"sv, u"o, World"sv, u""sv, },
            { u"Hello, World"sv, u"Helo, World"sv, u"He World"sv, u"He"sv, },
            { u"Hello, World"sv, u"Hello, Worl"sv, u"Hello, Worl"sv, u"Hello, Worl"sv, },
            { u"Hello, World"sv, u"Hello, World"sv, u"Hello, World"sv, u"Hello, World"sv, },
        }, {
            { u"<>Hello, World"sv, u"<>ello, World"sv, u"<>o, World"sv, u"<>"sv, },
            { u"He<>llo, World"sv, u"He<>lo, World"sv, u"He<> World"sv, u"He<>"sv, },
            { u"Hello, Worl<>d"sv, u"Hello, Worl<>"sv, u"Hello, Worl<>"sv, u"Hello, Worl<>"sv, },
            { N, N, N, N, },
        },
        false,
    }, {
        {
            { u""sv, u"Ca"sv, u"Caf\x00E9 "sv, u"Caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv, },
            { u""sv, u"af"sv, u"af\x00E9 c"sv, u"af\x00E9 cr\x00E8me \x00C9t\x00E9"sv, },
            { u""sv, u"\x00E9 "sv, u"\x00E9 cr\x00E8"sv, u"\x00E9 cr\x00E8me \x00C9t\x00E9"sv, },
            { u""sv, u""sv, u""sv, u""sv, },
        },
        { u"Caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv, u"af\x00E9 cr\x00E8me \x00C9t\x00E9"sv, u"\x00E9 cr\x00E8me \x00C9t\x00E9"sv, u""sv, },
        { u""sv, u"C"sv, u"Caf"sv, u"Caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv, },
        { u""sv, u"\x00E9"sv, u"\x00C9t\x00E9"sv, N, },
        u"caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv, u"CAF\x00E9 CR\x00E8ME \x00C9T\x00E9"sv, u"caf\x00E9 cr\x00E8me \x00E9t\x00E9"sv, u"CAF\x00C9 CR\x00C8ME \x00C9T\x00C9"sv,
        { u"caf\x00E9 cr\x00E8me \x00E9t\x00E9"sv, u"caf\x00E9 cr\x00E8me \x00E9t\x00E9"sv, u"caf\x00E9 cr\x00E8me \x00E9t\x00E9"sv, u"caf\x00E9 cr\x00E8me \x00E9t\x00E9"sv, u"caf\x00E9 cr\x00E8me \x00E9t\x00E9"sv, },
        { u"CAF\x00C9 CR\x00C8ME \x00C9T\x00C9"sv, u"CAF\x00C9 CR\x00C8ME \x00C9T\x00C9"sv, u"CAF\x00C9 CR\x00C8ME \x00C9T\x00C9"sv, u"CAF\x00C9 CR\x00C8ME \x00C9T\x00C9"sv, u"CAF\x00C9 CR\x00C8ME \x00C9T\x00C9"sv, },
        u"Caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv, u"Caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv, u"Caf\x00E9" u"cr\x00E8me\x00C9t\x00E9"sv, u"  "sv, u"caf\x00E9 cr\x00E8me \x00E9t\x00E9"sv,
        u"Caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv, u"Caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv, u"Caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv, u"Caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv, {
            { u"Caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv, u"af\x00E9 cr\x00E8me \x00C9t\x00E9"sv, u" cr\x00E8me \x00C9t\x00E9"sv, u""sv, },
            { u"Caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv, u"Ca\x00E9 cr\x00E8me \x00C9t\x00E9"sv, u"Car\x00E8me \x00C9t\x00E9"sv, u"Ca"sv, },
            { u"Caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv, u"Caf\x00E9 cr\x00E8me t\x00E9"sv, u"Caf\x00E9 cr\x00E8me "sv, u"Caf\x00E9 cr\x00E8me "sv, },
            { u"Caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv, u"Caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv, u"Caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv, u"Caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv, },
        }, {
            { u"<>Caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv, u"<>af\x00E9 cr\x00E8me \x00C9t\x00E9"sv, u"<> cr\x00E8me \x00C9t\x00E9"sv, u"<>"sv, },
            { u"Ca<>f\x00E9 cr\x00E8me \x00C9t\x00E9"sv, u"Ca<>\x00E9 cr\x00E8me \x00C9t\x00E9"sv, u"Ca<>r\x00E8me \x00C9t\x00E9"sv, u"Ca<>"sv, },
            { u"Caf\x00E9 cr\x00E8me <>\x00C9t\x00E9"sv, u"Caf\x00E9 cr\x00E8me <>t\x00E9"sv, u"Caf\x00E9 cr\x00E8me <>"sv, u"Caf\x00E9 cr\x00E8me <>"sv, },
            { N, N, N, N, },
        },
        false,
    }, {
        {
            { u""sv, u"\x0421\x0442"sv, u"\x0421\x0442\x0440\x043E\x043A"sv, u"\x0421\x0442\x0440\x043E\x043A\x0430 Hello \x00E9 World"sv, },
            { u""sv, u"\x0442\x0440"sv, u"\x0442\x0440\x043E\x043A\x0430"sv, u"\x0442\x0440\x043E\x043A\x0430 Hello \x00E9 World"sv, },
            { u""sv, u"\x043E\x043A"sv, u"\x043E\x043A\x0430 H"sv, u"\x043E\x043A\x0430 Hello \x00E9 World"sv, },
            { u""sv, u""sv, u""sv, u""sv, },
        },
        { u"\x0421\x0442\x0440\x043E\x043A\x0430 Hello \x00E9 World"sv, u"\x0442\x0440\x043E\x043A\x0430 Hello \x00E9 World"sv, u"\x043E\x043A\x0430 Hello \x00E9 World"sv, u""sv, },
        { u""sv, u"\x0421"sv, u"\x0421\x0442\x0440"sv, u"\x0421\x0442\x0440\x043E\x043A\x0430 Hello \x00E9 World"sv, },
        { u""sv, u"d"sv, u"rld"sv, N, },
        u"\x0421\x0442\x0440\x043E\x043A\x0430 hello \x00E9 world"sv, u"\x0421\x0442\x0440\x043E\x043A\x0430 HELLO \x00E9 WORLD"sv, u"\x0441\x0442\x0440\x043E\x043A\x0430 hello \x00E9 world"sv, u"\x0421\x0422\x0420\x041E\x041A\x0410 HELLO \x00C9 WORLD"sv,
        { u"\x0441\x0442\x0440\x043E\x043A\x0430 hello \x00E9 world"sv, u"\x0441\x0442\x0440\x043E\x043A\x0430 hello \x00E9 world"sv, u"\x0441\x0442\x0440\x043E\x043A\x0430 hello \x00E9 world"sv, u"\x0441\x0442\x0440\x043E\x043A\x0430 hello \x00E9 world"sv, u"\x0441\x0442\x0440\x043E\x043A\x0430 hello \x00E9 world"sv, },
        { u"\x0421\x0422\x0420\x041E\x041A\x0410 HELLO \x00C9 WORLD"sv, u"\x0421\x0422\x0420\x041E\x041A\x0410 HELLO \x00C9 WORLD"sv, u"\x0421\x0422\x0420\x041E\x041A\x0410 HELLO \x00C9 WORLD"sv, u"\x0421\x0422\x0420\x041E\x041A\x0410 HELLO \x00C9 WORLD"sv, u"\x0421\x0422\x0420\x041E\x041A\x0410 HELLO \x00C9 WORLD"sv, },
        u"\x0421\x0442\x0440\x043E\x043A\x0430 Hello \x00E9 World"sv, u"\x0421\x0442\x0440\x043E\x043A\x0430 Hello \x00E9 World"sv, u"\x0421\x0442\x0440\x043E\x043A\x0430Hello\x00E9World"sv, u"   "sv, u"\x0441\x0442\x0440\x043E\x043A\x0430 hello \x00E9 world"sv,
        u"\x0421\x0442\x0440\x043E\x043A\x0430 Hell0 \x00E9 W0rld"sv, u"\x0421\x0442\x0440\x043E\x043A\x0430 HellOO \x00E9 WOOrld"sv, u"\x0421\x0442\x0440\x043E\x043A\x0430 He\x00E9\x0100\x00E9\x0100o \x00E9 Wor\x00E9\x0100" u"d"sv, u"\x0421\x0442\x0440\x043E\x043A\x0430 Hello \x00E9 There"sv, {
            { u"\x0421\x0442\x0440\x043E\x043A\x0430 Hello \x00E9 World"sv, u"\x0442\x0440\x043E\x043A\x0430 Hello \x00E9 World"sv, u"\x043A\x0430 Hello \x00E9 World"sv, u""sv, },
            { u"\x0421\x0442\x0440\x043E\x043A\x0430 Hello \x00E9 World"sv, u"\x0421\x0442\x043E\x043A\x0430 Hello \x00E9 World"sv, u"\x0421\x0442 Hello \x00E9 World"sv, u"\x0421\x0442"sv, },
            { u"\x0421\x0442\x0440\x043E\x043A\x0430 Hello \x00E9 World"sv, u"\x0421\x0442\x0440\x043E\x043A\x0430 Hell \x00E9 World"sv, u"\x0421\x0442\x0440\x043E\x043A\x0430 HellWorld"sv, u"\x0421\x0442\x0440\x043E\x043A\x0430 Hell"sv, },
            { u"\x0421\x0442\x0440\x043E\x043A\x0430 Hello \x00E9 World"sv, u"\x0421\x0442\x0440\x043E\x043A\x0430 Hello \x00E9 World"sv, u"\x0421\x0442\x0440\x043E\x043A\x0430 Hello \x00E9 World"sv, u"\x0421\x0442\x0440\x043E\x043A\x0430 Hello \x00E9 World"sv, },
        }, {
            { u"<>\x0421\x0442\x0440\x043E\x043A\x0430 Hello \x00E9 World"sv, u"<>\x0442\x0440\x043E\x043A\x0430 Hello \x00E9 World"sv, u"<>\x043A\x0430 Hello \x00E9 World"sv, u"<>"sv, },
            { u"\x0421\x0442<>\x0440\x043E\x043A\x0430 Hello \x00E9 World"sv, u"\x0421\x0442<>\x043E\x043A\x0430 Hello \x00E9 World"sv, u"\x0421\x0442<> Hello \x00E9 World"sv, u"\x0421\x0442<>"sv, },
            { u"\x0421\x0442\x0440\x043E\x043A\x0430 Hell<>o \x00E9 World"sv, u"\x0421\x0442\x0440\x043E\x043A\x0430 Hell<> \x00E9 World"sv, u"\x0421\x0442\x0440\x043E\x043A\x0430 Hell<>World"sv, u"\x0421\x0442\x0440\x043E\x043A\x0430 Hell<>"sv, },
            { N, N, N, N, },
        },
        false,
    }, {
        {
            { u""sv, u"At"sv, u"Atomi"sv, u"Atomic Hello World"sv, },
            { u""sv, u"to"sv, u"tomic"sv, u"tomic Hello World"sv, },
            { u""sv, u"mi"sv, u"mic H"sv, u"mic Hello World"sv, },
            { u""sv, u""sv, u""sv, u""sv, },
        },
        { u"Atomic Hello World"sv, u"tomic Hello World"sv, u"mic Hello World"sv, u""sv, },
        { u""sv, u"A"sv, u"Ato"sv, u"Atomic Hello World"sv, },
        { u""sv, u"d"sv, u"rld"sv, N, },
        u"atomic hello world"sv, u"ATOMIC HELLO WORLD"sv, u"atomic hello world"sv, u"ATOMIC HELLO WORLD"sv,
        { u"atomic hello world"sv, u"atomic hello world"sv, u"atomic hello world"sv, u"atomic hello world"sv, u"atomic hello world"sv, },
        { u"ATOMIC HELLO WORLD"sv, u"ATOMIC HELLO WORLD"sv, u"ATOM\x0130" u"C HELLO WORLD"sv, u"ATOMIC HELLO WORLD"sv, u"ATOMIC HELLO WORLD"sv, },
        u"Atomic Hello World"sv, u"Atomic Hello World"sv, u"AtomicHelloWorld"sv, u"  "sv, u"atomic hello world"sv,
        u"At0mic Hell0 W0rld"sv, u"AtOOmic HellOO WOOrld"sv, u"Atomic He\x00E9\x0100\x00E9\x0100o Wor\x00E9\x0100" u"d"sv, u"Atomic Hello There"sv, {
            { u"Atomic Hello World"sv, u"tomic Hello World"sv, u"ic Hello World"sv, u""sv, },
            { u"Atomic Hello World"sv, u"Atmic Hello World"sv, u"At Hello World"sv, u"At"sv, },
            { u"Atomic Hello World"sv, u"Atomic Hell World"sv, u"Atomic Hellrld"sv, u"Atomic Hell"sv, },
            { u"Atomic Hello World"sv, u"Atomic Hello World"sv, u"Atomic Hello World"sv, u"Atomic Hello World"sv, },
        }, {
            { u"<>Atomic Hello World"sv, u"<>tomic Hello World"sv, u"<>ic Hello World"sv, u"<>"sv, },
            { u"At<>omic Hello World"sv, u"At<>mic Hello World"sv, u"At<> Hello World"sv, u"At<>"sv, },
            { u"Atomic Hell<>o World"sv, u"Atomic Hell<> World"sv, u"Atomic Hell<>rld"sv, u"Atomic Hell<>"sv, },
            { N, N, N, N, },
        },
        false,
    }, {
        {
            { u""sv, u"\x0130s"sv, u"\x0130stan"sv, u"\x0130stanbul ISTANBUL istanbul"sv, },
            { u""sv, u"st"sv, u"stanb"sv, u"stanbul ISTANBUL istanbul"sv, },
            { u""sv, u"an"sv, u"anbul"sv, u"anbul ISTANBUL istanbul"sv, },
            { u""sv, u""sv, u""sv, u""sv, },
        },
        { u"\x0130stanbul ISTANBUL istanbul"sv, u"stanbul ISTANBUL istanbul"sv, u"anbul ISTANBUL istanbul"sv, u""sv, },
        { u""sv, u"\x0130"sv, u"\x0130st"sv, u"\x0130stanbul ISTANBUL istanbul"sv, },
        { u""sv, u"l"sv, u"bul"sv, N, },
        u"\x0130stanbul istanbul istanbul"sv, u"\x0130STANBUL ISTANBUL ISTANBUL"sv, u"i\x0307stanbul istanbul istanbul"sv, u"\x0130STANBUL ISTANBUL ISTANBUL"sv,
        { u"i\x0307stanbul istanbul istanbul"sv, u"i\x0307stanbul istanbul istanbul"sv, u"istanbul \x0131stanbul istanbul"sv, u"i\x0307stanbul istanbul istanbul"sv, u"i\x0307stanbul istanbul istanbul"sv, },
        { u"\x0130STANBUL ISTANBUL ISTANBUL"sv, u"\x0130STANBUL ISTANBUL ISTANBUL"sv, u"\x0130STANBUL ISTANBUL \x0130STANBUL"sv, u"\x0130STANBUL ISTANBUL ISTANBUL"sv, u"\x0130STANBUL ISTANBUL ISTANBUL"sv, },
        u"\x0130stanbul ISTANBUL istanbul"sv, u"\x0130stanbul ISTANBUL istanbul"sv, u"\x0130stanbulISTANBUListanbul"sv, u"  "sv, u"i\x0307stanbul istanbul istanbul"sv,
        u"\x0130stanbul ISTANBUL istanbul"sv, u"\x0130stanbul ISTANBUL istanbul"sv, u"\x0130stanbu\x00E9\x0100 ISTANBUL istanbu\x00E9\x0100"sv, u"\x0130stanbul ISTANBUL istanbul"sv, {
            { u"\x0130stanbul ISTANBUL istanbul"sv, u"stanbul ISTANBUL istanbul"sv, u"nbul ISTANBUL istanbul"sv, u""sv, },
            { u"\x0130stanbul ISTANBUL istanbul"sv, u"\x0130sanbul ISTANBUL istanbul"sv, u"\x0130sul ISTANBUL istanbul"sv, u"\x0130s"sv, },
            { u"\x0130stanbul ISTANBUL istanbul"sv, u"\x0130stanbul ISANBUL istanbul"sv, u"\x0130stanbul ISUL istanbul"sv, u"\x0130stanbul IS"sv, },
            { u"\x0130stanbul ISTANBUL istanbul"sv, u"\x0130stanbul ISTANBUL istanbul"sv, u"\x0130stanbul ISTANBUL istanbul"sv, u"\x0130stanbul ISTANBUL istanbul"sv, },
        }, {
            { u"<>\x0130stanbul ISTANBUL istanbul"sv, u"<>stanbul ISTANBUL istanbul"sv, u"<>nbul ISTANBUL istanbul"sv, u"<>"sv, },
            { u"\x0130s<>tanbul ISTANBUL istanbul"sv, u"\x0130s<>anbul ISTANBUL istanbul"sv, u"\x0130s<>ul ISTANBUL istanbul"sv, u"\x0130s<>"sv, },
            { u"\x0130stanbul IS<>TANBUL istanbul"sv, u"\x0130stanbul IS<>ANBUL istanbul"sv, u"\x0130stanbul IS<>UL istanbul"sv, u"\x0130stanbul IS<>"sv, },
            { N, N, N, N, },
        },
        false,
    }, {
        {
            { u""sv, u"  "sv, u"  Lea"sv, u"  Leading and trailing \x0009 whitespace \x000A "sv, },
            { u""sv, u" L"sv, u" Lead"sv, u" Leading and trailing \x0009 whitespace \x000A "sv, },
            { u""sv, u"ea"sv, u"eadin"sv, u"eading and trailing \x0009 whitespace \x000A "sv, },
            { u""sv, u""sv, u""sv, u""sv, },
        },
        { u"  Leading and trailing \x0009 whitespace \x000A "sv, u" Leading and trailing \x0009 whitespace \x000A "sv, u"eading and trailing \x0009 whitespace \x000A "sv, u""sv, },
        { u""sv, u" "sv, u"  L"sv, u"  Leading and trailing \x0009 whitespace \x000A "sv, },
        { u""sv, u" "sv, u" \x000A "sv, N, },
        u"  leading and trailing \x0009 whitespace \x000A "sv, u"  LEADING AND TRAILING \x0009 WHITESPACE \x000A "sv, u"  leading and trailing \x0009 whitespace \x000A "sv, u"  LEADING AND TRAILING \x0009 WHITESPACE \x000A "sv,
        { u"  leading and trailing \x0009 whitespace \x000A "sv, u"  leading and trailing \x0009 whitespace \x000A "sv, u"  leading and trailing \x0009 whitespace \x000A "sv, u"  leading and trailing \x0009 whitespace \x000A "sv, u"  leading and trailing \x0009 whitespace \x000A "sv, },
        { u"  LEADING AND TRAILING \x0009 WHITESPACE \x000A "sv, u"  LEADING AND TRAILING \x0009 WHITESPACE \x000A "sv, u"  LEAD\x0130NG AND TRA\x0130L\x0130NG \x0009 WH\x0130TESPACE \x000A "sv, u"  LEADING AND TRAILING \x0009 WHITESPACE \x000A "sv, u"  LEADING AND TRAILING \x0009 WHITESPACE \x000A "sv, },
        u"Leading and trailing whitespace"sv, u"Leading and trailing \x0009 whitespace"sv, u"Leadingandtrailingwhitespace"sv, u"     \x0009  \x000A "sv, u"  leading and trailing \x0009 whitespace \x000A "sv,
        u"  Leading and trailing \x0009 whitespace \x000A "sv, u"  Leading and trailing \x0009 whitespace \x000A "sv, u"  Leading and trai\x00E9\x0100ing \x0009 whitespace \x000A "sv, u"  Leading and trailing \x0009 whitespace \x000A "sv, {
            { u"  Leading and trailing \x0009 whitespace \x000A "sv, u" Leading and trailing \x0009 whitespace \x000A "sv, u"ading and trailing \x0009 whitespace \x000A "sv, u""sv, },
            { u"  Leading and trailing \x0009 whitespace \x000A "sv, u"  eading and trailing \x0009 whitespace \x000A "sv, u"  ing and trailing \x0009 whitespace \x000A "sv, u"  "sv, },
            { u"  Leading and trailing \x0009 whitespace \x000A "sv, u"  Leading ad trailing \x0009 whitespace \x000A "sv, u"  Leading arailing \x0009 whitespace \x000A "sv, u"  Leading a"sv, },
            { u"  Leading and trailing \x0009 whitespace \x000A "sv, u"  Leading and trailing \x0009 whitespace \x000A "sv, u"  Leading and trailing \x0009 whitespace \x000A "sv, u"  Leading and trailing \x0009 whitespace \x000A "sv, },
        }, {
            { u"<>  Leading and trailing \x0009 whitespace \x000A "sv, u"<> Leading and trailing \x0009 whitespace \x000A "sv, u"<>ading and trailing \x0009 whitespace \x000A "sv, u"<>"sv, },
            { u"  <>Leading and trailing \x0009 whitespace \x000A "sv, u"  <>eading and trailing \x0009 whitespace \x000A "sv, u"  <>ing and trailing \x0009 whitespace \x000A "sv, u"  <>"sv, },
            { u"  Leading a<>nd trailing \x0009 whitespace \x000A "sv, u"  Leading a<>d trailing \x0009 whitespace \x000A "sv, u"  Leading a<>railing \x0009 whitespace \x000A "sv, u"  Leading a<>"sv, },
            { N, N, N, N, },
        },
        false,
    }, {
        {
            { u""sv, u"a,"sv, u"a,b,,"sv, u"a,b,,c,"sv, },
            { u""sv, u",b"sv, u",b,,c"sv, u",b,,c,"sv, },
            { u""sv, u",,"sv, u",,c,"sv, u",,c,"sv, },
            { u""sv, u""sv, u""sv, u""sv, },
        },
        { u"a,b,,c,"sv, u",b,,c,"sv, u",,c,"sv, u""sv, },
        { u""sv, u"a"sv, u"a,b"sv, u"a,b,,c,"sv, },
        { u""sv, u","sv, u",c,"sv, N, },
        u"a,b,,c,"sv, u"A,B,,C,"sv, u"a,b,,c,"sv, u"A,B,,C,"sv,
        { u"a,b,,c,"sv, u"a,b,,c,"sv, u"a,b,,c,"sv, u"a,b,,c,"sv, u"a,b,,c,"sv, },
        { u"A,B,,C,"sv, u"A,B,,C,"sv, u"A,B,,C,"sv, u"A,B,,C,"sv, u"A,B,,C,"sv, },
        u"a,b,,c,"sv, u"a,b,,c,"sv, u"a,b,,c,"sv, u""sv, u"a,b,,c,"sv,
        u"a,b,,c,"sv, u"a,b,,c,"sv, u"a,b,,c,"sv, u"a,b,,c,"sv, {
            { u"a,b,,c,"sv, u",b,,c,"sv, u",c,"sv, u""sv, },
            { u"a,b,,c,"sv, u"a,,,c,"sv, u"a,,"sv, u"a,"sv, },
            { u"a,b,,c,"sv, u"a,b,,c,"sv, u"a,b,,c,"sv, u"a,b,,c,"sv, },
            { u"a,b,,c,"sv, u"a,b,,c,"sv, u"a,b,,c,"sv, u"a,b,,c,"sv, },
        }, {
            { u"<>a,b,,c,"sv, u"<>,b,,c,"sv, u"<>,c,"sv, u"<>"sv, },
            { u"a,<>b,,c,"sv, u"a,<>,,c,"sv, u"a,<>,"sv, u"a,<>"sv, },
            { N, N, N, N, },
            { N, N, N, N, },
        },
        false,
    }, {
        {
            { u""sv, u"1."sv, u"1.5"sv, u"1.5"sv, },
            { u""sv, u".5"sv, u".5"sv, u".5"sv, },
            { u""sv, u""sv, u""sv, u""sv, },
            { u""sv, u""sv, u""sv, u""sv, },
        },
        { u"1.5"sv, u".5"sv, u""sv, u""sv, },
        { u""sv, u"1"sv, u"1.5"sv, u"1.5"sv, },
        { u""sv, u"5"sv, u"1.5"sv, N, },
        u"1.5"sv, u"1.5"sv, u"1.5"sv, u"1.5"sv,
        { u"1.5"sv, u"1.5"sv, u"1.5"sv, u"1.5"sv, u"1.5"sv, },
        { u"1.5"sv, u"1.5"sv, u"1.5"sv, u"1.5"sv, u"1.5"sv, },
        u"1.5"sv, u"1.5"sv, u"1.5"sv, u""sv, u"1.5"sv,
        u"1.5"sv, u"1.5"sv, u"1.5"sv, u"1.5"sv, {
            { u"1.5"sv, u".5"sv, u""sv, u""sv, },
            { u"1.5"sv, u"1."sv, u"1."sv, u"1."sv, },
            { u"1.5"sv, u"1.5"sv, u"1.5"sv, u"1.5"sv, },
            { u"1.5"sv, u"1.5"sv, u"1.5"sv, u"1.5"sv, },
        }, {
            { u"<>1.5"sv, u"<>.5"sv, u"<>"sv, u"<>"sv, },
            { u"1.<>5"sv, u"1.<>"sv, u"1.<>"sv, u"1.<>"sv, },
            { N, N, N, N, },
            { N, N, N, N, },
        },
        false,
    }, {
        {
            { u""sv, u"  "sv, u"  -42"sv, u"  -42.25e1junk"sv, },
            { u""sv, u" -"sv, u" -42."sv, u" -42.25e1junk"sv, },
            { u""sv, u"42"sv, u"42.25"sv, u"42.25e1junk"sv, },
            { u""sv, u""sv, u""sv, u""sv, },
        },
        { u"  -42.25e1junk"sv, u" -42.25e1junk"sv, u"42.25e1junk"sv, u""sv, },
        { u""sv, u" "sv, u"  -"sv, u"  -42.25e1junk"sv, },
        { u""sv, u"k"sv, u"unk"sv, N, },
        u"  -42.25e1junk"sv, u"  -42.25E1JUNK"sv, u"  -42.25e1junk"sv, u"  -42.25E1JUNK"sv,
        { u"  -42.25e1junk"sv, u"  -42.25e1junk"sv, u"  -42.25e1junk"sv, u"  -42.25e1junk"sv, u"  -42.25e1junk"sv, },
        { u"  -42.25E1JUNK"sv, u"  -42.25E1JUNK"sv, u"  -42.25E1JUNK"sv, u"  -42.25E1JUNK"sv, u"  -42.25E1JUNK"sv, },
        u"-42.25e1junk"sv, u"-42.25e1junk"sv, u"-42.25e1junk"sv, u"  "sv, u"  -42.25e1junk"sv,
        u"  -42.25e1junk"sv, u"  -42.25e1junk"sv, u"  -42.25e1junk"sv, u"  -42.25e1junk"sv, {
            { u"  -42.25e1junk"sv, u" -42.25e1junk"sv, u"2.25e1junk"sv, u""sv, },
            { u"  -42.25e1junk"sv, u"  42.25e1junk"sv, u"  25e1junk"sv, u"  "sv, },
            { u"  -42.25e1junk"sv, u"  -42.25e1jnk"sv, u"  -42.25e1j"sv, u"  -42.25e1j"sv, },
            { u"  -42.25e1junk"sv, u"  -42.25e1junk"sv, u"  -42.25e1junk"sv, u"  -42.25e1junk"sv, },
        }, {
            { u"<>  -42.25e1junk"sv, u"<> -42.25e1junk"sv, u"<>2.25e1junk"sv, u"<>"sv, },
            { u"  <>-42.25e1junk"sv, u"  <>42.25e1junk"sv, u"  <>25e1junk"sv, u"  <>"sv, },
            { u"  -42.25e1j<>unk"sv, u"  -42.25e1j<>nk"sv, u"  -42.25e1j<>"sv, u"  -42.25e1j<>"sv, },
            { N, N, N, N, },
        },
        false,
    }, {
        {
            { u""sv, u"\xD83D\xDE00"sv, u"\xD83D\xDE00 em"sv, u"\xD83D\xDE00 emoji \xD83D\xDE00"sv, },
            { u""sv, u"\xDE00 "sv, u"\xDE00 emo"sv, u"\xDE00 emoji \xD83D\xDE00"sv, },
            { u""sv, u"em"sv, u"emoji"sv, u"emoji \xD83D\xDE00"sv, },
            { u""sv, u""sv, u""sv, u""sv, },
        },
        { u"\xD83D\xDE00 emoji \xD83D\xDE00"sv, u"\xDE00 emoji \xD83D\xDE00"sv, u"emoji \xD83D\xDE00"sv, u""sv, },
        { u""sv, u"\xD83D"sv, u"\xD83D\xDE00 "sv, u"\xD83D\xDE00 emoji \xD83D\xDE00"sv, },
        { u""sv, u"\xDE00"sv, u" \xD83D\xDE00"sv, N, },
        u"\xD83D\xDE00 emoji \xD83D\xDE00"sv, u"\xD83D\xDE00 EMOJI \xD83D\xDE00"sv, u"\xD83D\xDE00 emoji \xD83D\xDE00"sv, u"\xD83D\xDE00 EMOJI \xD83D\xDE00"sv,
        { u"\xD83D\xDE00 emoji \xD83D\xDE00"sv, u"\xD83D\xDE00 emoji \xD83D\xDE00"sv, u"\xD83D\xDE00 emoji \xD83D\xDE00"sv, u"\xD83D\xDE00 emoji \xD83D\xDE00"sv, u"\xD83D\xDE00 emoji \xD83D\xDE00"sv, },
        { u"\xD83D\xDE00 EMOJI \xD83D\xDE00"sv, u"\xD83D\xDE00 EMOJI \xD83D\xDE00"sv, u"\xD83D\xDE00 EMOJ\x0130 \xD83D\xDE00"sv, u"\xD83D\xDE00 EMOJI \xD83D\xDE00"sv, u"\xD83D\xDE00 EMOJI \xD83D\xDE00"sv, },
        u"\xD83D\xDE00 emoji \xD83D\xDE00"sv, u"\xD83D\xDE00 emoji \xD83D\xDE00"sv, u"\xD83D\xDE00" u"emoji\xD83D\xDE00"sv, u"  "sv, u"\xD83D\xDE00 emoji \xD83D\xDE00"sv,
        u"\xD83D\xDE00 em0ji \xD83D\xDE00"sv, u"\xD83D\xDE00 emOOji \xD83D\xDE00"sv, u"\xD83D\xDE00 emoji \xD83D\xDE00"sv, u"\xD83D\xDE00 emoji \xD83D\xDE00"sv, {
            { u"\xD83D\xDE00 emoji \xD83D\xDE00"sv, u"\xDE00 emoji \xD83D\xDE00"sv, u"moji \xD83D\xDE00"sv, u""sv, },
            { u"\xD83D\xDE00 emoji \xD83D\xDE00"sv, u"\xD83D\xDE00" u"emoji \xD83D\xDE00"sv, u"\xD83D\xDE00ji \xD83D\xDE00"sv, u"\xD83D\xDE00"sv, },
            { u"\xD83D\xDE00 emoji \xD83D\xDE00"sv, u"\xD83D\xDE00 emoji \xD83D\xDE00"sv, u"\xD83D\xDE00 emoji \xD83D\xDE00"sv, u"\xD83D\xDE00 emoji \xD83D\xDE00"sv, },
            { u"\xD83D\xDE00 emoji \xD83D\xDE00"sv, u"\xD83D\xDE00 emoji \xD83D\xDE00"sv, u"\xD83D\xDE00 emoji \xD83D\xDE00"sv, u"\xD83D\xDE00 emoji \xD83D\xDE00"sv, },
        }, {
            { u"<>\xD83D\xDE00 emoji \xD83D\xDE00"sv, u"<>\xDE00 emoji \xD83D\xDE00"sv, u"<>moji \xD83D\xDE00"sv, u"<>"sv, },
            { u"\xD83D\xDE00<> emoji \xD83D\xDE00"sv, u"\xD83D\xDE00<>emoji \xD83D\xDE00"sv, u"\xD83D\xDE00<>ji \xD83D\xDE00"sv, u"\xD83D\xDE00<>"sv, },
            { u"\xD83D\xDE00 emoji \xD83D\xDE00<>"sv, u"\xD83D\xDE00 emoji \xD83D\xDE00<>"sv, u"\xD83D\xDE00 emoji \xD83D\xDE00<>"sv, u"\xD83D\xDE00 emoji \xD83D\xDE00<>"sv, },
            { N, N, N, N, },
        },
        false,
    }, {
        {
            { u""sv, u"lo"sv, u"lower"sv, u"lowercase only"sv, },
            { u""sv, u"ow"sv, u"owerc"sv, u"owercase only"sv, },
            { u""sv, u"er"sv, u"ercas"sv, u"ercase only"sv, },
            { u""sv, u""sv, u""sv, u""sv, },
        },
        { u"lowercase only"sv, u"owercase only"sv, u"ercase only"sv, u""sv, },
        { u""sv, u"l"sv, u"low"sv, u"lowercase only"sv, },
        { u""sv, u"y"sv, u"nly"sv, N, },
        u"lowercase only"sv, u"LOWERCASE ONLY"sv, u"lowercase only"sv, u"LOWERCASE ONLY"sv,
        { u"lowercase only"sv, u"lowercase only"sv, u"lowercase only"sv, u"lowercase only"sv, u"lowercase only"sv, },
        { u"LOWERCASE ONLY"sv, u"LOWERCASE ONLY"sv, u"LOWERCASE ONLY"sv, u"LOWERCASE ONLY"sv, u"LOWERCASE ONLY"sv, },
        u"lowercase only"sv, u"lowercase only"sv, u"lowercaseonly"sv, u" "sv, u"lowercase only"sv,
        u"l0wercase 0nly"sv, u"lOOwercase OOnly"sv, u"\x00E9\x0100owercase on\x00E9\x0100y"sv, u"lowercase only"sv, {
            { u"lowercase only"sv, u"owercase only"sv, u"rcase only"sv, u""sv, },
            { u"lowercase only"sv, u"loercase only"sv, u"loase only"sv, u"lo"sv, },
            { u"lowercase only"sv, u"lowercase oly"sv, u"lowercase o"sv, u"lowercase o"sv, },
            { u"lowercase only"sv, u"lowercase only"sv, u"lowercase only"sv, u"lowercase only"sv, },
        }, {
            { u"<>lowercase only"sv, u"<>owercase only"sv, u"<>rcase only"sv, u"<>"sv, },
            { u"lo<>wercase only"sv, u"lo<>ercase only"sv, u"lo<>ase only"sv, u"lo<>"sv, },
            { u"lowercase o<>nly"sv, u"lowercase o<>ly"sv, u"lowercase o<>"sv, u"lowercase o<>"sv, },
            { N, N, N, N, },
        },
        false,
    }, {
        {
            { u""sv, u"UP"sv, u"UPPER"sv, u"UPPERCASE ONLY"sv, },
            { u""sv, u"PP"sv, u"PPERC"sv, u"PPERCASE ONLY"sv, },
            { u""sv, u"ER"sv, u"ERCAS"sv, u"ERCASE ONLY"sv, },
            { u""sv, u""sv, u""sv, u""sv, },
        },
        { u"UPPERCASE ONLY"sv, u"PPERCASE ONLY"sv, u"ERCASE ONLY"sv, u""sv, },
        { u""sv, u"U"sv, u"UPP"sv, u"UPPERCASE ONLY"sv, },
        { u""sv, u"Y"sv, u"NLY"sv, N, },
        u"uppercase only"sv, u"UPPERCASE ONLY"sv, u"uppercase only"sv, u"UPPERCASE ONLY"sv,
        { u"uppercase only"sv, u"uppercase only"sv, u"uppercase only"sv, u"uppercase only"sv, u"uppercase only"sv, },
        { u"UPPERCASE ONLY"sv, u"UPPERCASE ONLY"sv, u"UPPERCASE ONLY"sv, u"UPPERCASE ONLY"sv, u"UPPERCASE ONLY"sv, },
        u"UPPERCASE ONLY"sv, u"UPPERCASE ONLY"sv, u"UPPERCASEONLY"sv, u" "sv, u"uppercase only"sv,
        u"UPPERCASE ONLY"sv, u"UPPERCASE ONLY"sv, u"UPPERCASE ONLY"sv, u"UPPERCASE ONLY"sv, {
            { u"UPPERCASE ONLY"sv, u"PPERCASE ONLY"sv, u"RCASE ONLY"sv, u""sv, },
            { u"UPPERCASE ONLY"sv, u"UPERCASE ONLY"sv, u"UPASE ONLY"sv, u"UP"sv, },
            { u"UPPERCASE ONLY"sv, u"UPPERCASE OLY"sv, u"UPPERCASE O"sv, u"UPPERCASE O"sv, },
            { u"UPPERCASE ONLY"sv, u"UPPERCASE ONLY"sv, u"UPPERCASE ONLY"sv, u"UPPERCASE ONLY"sv, },
        }, {
            { u"<>UPPERCASE ONLY"sv, u"<>PPERCASE ONLY"sv, u"<>RCASE ONLY"sv, u"<>"sv, },
            { u"UP<>PERCASE ONLY"sv, u"UP<>ERCASE ONLY"sv, u"UP<>ASE ONLY"sv, u"UP<>"sv, },
            { u"UPPERCASE O<>NLY"sv, u"UPPERCASE O<>LY"sv, u"UPPERCASE O<>"sv, u"UPPERCASE O<>"sv, },
            { N, N, N, N, },
        },
        false,
    },
};

struct AtomCaseConversion {
    ExpectedString lowercase;
    ExpectedString uppercase;
};

inline const std::vector<AtomCaseConversion> atomCaseConversion {
    { N, N },
    { u""sv, u""sv },
    { u"hello, world"sv, u"HELLO, WORLD"sv },
    { u"caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv, u"CAF\x00E9 CR\x00E8ME \x00C9T\x00E9"sv },
    { u"\x0421\x0442\x0440\x043E\x043A\x0430 hello \x00E9 world"sv, u"\x0421\x0442\x0440\x043E\x043A\x0430 HELLO \x00E9 WORLD"sv },
    { u"atomic hello world"sv, u"ATOMIC HELLO WORLD"sv },
    { u"\x0130stanbul istanbul istanbul"sv, u"\x0130STANBUL ISTANBUL ISTANBUL"sv },
    { u"  leading and trailing \x0009 whitespace \x000A "sv, u"  LEADING AND TRAILING \x0009 WHITESPACE \x000A "sv },
    { u"a,b,,c,"sv, u"A,B,,C,"sv },
    { u"1.5"sv, u"1.5"sv },
    { u"  -42.25e1junk"sv, u"  -42.25E1JUNK"sv },
    { u"\xD83D\xDE00 emoji \xD83D\xDE00"sv, u"\xD83D\xDE00 EMOJI \xD83D\xDE00"sv },
    { u"lowercase only"sv, u"LOWERCASE ONLY"sv },
    { u"uppercase only"sv, u"UPPERCASE ONLY"sv },
};

inline constexpr char16_t splitCharacters[] { u',', u' ', u'l', u'x' };
inline constexpr std::u16string_view splitStrings[] { u","sv, u", "sv, u"ll"sv, u"nothing"sv };

struct Split {
    std::vector<ExpectedString> byCharacter[4];
    std::vector<ExpectedString> byCharacterAllowingEmptyEntries[4];
    std::vector<ExpectedString> byString[4];
    std::vector<ExpectedString> byStringAllowingEmptyEntries[4];
};

inline const std::vector<Split> split {
    {
        {
            { },
            { },
            { },
            { },
        }, {
            { u""sv },
            { u""sv },
            { u""sv },
            { u""sv },
        }, {
            { },
            { },
            { },
            { },
        }, {
            { N },
            { N },
            { N },
            { N },
        },
    }, {
        {
            { },
            { },
            { },
            { },
        }, {
            { u""sv },
            { u""sv },
            { u""sv },
            { u""sv },
        }, {
            { },
            { },
            { },
            { },
        }, {
            { u""sv },
            { u""sv },
            { u""sv },
            { u""sv },
        },
    }, {
        {
            { u"Hello"sv, u" World"sv },
            { u"Hello,"sv, u"World"sv },
            { u"He"sv, u"o, Wor"sv, u"d"sv },
            { u"Hello, World"sv },
        }, {
            { u"Hello"sv, u" World"sv },
            { u"Hello,"sv, u"World"sv },
            { u"He"sv, u""sv, u"o, Wor"sv, u"d"sv },
            { u"Hello, World"sv },
        }, {
            { u"Hello"sv, u" World"sv },
            { u"Hello"sv, u"World"sv },
            { u"He"sv, u"o, World"sv },
            { u"Hello, World"sv },
        }, {
            { u"Hello"sv, u" World"sv },
            { u"Hello"sv, u"World"sv },
            { u"He"sv, u"o, World"sv },
            { u"Hello, World"sv },
        },
    }, {
        {
            { u"Caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv },
            { u"Caf\x00E9"sv, u"cr\x00E8me"sv, u"\x00C9t\x00E9"sv },
            { u"Caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv },
            { u"Caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv },
        }, {
            { u"Caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv },
            { u"Caf\x00E9"sv, u"cr\x00E8me"sv, u"\x00C9t\x00E9"sv },
            { u"Caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv },
            { u"Caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv },
        }, {
            { u"Caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv },
            { u"Caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv },
            { u"Caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv },
            { u"Caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv },
        }, {
            { u"Caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv },
            { u"Caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv },
            { u"Caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv },
            { u"Caf\x00E9 cr\x00E8me \x00C9t\x00E9"sv },
        },
    }, {
        {
            { u"\x0421\x0442\x0440\x043E\x043A\x0430 Hello \x00E9 World"sv },
            { u"\x0421\x0442\x0440\x043E\x043A\x0430"sv, u"Hello"sv, u"\x00E9"sv, u"World"sv },
            { u"\x0421\x0442\x0440\x043E\x043A\x0430 He"sv, u"o \x00E9 Wor"sv, u"d"sv },
            { u"\x0421\x0442\x0440\x043E\x043A\x0430 Hello \x00E9 World"sv },
        }, {
            { u"\x0421\x0442\x0440\x043E\x043A\x0430 Hello \x00E9 World"sv },
            { u"\x0421\x0442\x0440\x043E\x043A\x0430"sv, u"Hello"sv, u"\x00E9"sv, u"World"sv },
            { u"\x0421\x0442\x0440\x043E\x043A\x0430 He"sv, u""sv, u"o \x00E9 Wor"sv, u"d"sv },
            { u"\x0421\x0442\x0440\x043E\x043A\x0430 Hello \x00E9 World"sv },
        }, {
            { u"\x0421\x0442\x0440\x043E\x043A\x0430 Hello \x00E9 World"sv },
            { u"\x0421\x0442\x0440\x043E\x043A\x0430 Hello \x00E9 World"sv },
            { u"\x0421\x0442\x0440\x043E\x043A\x0430 He"sv, u"o \x00E9 World"sv },
            { u"\x0421\x0442\x0440\x043E\x043A\x0430 Hello \x00E9 World"sv },
        }, {
            { u"\x0421\x0442\x0440\x043E\x043A\x0430 Hello \x00E9 World"sv },
            { u"\x0421\x0442\x0440\x043E\x043A\x0430 Hello \x00E9 World"sv },
            { u"\x0421\x0442\x0440\x043E\x043A\x0430 He"sv, u"o \x00E9 World"sv },
            { u"\x0421\x0442\x0440\x043E\x043A\x0430 Hello \x00E9 World"sv },
        },
    }, {
        {
            { u"Atomic Hello World"sv },
            { u"Atomic"sv, u"Hello"sv, u"World"sv },
            { u"Atomic He"sv, u"o Wor"sv, u"d"sv },
            { u"Atomic Hello World"sv },
        }, {
            { u"Atomic Hello World"sv },
            { u"Atomic"sv, u"Hello"sv, u"World"sv },
            { u"Atomic He"sv, u""sv, u"o Wor"sv, u"d"sv },
            { u"Atomic Hello World"sv },
        }, {
            { u"Atomic Hello World"sv },
            { u"Atomic Hello World"sv },
            { u"Atomic He"sv, u"o World"sv },
            { u"Atomic Hello World"sv },
        }, {
            { u"Atomic Hello World"sv },
            { u"Atomic Hello World"sv },
            { u"Atomic He"sv, u"o World"sv },
            { u"Atomic Hello World"sv },
        },
    }, {
        {
            { u"\x0130stanbul ISTANBUL istanbul"sv },
            { u"\x0130stanbul"sv, u"ISTANBUL"sv, u"istanbul"sv },
            { u"\x0130stanbu"sv, u" ISTANBUL istanbu"sv },
            { u"\x0130stanbul ISTANBUL istanbul"sv },
        }, {
            { u"\x0130stanbul ISTANBUL istanbul"sv },
            { u"\x0130stanbul"sv, u"ISTANBUL"sv, u"istanbul"sv },
            { u"\x0130stanbu"sv, u" ISTANBUL istanbu"sv, u""sv },
            { u"\x0130stanbul ISTANBUL istanbul"sv },
        }, {
            { u"\x0130stanbul ISTANBUL istanbul"sv },
            { u"\x0130stanbul ISTANBUL istanbul"sv },
            { u"\x0130stanbul ISTANBUL istanbul"sv },
            { u"\x0130stanbul ISTANBUL istanbul"sv },
        }, {
            { u"\x0130stanbul ISTANBUL istanbul"sv },
            { u"\x0130stanbul ISTANBUL istanbul"sv },
            { u"\x0130stanbul ISTANBUL istanbul"sv },
            { u"\x0130stanbul ISTANBUL istanbul"sv },
        },
    }, {
        {
            { u"  Leading and trailing \x0009 whitespace \x000A "sv },
            { u"Leading"sv, u"and"sv, u"trailing"sv, u"\x0009"sv, u"whitespace"sv, u"\x000A"sv },
            { u"  Leading and trai"sv, u"ing \x0009 whitespace \x000A "sv },
            { u"  Leading and trailing \x0009 whitespace \x000A "sv },
        }, {
            { u"  Leading and trailing \x0009 whitespace \x000A "sv },
            { u""sv, u""sv, u"Leading"sv, u"and"sv, u"trailing"sv, u"\x0009"sv, u"whitespace"sv, u"\x000A"sv, u""sv },
            { u"  Leading and trai"sv, u"ing \x0009 whitespace \x000A "sv },
            { u"  Leading and trailing \x0009 whitespace \x000A "sv },
        }, {
            { u"  Leading and trailing \x0009 whitespace \x000A "sv },
            { u"  Leading and trailing \x0009 whitespace \x000A "sv },
            { u"  Leading and trailing \x0009 whitespace \x000A "sv },
            { u"  Leading and trailing \x0009 whitespace \x000A "sv },
        }, {
            { u"  Leading and trailing \x0009 whitespace \x000A "sv },
            { u"  Leading and trailing \x0009 whitespace \x000A "sv },
            { u"  Leading and trailing \x0009 whitespace \x000A "sv },
            { u"  Leading and trailing \x0009 whitespace \x000A "sv },
        },
    }, {
        {
            { u"a"sv, u"b"sv, u"c"sv },
            { u"a,b,,c,"sv },
            { u"a,b,,c,"sv },
            { u"a,b,,c,"sv },
        }, {
            { u"a"sv, u"b"sv, u""sv, u"c"sv, u""sv },
            { u"a,b,,c,"sv },
            { u"a,b,,c,"sv },
            { u"a,b,,c,"sv },
        }, {
            { u"a"sv, u"b"sv, u"c"sv },
            { u"a,b,,c,"sv },
            { u"a,b,,c,"sv },
            { u"a,b,,c,"sv },
        }, {
            { u"a"sv, u"b"sv, u""sv, u"c"sv, u""sv },
            { u"a,b,,c,"sv },
            { u"a,b,,c,"sv },
            { u"a,b,,c,"sv },
        },
    }, {
        {
            { u"1.5"sv },
            { u"1.5"sv },
            { u"1.5"sv },
            { u"1.5"sv },
        }, {
            { u"1.5"sv },
            { u"1.5"sv },
            { u"1.5"sv },
            { u"1.5"sv },
        }, {
            { u"1.5"sv },
            { u"1.5"sv },
            { u"1.5"sv },
            { u"1.5"sv },
        }, {
            { u"1.5"sv },
            { u"1.5"sv },
            { u"1.5"sv },
            { u"1.5"sv },
        },
    }, {
        {
            { u"  -42.25e1junk"sv },
            { u"-42.25e1junk"sv },
            { u"  -42.25e1junk"sv },
            { u"  -42.25e1junk"sv },
        }, {
            { u"  -42.25e1junk"sv },
            { u""sv, u""sv, u"-42.25e1junk"sv },
            { u"  -42.25e1junk"sv },
            { u"  -42.25e1junk"sv },
        }, {
            { u"  -42.25e1junk"sv },
            { u"  -42.25e1junk"sv },
            { u"  -42.25e1junk"sv },
            { u"  -42.25e1junk"sv },
        }, {
            { u"  -42.25e1junk"sv },
            { u"  -42.25e1junk"sv },
            { u"  -42.25e1junk"sv },
            { u"  -42.25e1junk"sv },
        },
    }, {
        {
            { u"\xD83D\xDE00 emoji \xD83D\xDE00"sv },
            { u"\xD83D\xDE00"sv, u"emoji"sv, u"\xD83D\xDE00"sv },
            { u"\xD83D\xDE00 emoji \xD83D\xDE00"sv },
            { u"\xD83D\xDE00 emoji \xD83D\xDE00"sv },
        }, {
            { u"\xD83D\xDE00 emoji \xD83D\xDE00"sv },
            { u"\xD83D\xDE00"sv, u"emoji"sv, u"\xD83D\xDE00"sv },
            { u"\xD83D\xDE00 emoji \xD83D\xDE00"sv },
            { u"\xD83D\xDE00 emoji \xD83D\xDE00"sv },
        }, {
            { u"\xD83D\xDE00 emoji \xD83D\xDE00"sv },
            { u"\xD83D\xDE00 emoji \xD83D\xDE00"sv },
            { u"\xD83D\xDE00 emoji \xD83D\xDE00"sv },
            { u"\xD83D\xDE00 emoji \xD83D\xDE00"sv },
        }, {
            { u"\xD83D\xDE00 emoji \xD83D\xDE00"sv },
            { u"\xD83D\xDE00 emoji \xD83D\xDE00"sv },
            { u"\xD83D\xDE00 emoji \xD83D\xDE00"sv },
            { u"\xD83D\xDE00 emoji \xD83D\xDE00"sv },
        },
    }, {
        {
            { u"lowercase only"sv },
            { u"lowercase"sv, u"only"sv },
            { u"owercase on"sv, u"y"sv },
            { u"lowercase only"sv },
        }, {
            { u"lowercase only"sv },
            { u"lowercase"sv, u"only"sv },
            { u""sv, u"owercase on"sv, u"y"sv },
            { u"lowercase only"sv },
        }, {
            { u"lowercase only"sv },
            { u"lowercase only"sv },
            { u"lowercase only"sv },
            { u"lowercase only"sv },
        }, {
            { u"lowercase only"sv },
            { u"lowercase only"sv },
            { u"lowercase only"sv },
            { u"lowercase only"sv },
        },
    }, {
        {
            { u"UPPERCASE ONLY"sv },
            { u"UPPERCASE"sv, u"ONLY"sv },
            { u"UPPERCASE ONLY"sv },
            { u"UPPERCASE ONLY"sv },
        }, {
            { u"UPPERCASE ONLY"sv },
            { u"UPPERCASE"sv, u"ONLY"sv },
            { u"UPPERCASE ONLY"sv },
            { u"UPPERCASE ONLY"sv },
        }, {
            { u"UPPERCASE ONLY"sv },
            { u"UPPERCASE ONLY"sv },
            { u"UPPERCASE ONLY"sv },
            { u"UPPERCASE ONLY"sv },
        }, {
            { u"UPPERCASE ONLY"sv },
            { u"UPPERCASE ONLY"sv },
            { u"UPPERCASE ONLY"sv },
            { u"UPPERCASE ONLY"sv },
        },
    },
};

inline const std::vector<ExpectedString> numberParsingExtraInputs { u"3.14"sv, u"  2.5  "sv, u"1e400"sv, u"nan"sv, u"-0"sv, u"12abc"sv };

struct NumberParsing {
    double toDouble;
    bool toDoubleOK;
    float toFloat;
    bool toFloatOK;
};

inline const std::vector<NumberParsing> numberParsing {
    { 0x0p+0, false, static_cast<float>(0x0p+0), false },
    { 0x0p+0, false, static_cast<float>(0x0p+0), false },
    { 0x0p+0, false, static_cast<float>(0x0p+0), false },
    { 0x0p+0, false, static_cast<float>(0x0p+0), false },
    { 0x0p+0, false, static_cast<float>(0x0p+0), false },
    { 0x0p+0, false, static_cast<float>(0x0p+0), false },
    { 0x0p+0, false, static_cast<float>(0x0p+0), false },
    { 0x0p+0, false, static_cast<float>(0x0p+0), false },
    { 0x0p+0, false, static_cast<float>(0x0p+0), false },
    { 0x1.8p+0, true, static_cast<float>(0x1.8p+0), true },
    { -0x1.a68p+8, false, static_cast<float>(-0x1.a68p+8), false },
    { 0x0p+0, false, static_cast<float>(0x0p+0), false },
    { 0x0p+0, false, static_cast<float>(0x0p+0), false },
    { 0x0p+0, false, static_cast<float>(0x0p+0), false },
    { 0x1.91eb851eb851fp+1, true, static_cast<float>(0x1.91eb86p+1), true },
    { 0x1.4p+1, false, static_cast<float>(0x1.4p+1), false },
    { std::numeric_limits<double>::infinity(), true, static_cast<float>(std::numeric_limits<double>::infinity()), true },
    { 0x0p+0, false, static_cast<float>(0x0p+0), false },
    { -0x0p+0, true, static_cast<float>(-0x0p+0), true },
    { 0x1.8p+3, false, static_cast<float>(0x1.8p+3), false },
};

inline const std::vector<bool> isSafeToSendToAnotherThread { true, true, false, false, false, false, false, false, false, false, false, false, false, false, };
inline constexpr bool isolatedCopyOfEmptyStringIsSame = true;

// Row a, column b; '1' is true. b is an isolated copy, so only isSameImpl depends on identity.
struct Equality {
    std::string_view equal;
    std::string_view viewsEqual;
    std::string_view equalIgnoringNullity;
    std::string_view equalIgnoringASCIICase;
    std::string_view viewsEqualIgnoringASCIICase;
    bool equalsHelloWorld;
    bool equalsEmptyLiteral;
    bool equalsHelloWorldIgnoringASCIICase;
    bool equalsLowercaseOnlyLetters;
    bool startsWithHelloLetters;
    bool equalsOwnSpan;
};

inline const std::vector<Equality> equality {
    { "10000000000000"sv, "11000000000000"sv, "11000000000000"sv, "10000000000000"sv, "11000000000000"sv, false, false, false, false, false, true },
    { "01000000000000"sv, "11000000000000"sv, "11000000000000"sv, "01000000000000"sv, "11000000000000"sv, false, true, false, false, false, true },
    { "00100000000000"sv, "00100000000000"sv, "00100000000000"sv, "00100000000000"sv, "00100000000000"sv, true, false, true, false, true, true },
    { "00010000000000"sv, "00010000000000"sv, "00010000000000"sv, "00010000000000"sv, "00010000000000"sv, false, false, false, false, false, true },
    { "00001000000000"sv, "00001000000000"sv, "00001000000000"sv, "00001000000000"sv, "00001000000000"sv, false, false, false, false, false, true },
    { "00000100000000"sv, "00000100000000"sv, "00000100000000"sv, "00000100000000"sv, "00000100000000"sv, false, false, false, false, false, true },
    { "00000010000000"sv, "00000010000000"sv, "00000010000000"sv, "00000010000000"sv, "00000010000000"sv, false, false, false, false, false, true },
    { "00000001000000"sv, "00000001000000"sv, "00000001000000"sv, "00000001000000"sv, "00000001000000"sv, false, false, false, false, false, true },
    { "00000000100000"sv, "00000000100000"sv, "00000000100000"sv, "00000000100000"sv, "00000000100000"sv, false, false, false, false, false, true },
    { "00000000010000"sv, "00000000010000"sv, "00000000010000"sv, "00000000010000"sv, "00000000010000"sv, false, false, false, false, false, true },
    { "00000000001000"sv, "00000000001000"sv, "00000000001000"sv, "00000000001000"sv, "00000000001000"sv, false, false, false, false, false, true },
    { "00000000000100"sv, "00000000000100"sv, "00000000000100"sv, "00000000000100"sv, "00000000000100"sv, false, false, false, false, false, true },
    { "00000000000010"sv, "00000000000010"sv, "00000000000010"sv, "00000000000010"sv, "00000000000010"sv, false, false, false, true, false, true },
    { "00000000000001"sv, "00000000000001"sv, "00000000000001"sv, "00000000000001"sv, "00000000000001"sv, false, false, false, false, false, true },
};

inline const std::vector<unsigned> computedHashes { 82610334U, 82610334U, 723739326U, 4242217545U, 3747914500U, 3495704424U, 2645115550U, 218286130U, 369036763U, 3023838179U, 2193473126U, 872147972U, 1573813558U, 3270581170U, };

} // namespace TestWebKitAPI::StringFunctionsExpectations
