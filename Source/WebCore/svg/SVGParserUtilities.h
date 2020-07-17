/*
 * Copyright (C) 2002, 2003 The Karbon Developers
 * Copyright (C) 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#pragma once

#include "ParsingUtilities.h"
#include <wtf/Forward.h>

typedef std::pair<UChar32, UChar32> UnicodeRange;
typedef Vector<UnicodeRange> UnicodeRanges;

namespace WebCore {

class FloatPoint;
class FloatRect;

enum class SuffixSkippingPolicy {
    DontSkip,
    Skip
};

Optional<float> parseNumber(StringParsingBuffer<LChar>&, SuffixSkippingPolicy = SuffixSkippingPolicy::Skip);
Optional<float> parseNumber(StringParsingBuffer<UChar>&, SuffixSkippingPolicy = SuffixSkippingPolicy::Skip);
Optional<float> parseNumber(const StringView&, SuffixSkippingPolicy = SuffixSkippingPolicy::Skip);

Optional<std::pair<float, float>> parseNumberOptionalNumber(const StringView&);

Optional<bool> parseArcFlag(StringParsingBuffer<LChar>&);
Optional<bool> parseArcFlag(StringParsingBuffer<UChar>&);

Optional<FloatPoint> parsePoint(const StringView&);
Optional<FloatRect> parseRect(const StringView&);

Optional<FloatPoint> parseFloatPoint(StringParsingBuffer<LChar>&);
Optional<FloatPoint> parseFloatPoint(StringParsingBuffer<UChar>&);

Optional<std::pair<UnicodeRanges, HashSet<String>>> parseKerningUnicodeString(const StringView&);
Optional<HashSet<String>> parseGlyphName(const StringView&);


// SVG allows several different whitespace characters:
// http://www.w3.org/TR/SVG/paths.html#PathDataBNF
template<typename CharacterType> constexpr bool isSVGSpace(CharacterType c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

template<typename CharacterType> constexpr bool isSVGSpaceOrComma(CharacterType c)
{
    return isSVGSpace(c) || c == ',';
}

template<typename CharacterType> constexpr bool skipOptionalSVGSpaces(const CharacterType*& ptr, const CharacterType* end)
{
    skipWhile<CharacterType, isSVGSpace>(ptr, end);
    return ptr < end;
}

template<typename CharacterType> constexpr bool skipOptionalSVGSpaces(StringParsingBuffer<CharacterType>& characters)
{
    skipWhile<CharacterType, isSVGSpace>(characters);
    return characters.hasCharactersRemaining();
}

template<typename CharacterType> constexpr bool skipOptionalSVGSpacesOrDelimiter(const CharacterType*& ptr, const CharacterType* end, char delimiter = ',')
{
    if (ptr < end && !isSVGSpace(*ptr) && *ptr != delimiter)
        return false;
    if (skipOptionalSVGSpaces(ptr, end)) {
        if (ptr < end && *ptr == delimiter) {
            ptr++;
            skipOptionalSVGSpaces(ptr, end);
        }
    }
    return ptr < end;
}

template<typename CharacterType> constexpr bool skipOptionalSVGSpacesOrDelimiter(StringParsingBuffer<CharacterType>& characters, char delimiter = ',')
{
    if (characters.hasCharactersRemaining() && !isSVGSpace(*characters) && *characters != delimiter)
        return false;
    if (skipOptionalSVGSpaces(characters)) {
        if (characters.hasCharactersRemaining() && *characters == delimiter) {
            characters++;
            skipOptionalSVGSpaces(characters);
        }
    }
    return characters.hasCharactersRemaining();
}

} // namespace WebCore
