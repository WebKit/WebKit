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
#include <wtf/dtoa.h>
#include <wtf/text/StringParsingBuffer.h>
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
template <typename CharacterType> static std::optional<float> genericParseNumber(StringParsingBuffer<CharacterType>& buffer, SuffixSkippingPolicy skip = SuffixSkippingPolicy::Skip)
{
    size_t parsedLength = 0;
    float number = parseFloat(buffer.position(), buffer.lengthRemaining(), parsedLength);

    // Don't return Infinity() or NaN().
    if (!isValidRange(number))
        return std::nullopt;

    if (!parsedLength)
        return std::nullopt;

    // If the number ends with '.', we treat it as an error.
    if (buffer[parsedLength - 1] == '.')
        return std::nullopt;

    buffer.advanceBy(parsedLength);
    if (skip == SuffixSkippingPolicy::Skip)
        skipOptionalSVGSpacesOrDelimiter(buffer);

    return number;
}

std::optional<float> parseNumber(StringParsingBuffer<LChar>& buffer, SuffixSkippingPolicy skip)
{
    return genericParseNumber(buffer, skip);
}

std::optional<float> parseNumber(StringParsingBuffer<UChar>& buffer, SuffixSkippingPolicy skip)
{
    return genericParseNumber(buffer, skip);
}

std::optional<float> parseNumber(StringView string, SuffixSkippingPolicy skip)
{
    return readCharactersForParsing(string, [skip](auto buffer) -> std::optional<float> {
        auto result = genericParseNumber(buffer, skip);
        if (!buffer.atEnd())
            return std::nullopt;
        return result;
    });
}

// only used to parse largeArcFlag and sweepFlag which must be a "0" or "1"
// and might not have any whitespace/comma after it
template <typename CharacterType> std::optional<bool> genericParseArcFlag(StringParsingBuffer<CharacterType>& buffer)
{
    if (buffer.atEnd())
        return std::nullopt;

    const CharacterType flagChar = *buffer;
    ++buffer;

    bool flag;
    if (flagChar == '0')
        flag = false;
    else if (flagChar == '1')
        flag = true;
    else
        return std::nullopt;

    skipOptionalSVGSpacesOrDelimiter(buffer);
    
    return flag;
}

std::optional<bool> parseArcFlag(StringParsingBuffer<LChar>& buffer)
{
    return genericParseArcFlag(buffer);
}

std::optional<bool> parseArcFlag(StringParsingBuffer<UChar>& buffer)
{
    return genericParseArcFlag(buffer);
}

std::optional<std::pair<float, float>> parseNumberOptionalNumber(StringView string)
{
    if (string.isEmpty())
        return std::nullopt;

    return readCharactersForParsing(string, [](auto buffer) -> std::optional<std::pair<float, float>> {
        auto x = parseNumber(buffer);
        if (!x)
            return std::nullopt;

        if (buffer.atEnd())
            return std::make_pair(*x, *x);

        auto y = parseNumber(buffer, SuffixSkippingPolicy::DontSkip);
        if (!y)
            return std::nullopt;

        if (!buffer.atEnd())
            return std::nullopt;

        return std::make_pair(*x, *y);
    });
}

std::optional<FloatPoint> parsePoint(StringView string)
{
    if (string.isEmpty())
        return std::nullopt;

    return readCharactersForParsing(string, [](auto buffer) -> std::optional<FloatPoint> {
        if (!skipOptionalSVGSpaces(buffer))
            return std::nullopt;

        auto point = parseFloatPoint(buffer);
        if (!point)
            return std::nullopt;

        // Disallow anything except spaces at the end.
        skipOptionalSVGSpaces(buffer);
        
        return point;
    });
}

std::optional<FloatRect> parseRect(StringView string)
{
    return readCharactersForParsing(string, [](auto buffer) -> std::optional<FloatRect> {
        skipOptionalSVGSpaces(buffer);
        
        auto x = parseNumber(buffer);
        if (!x)
            return std::nullopt;
        auto y = parseNumber(buffer);
        if (!y)
            return std::nullopt;
        auto width = parseNumber(buffer);
        if (!width)
            return std::nullopt;
        auto height = parseNumber(buffer, SuffixSkippingPolicy::DontSkip);
        if (!height)
            return std::nullopt;

        return FloatRect { *x, *y, *width, *height };
    });
}

