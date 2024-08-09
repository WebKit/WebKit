/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include <WebCore/FontCascade.h>

namespace TestWebKitAPI {

using namespace WebCore;
using CodePath = FontCascade::CodePath;

static void testCodePath(UChar codePoint, CodePath codePath)
{
    std::array<UChar, 1> target = { codePoint };
    EXPECT_EQ(codePath, FontCascade::characterRangeCodePath(std::span<UChar>(target))) << "target: " << static_cast<int>(target[0]);
}

struct CodePathRange {
    UChar start;
    UChar end;
    CodePath path;
    CodePath belowPath { CodePath::Simple };
    CodePath abovePath { CodePath::Simple };
};

static void testCodePathRange(CodePathRange range)
{
    std::array<UChar, 1> below = { static_cast<UChar>(range.start - 1) };
    std::array<UChar, 1> start = { range.start };
    std::array<UChar, 1> middle = { static_cast<UChar>((range.start + range.end) / 2) };
    std::array<UChar, 1> end = { range.end };
    std::array<UChar, 1> above = { static_cast<UChar>(range.end + 1) };

    EXPECT_EQ(range.belowPath, FontCascade::characterRangeCodePath(std::span<UChar>(below))) << "below: " << std::hex << static_cast<int>(below[0]);
    EXPECT_EQ(range.path, FontCascade::characterRangeCodePath(std::span<UChar>(start))) << "start: " << std::hex << static_cast<int>(start[0]);
    EXPECT_EQ(range.path, FontCascade::characterRangeCodePath(std::span<UChar>(middle))) << "middle: " << std::hex << static_cast<int>(middle[0]);
    EXPECT_EQ(range.path, FontCascade::characterRangeCodePath(std::span<UChar>(end))) << "end: " << std::hex << static_cast<int>(end[0]);
    EXPECT_EQ(range.abovePath, FontCascade::characterRangeCodePath(std::span<UChar>(above))) << "above: " << std::hex << static_cast<int>(above[0]);
}

// Testing characterRangeCodePath for non-surrogate codepoints
TEST(FontCascadeTest, characterRangeCodePath_NonSurrogates)
{
    // U+02E5 through U+02E9 (Modifier Letters : Tone letters)
    testCodePathRange({ 0x02E5, 0x02E9, CodePath::Complex });
    // U+0300 through U+036F Combining diacritical marks
    testCodePathRange({ 0x0300, 0x036F, CodePath::Complex });
    // U+0591 through U+05CF excluding U+05BE Hebrew combining marks, Hebrew punctuation Paseq, Sof Pasuq and Nun Hafukha
    testCodePathRange({ 0x0591, 0x05BD, CodePath::Complex });
    testCodePath(0x5BE, CodePath::Simple);
    testCodePathRange({ 0x05BF, 0x05CF, CodePath::Complex });
    // U+0600 through U+109F Arabic, Syriac, Thaana, NKo, Samaritan, Mandaic,
    // Devanagari, Bengali, Gurmukhi, Gujarati, Oriya, Tamil, Telugu, Kannada,
    // Malayalam, Sinhala, Thai, Lao, Tibetan, Myanmar
    testCodePathRange({ 0x0600, 0x109F, CodePath::Complex });
    // U+1100 through U+11FF Hangul Jamo (only Ancient Korean should be left here if you precompose;
    // Modern Korean will be precomposed as a result of step A)
    testCodePathRange({ 0x1100, 0x11FF, CodePath::Complex });
    // U+135D through U+135F Ethiopic combining marks
    testCodePathRange({ 0x135D, 0x135F, CodePath::Complex });
    // U+1700 through U+18AF Tagalog, Hanunoo, Buhid, Taghanwa,Khmer, Mongolian
    testCodePathRange({ 0x1700, 0x18AF, CodePath::Complex });
    // U+1900 through U+194F Limbu (Unicode 4.0)
    testCodePathRange({ 0x1900, 0x194F, CodePath::Complex });
    // U+1980 through U+19DF New Tai Lue
    testCodePathRange({ 0x1980, 0x19DF, CodePath::Complex });
    // U+1A00 through U+1CFF Buginese, Tai Tham, Balinese, Batak, Lepcha, Vedic
    testCodePathRange({ 0x1A00, 0x1CFF, CodePath::Complex });
    // U+1DC0 through U+1DFF Comining diacritical mark supplement
    testCodePathRange({ 0x1DC0, 0x1DFF, CodePath::Complex, CodePath::Simple, CodePath::SimpleWithGlyphOverflow });
    // U+1E00 through U+2000 characters with diacritics and stacked diacritics
    testCodePathRange({ 0x1E00, 0x2000, CodePath::SimpleWithGlyphOverflow, CodePath::Complex, CodePath::Simple });
    // U+20D0 through U+20FF Combining marks for symbols
    testCodePathRange({ 0x20D0, 0x20FF, CodePath::Complex });
    testCodePath(0x26F9, CodePath::Complex);
    // U+2CEF through U+2CF1 Combining marks for Coptic
    testCodePathRange({ 0x2CEF, 0x2CF1, CodePath::Complex });
    // U+302A through U+302F Ideographic and Hangul Tone marks
    testCodePathRange({ 0x302A, 0x302F, CodePath::Complex });
    // KATAKANA-HIRAGANA (SEMI-)VOICED SOUND MARKS require character composition
    testCodePathRange({ 0x3099, 0x309C, CodePath::Complex });
    // U+A67C through U+A67D Combining marks for old Cyrillic
    testCodePathRange({ 0xA67C, 0xA67D, CodePath::Complex });
    // U+A6F0 through U+A6F1 Combining mark for Bamum
    testCodePathRange({ 0xA6F0, 0xA6F1, CodePath::Complex });
    // U+A800 through U+ABFF Nagri, Phags-pa, Saurashtra, Devanagari Extended,
    // Hangul Jamo Ext. A, Javanese, Myanmar Extended A, Tai Viet, Meetei Mayek,
    testCodePathRange({ 0xA800, 0xABFF, CodePath::Complex });
    // U+D7B0 through U+D7FF Hangul Jamo Ext. B
    testCodePathRange({ 0xD7B0, 0xD7FF, CodePath::Complex });
    // U+FE00 through U+FE0F Unicode variation selectors
    testCodePathRange({ 0xFE00, 0xFE0F, CodePath::Complex });
    // U+FE20 through U+FE2F Combining half marks
    testCodePathRange({ 0xFE20, 0xFE2F, CodePath::Complex });
}
}
