/*
 * Copyright (C) 2002, 2003 The Karbon Developers
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007-2018 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "SVGParserUtilities.h"

#include "Document.h"
#include "FloatRect.h"
#include <limits>
#include <wtf/ASCIICType.h>
#include <wtf/text/StringView.h>

namespace WebCore {

template <typename FloatType> static inline bool isValidRange(const FloatType& x)
{
    static const FloatType max = std::numeric_limits<FloatType>::max();
    return x >= -max && x <= max;
}

// We use this generic parseNumber function to allow the Path parsing code to work 
// at a higher precision internally, without any unnecessary runtime cost or code
// complexity.
// FIXME: Can this be shared/replaced with number parsing in WTF?
template <typename CharacterType, typename FloatType = float> static Optional<FloatType> genericParseNumber(const CharacterType*& ptr, const CharacterType* end, SuffixSkippingPolicy skip = SuffixSkippingPolicy::Skip)
{
    FloatType number = 0;
    FloatType integer = 0;
    FloatType decimal = 0;
    FloatType frac = 1;
    FloatType exponent = 0;
    int sign = 1;
    int expsign = 1;
    const CharacterType* start = ptr;

    // read the sign
    if (ptr < end && *ptr == '+')
        ptr++;
    else if (ptr < end && *ptr == '-') {
        ptr++;
        sign = -1;
    } 
    
    if (ptr == end || (!isASCIIDigit(*ptr) && *ptr != '.'))
        return WTF::nullopt;

    // read the integer part, build right-to-left
    const CharacterType* ptrStartIntPart = ptr;
    while (ptr < end && isASCIIDigit(*ptr))
        ++ptr; // Advance to first non-digit.

    if (ptr != ptrStartIntPart) {
        const CharacterType* ptrScanIntPart = ptr - 1;
        FloatType multiplier = 1;
        while (ptrScanIntPart >= ptrStartIntPart) {
            integer += multiplier * static_cast<FloatType>(*(ptrScanIntPart--) - '0');
            multiplier *= 10;
        }
        // Bail out early if this overflows.
        if (!isValidRange(integer))
            return WTF::nullopt;
    }

    if (ptr < end && *ptr == '.') { // read the decimals
        ptr++;
        
        // There must be a least one digit following the .
        if (ptr >= end || !isASCIIDigit(*ptr))
            return WTF::nullopt;
        
        while (ptr < end && isASCIIDigit(*ptr))
            decimal += (*(ptr++) - '0') * (frac *= static_cast<FloatType>(0.1));
    }

    // read the exponent part
    if (ptr != start && ptr + 1 < end && (*ptr == 'e' || *ptr == 'E') 
        && (ptr[1] != 'x' && ptr[1] != 'm')) { 
        ptr++;

        // read the sign of the exponent
        if (*ptr == '+')
            ptr++;
        else if (*ptr == '-') {
            ptr++;
            expsign = -1;
        }
        
        // There must be an exponent
        if (ptr >= end || !isASCIIDigit(*ptr))
            return WTF::nullopt;

        while (ptr < end && isASCIIDigit(*ptr)) {
            exponent *= static_cast<FloatType>(10);
            exponent += *ptr - '0';
            ptr++;
        }
        // Make sure exponent is valid.
        if (!isValidRange(exponent) || exponent > std::numeric_limits<FloatType>::max_exponent)
            return WTF::nullopt;
    }

    number = integer + decimal;
    number *= sign;

    if (exponent)
        number *= static_cast<FloatType>(pow(10.0, expsign * static_cast<int>(exponent)));

    // Don't return Infinity() or NaN().
    if (!isValidRange(number))
        return WTF::nullopt;

    if (start == ptr)
        return WTF::nullopt;

    if (skip == SuffixSkippingPolicy::Skip)
        skipOptionalSVGSpacesOrDelimiter(ptr, end);

    return number;
}

Optional<float> parseNumber(const LChar*& ptr, const LChar* end, SuffixSkippingPolicy skip)
{
    return genericParseNumber(ptr, end, skip);
}

Optional<float> parseNumber(const UChar*& ptr, const UChar* end, SuffixSkippingPolicy skip)
{
    return genericParseNumber(ptr, end, skip);
}

Optional<float> parseNumber(const StringView& string, SuffixSkippingPolicy skip)
{
    auto upconvertedCharacters = string.upconvertedCharacters();
    const UChar* ptr = upconvertedCharacters;
    const UChar* end = ptr + string.length();
    auto result = genericParseNumber(ptr, end, skip);
    if (ptr != end)
        return WTF::nullopt;
    return result;
}

// only used to parse largeArcFlag and sweepFlag which must be a "0" or "1"
// and might not have any whitespace/comma after it
template <typename CharacterType> Optional<bool> genericParseArcFlag(const CharacterType*& ptr, const CharacterType* end)
{
    if (ptr >= end)
        return WTF::nullopt;

    const CharacterType flagChar = *ptr++;

    bool flag;
    if (flagChar == '0')
        flag = false;
    else if (flagChar == '1')
        flag = true;
    else
        return WTF::nullopt;

    skipOptionalSVGSpacesOrDelimiter(ptr, end);
    
    return flag;
}

Optional<bool> parseArcFlag(const LChar*& ptr, const LChar* end)
{
    return genericParseArcFlag(ptr, end);
}

Optional<bool> parseArcFlag(const UChar*& ptr, const UChar* end)
{
    return genericParseArcFlag(ptr, end);
}

Optional<std::pair<float, float>> parseNumberOptionalNumber(const StringView& string)
{
    if (string.isEmpty())
        return WTF::nullopt;

    auto upconvertedCharacters = string.upconvertedCharacters();
    const UChar* cur = upconvertedCharacters;
    const UChar* end = cur + string.length();

    auto x = parseNumber(cur, end);
    if (!x)
        return WTF::nullopt;

    if (cur == end)
        return std::make_pair(*x, *x);

    auto y = parseNumber(cur, end, SuffixSkippingPolicy::DontSkip);
    if (!y)
        return WTF::nullopt;

    if (cur != end)
        return WTF::nullopt;

    return std::make_pair(*x, *y);
}

Optional<FloatPoint> parsePoint(const StringView& string)
{
    if (string.isEmpty())
        return WTF::nullopt;

    auto upconvertedCharacters = string.upconvertedCharacters();
    const UChar* cur = upconvertedCharacters;
    const UChar* end = cur + string.length();

    if (!skipOptionalSVGSpaces(cur, end))
        return WTF::nullopt;

    auto point = parseFloatPoint(cur, end);
    if (!point)
        return WTF::nullopt;

    // Disallow anything except spaces at the end.
    skipOptionalSVGSpaces(cur, end);
    
    return point;
}

Optional<FloatRect> parseRect(const StringView& string)
{
    auto upconvertedCharacters = string.upconvertedCharacters();
    const UChar* ptr = upconvertedCharacters;
    const UChar* end = ptr + string.length();
    
    skipOptionalSVGSpaces(ptr, end);
    
    auto x = parseNumber(ptr, end);
    if (!x)
        return WTF::nullopt;
    auto y = parseNumber(ptr, end);
    if (!y)
        return WTF::nullopt;
    auto width = parseNumber(ptr, end);
    if (!width)
        return WTF::nullopt;
    auto height = parseNumber(ptr, end, SuffixSkippingPolicy::DontSkip);
    if (!height)
        return WTF::nullopt;

    return FloatRect { *x, *y, *width, *height };
}

Optional<HashSet<String>> parseGlyphName(const StringView& string)
{
    // FIXME: Parsing error detection is missing.

    auto upconvertedCharacters = string.upconvertedCharacters();
    const UChar* ptr = upconvertedCharacters;
    const UChar* end = ptr + string.length();
    skipOptionalSVGSpaces(ptr, end);

    HashSet<String> values;

    while (ptr < end) {
        // Leading and trailing white space, and white space before and after separators, will be ignored.
        const UChar* inputStart = ptr;
        while (ptr < end && *ptr != ',')
            ++ptr;

        if (ptr == inputStart)
            break;

        // walk backwards from the ; to ignore any whitespace
        const UChar* inputEnd = ptr - 1;
        while (inputStart < inputEnd && isSVGSpace(*inputEnd))
            --inputEnd;

        values.add(String(inputStart, inputEnd - inputStart + 1));
        skipOptionalSVGSpacesOrDelimiter(ptr, end, ',');
    }

    return values;
}

static Optional<UnicodeRange> parseUnicodeRange(const UChar* characters, unsigned length)
{
    if (length < 2 || characters[0] != 'U' || characters[1] != '+')
        return WTF::nullopt;
    
    // Parse the starting hex number (or its prefix).
    unsigned startRange = 0;
    unsigned startLength = 0;

    const UChar* ptr = characters + 2;
    const UChar* end = characters + length;
    while (ptr < end) {
        if (!isASCIIHexDigit(*ptr))
            break;
        ++startLength;
        if (startLength > 6)
            return WTF::nullopt;
        startRange = (startRange << 4) | toASCIIHexValue(*ptr);
        ++ptr;
    }

    // Handle the case of ranges separated by "-" sign.
    if (2 + startLength < length && *ptr == '-') {
        if (!startLength)
            return WTF::nullopt;
        
        // Parse the ending hex number (or its prefix).
        unsigned endRange = 0;
        unsigned endLength = 0;
        ++ptr;
        while (ptr < end) {
            if (!isASCIIHexDigit(*ptr))
                break;
            ++endLength;
            if (endLength > 6)
                return WTF::nullopt;
            endRange = (endRange << 4) | toASCIIHexValue(*ptr);
            ++ptr;
        }
        
        if (!endLength)
            return WTF::nullopt;
        
        UnicodeRange range;
        range.first = startRange;
        range.second = endRange;
        return range;
    }
    
    // Handle the case of a number with some optional trailing question marks.
    unsigned endRange = startRange;
    while (ptr < end) {
        if (*ptr != '?')
            break;
        ++startLength;
        if (startLength > 6)
            return WTF::nullopt;
        startRange <<= 4;
        endRange = (endRange << 4) | 0xF;
        ++ptr;
    }
    
    if (!startLength)
        return WTF::nullopt;
    
    UnicodeRange range;
    range.first = startRange;
    range.second = endRange;
    return range;
}

Optional<std::pair<UnicodeRanges, HashSet<String>>> parseKerningUnicodeString(const StringView& string)
{
    // FIXME: Parsing error detection is missing.

    auto upconvertedCharacters = string.upconvertedCharacters();
    const UChar* ptr = upconvertedCharacters;
    const UChar* end = ptr + string.length();

    UnicodeRanges rangeList;
    HashSet<String> stringList;

    while (ptr < end) {
        const UChar* inputStart = ptr;
        while (ptr < end && *ptr != ',')
            ++ptr;

        if (ptr == inputStart)
            break;

        // Try to parse unicode range first
        if (auto range = parseUnicodeRange(inputStart, ptr - inputStart))
            rangeList.append(WTFMove(*range));
        else
            stringList.add(String(inputStart, ptr - inputStart));
        ++ptr;
    }

    return std::make_pair(rangeList, stringList);
}

template <typename CharacterType> static Optional<FloatPoint> genericParseFloatPoint(const CharacterType*& current, const CharacterType* end)
{
    auto x = parseNumber(current, end);
    if (!x)
        return WTF::nullopt;

    auto y = parseNumber(current, end);
    if (!y)
        return WTF::nullopt;

    return FloatPoint { *x, *y };
}

Optional<FloatPoint> parseFloatPoint(const LChar*& current, const LChar* end)
{
    return genericParseFloatPoint(current, end);
}

Optional<FloatPoint> parseFloatPoint(const UChar*& current, const UChar* end)
{
    return genericParseFloatPoint(current, end);
}

}