std::optional<HashSet<String>> parseGlyphName(StringView string)
{
    // FIXME: Parsing error detection is missing.

    return readCharactersForParsing(string, [](auto buffer) -> HashSet<String> {
        skipOptionalSVGSpaces(buffer);

        HashSet<String> values;

        while (buffer.hasCharactersRemaining()) {
            // Leading and trailing white space, and white space before and after separators, will be ignored.
            auto inputStart = buffer.position();

            skipUntil(buffer, ',');

            if (buffer.position() == inputStart)
                break;

            // walk backwards from the ; to ignore any whitespace
            auto inputEnd = buffer.position() - 1;
            while (inputStart < inputEnd && isSVGSpace(*inputEnd))
                --inputEnd;

            values.add(String(inputStart, inputEnd - inputStart + 1));
            skipOptionalSVGSpacesOrDelimiter(buffer, ',');
        }
        return values;
    });

}

template<typename CharacterType> static std::optional<UnicodeRange> parseUnicodeRange(StringParsingBuffer<CharacterType> buffer)
{
    unsigned length = buffer.lengthRemaining();
    if (length < 2 || buffer[0] != 'U' || buffer[1] != '+')
        return std::nullopt;

    buffer += 2;

    // Parse the starting hex number (or its prefix).
    unsigned startRange = 0;
    unsigned startLength = 0;

    while (buffer.hasCharactersRemaining()) {
        if (!isASCIIHexDigit(*buffer))
            break;
        ++startLength;
        if (startLength > 6)
            return std::nullopt;
        startRange = (startRange << 4) | toASCIIHexValue(*buffer);
        ++buffer;
    }

    // Handle the case of ranges separated by "-" sign.
    if (2 + startLength < length && *buffer == '-') {
        if (!startLength)
            return std::nullopt;
        
        // Parse the ending hex number (or its prefix).
        unsigned endRange = 0;
        unsigned endLength = 0;
        ++buffer;
        while (buffer.hasCharactersRemaining()) {
            if (!isASCIIHexDigit(*buffer))
                break;
            ++endLength;
            if (endLength > 6)
                return std::nullopt;
            endRange = (endRange << 4) | toASCIIHexValue(*buffer);
            ++buffer;
        }
        
        if (!endLength)
            return std::nullopt;
        
        UnicodeRange range;
        range.first = startRange;
        range.second = endRange;
        return range;
    }
    
    // Handle the case of a number with some optional trailing question marks.
    unsigned endRange = startRange;
    while (buffer.hasCharactersRemaining()) {
        if (*buffer != '?')
            break;
        ++startLength;
        if (startLength > 6)
            return std::nullopt;
        startRange <<= 4;
        endRange = (endRange << 4) | 0xF;
        ++buffer;
    }
    
    if (!startLength)
        return std::nullopt;
    
    UnicodeRange range;
    range.first = startRange;
    range.second = endRange;
    return range;
}

std::optional<std::pair<UnicodeRanges, HashSet<String>>> parseKerningUnicodeString(StringView string)
{
    // FIXME: Parsing error detection is missing.

    return readCharactersForParsing(string, [](auto buffer) -> std::pair<UnicodeRanges, HashSet<String>> {
        UnicodeRanges rangeList;
        HashSet<String> stringList;

        while (1) {
            auto inputStart = buffer.position();

            skipUntil(buffer, ',');

            if (buffer.position() == inputStart)
                break;

            // Try to parse unicode range first
            if (auto range = parseUnicodeRange(StringParsingBuffer { inputStart, buffer.position() }))
                rangeList.append(WTFMove(*range));
            else
                stringList.add(String(inputStart, buffer.position() - inputStart));

            if (buffer.atEnd())
                break;

            ++buffer;
        }

        return std::make_pair(WTFMove(rangeList), WTFMove(stringList));
    });
}

template <typename CharacterType> static std::optional<FloatPoint> genericParseFloatPoint(StringParsingBuffer<CharacterType>& buffer)
{
    auto x = parseNumber(buffer);
    if (!x)
        return std::nullopt;

    auto y = parseNumber(buffer);
    if (!y)
        return std::nullopt;

    return FloatPoint { *x, *y };
}

std::optional<FloatPoint> parseFloatPoint(StringParsingBuffer<LChar>& buffer)
{
    return genericParseFloatPoint(buffer);
}

std::optional<FloatPoint> parseFloatPoint(StringParsingBuffer<UChar>& buffer)
{
    return genericParseFloatPoint(buffer);
}

}
