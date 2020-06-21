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

#include <wtf/HashSet.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

typedef std::pair<UChar32, UChar32> UnicodeRange;
typedef Vector<UnicodeRange> UnicodeRanges;

namespace WebCore {

class FloatPoint;
class FloatRect;

enum class SuffixSkippingPolicy {
    DontSkip,
    Skip
};

Optional<float> parseNumber(const LChar*& current, const LChar* end, SuffixSkippingPolicy = SuffixSkippingPolicy::Skip);
Optional<float> parseNumber(const UChar*& current, const UChar* end, SuffixSkippingPolicy = SuffixSkippingPolicy::Skip);
Optional<float> parseNumber(const StringView&, SuffixSkippingPolicy = SuffixSkippingPolicy::Skip);

Optional<std::pair<float, float>> parseNumberOptionalNumber(const StringView&);

Optional<bool> parseArcFlag(const LChar*& current, const LChar* end);
Optional<bool> parseArcFlag(const UChar*& current, const UChar* end);

Optional<FloatPoint> parsePoint(const StringView&);
Optional<FloatRect> parseRect(const StringView&);

Optional<FloatPoint> parseFloatPoint(const LChar*& current, const LChar* end);
Optional<FloatPoint> parseFloatPoint(const UChar*& current, const UChar* end);

Optional<std::pair<UnicodeRanges, HashSet<String>>> parseKerningUnicodeString(const StringView&);
Optional<HashSet<String>> parseGlyphName(const StringView&);


// SVG allows several different whitespace characters:
// http://www.w3.org/TR/SVG/paths.html#PathDataBNF
template<typename CharacterType> constexpr bool isSVGSpace(CharacterType c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

template<typename CharacterType> constexpr bool skipOptionalSVGSpaces(const CharacterType*& ptr, const CharacterType* end)
{
    while (ptr < end && isSVGSpace(*ptr))
        ptr++;
    return ptr < end;
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

constexpr bool skipString(const UChar*& ptr, const UChar* end, const UChar* name, int length)
{
    if (end - ptr < length)
        return false;
    if (memcmp(name, ptr, sizeof(UChar) * length))
        return false;
    ptr += length;
    return true;
}

template<unsigned characterCount>
inline bool skipString(const UChar*& ptr, const UChar* end, const char (&str)[characterCount])
{
    int length = characterCount - 1;
    if (end - ptr < length)
        return false;
    for (int i = 0; i < length; ++i) {
        if (ptr[i] != str[i])
            return false;
    }
    ptr += length;
    return true;
}

} // namespace WebCore
