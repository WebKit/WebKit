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

#include "config.h"
#include "SpatialMediaFragment.h"

#include "NaturalDimensions.h"
#include "ObjectSizeNegotiation.h"
#include <array>
#include <wtf/ASCIICType.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/text/ParsingUtilities.h>
#include <wtf/text/StringParsingBuffer.h>
#include <wtf/text/StringView.h>

namespace WebCore {

template<typename CharacterType> static constexpr std::array<CharacterType, 6> pixelUnit { 'p', 'i', 'x', 'e', 'l', ':' };
template<typename CharacterType> static constexpr std::array<CharacterType, 8> percentUnit { 'p', 'e', 'r', 'c', 'e', 'n', 't', ':' };

// 1*DIGIT: no sign, no whitespace, no decimal point.
template<typename CharacterType> static std::optional<unsigned> parseSpatialMediaFragmentNumber(StringParsingBuffer<CharacterType>& buffer)
{
    if (buffer.atEnd() || !isASCIIDigit(*buffer))
        return std::nullopt;

    Checked<unsigned, RecordOverflow> value = 0;
    do {
        value *= 10u;
        value += static_cast<unsigned>(*buffer - '0');
        ++buffer;
    } while (buffer.hasCharactersRemaining() && isASCIIDigit(*buffer));

    if (value.hasOverflowed())
        return std::nullopt;
    return value.value();
}

// xywhparam = [ xywhunit ":" ] 1*DIGIT "," 1*DIGIT "," 1*DIGIT "," 1*DIGIT
// The unit is case-sensitive.
template<typename CharacterType> static std::optional<SpatialMediaFragment> parseSpatialMediaFragment(StringParsingBuffer<CharacterType> buffer)
{
    SpatialMediaFragment fragment;
    if (skipCharactersExactly(buffer, std::span { percentUnit<CharacterType> }))
        fragment.unit = SpatialMediaFragment::Unit::Percent;
    else
        skipCharactersExactly(buffer, std::span { pixelUnit<CharacterType> });

    std::array<unsigned, 4> numbers;
    for (size_t i = 0; i < numbers.size(); ++i) {
        if (i && !skipExactly(buffer, ','))
            return std::nullopt;
        auto number = parseSpatialMediaFragmentNumber(buffer);
        if (!number)
            return std::nullopt;
        numbers[i] = *number;
    }
    if (buffer.hasCharactersRemaining())
        return std::nullopt;

    auto [x, y, width, height] = numbers;
    if (!width || !height)
        return std::nullopt;

    // https://www.w3.org/TR/media-frags/#error-uri-spatial
    if (fragment.unit == SpatialMediaFragment::Unit::Percent && (width > 100 || height > 100 || x > 100 - width || y > 100 - height))
        return std::nullopt;

    fragment.rect = { static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height) };
    return fragment;
}

std::optional<SpatialMediaFragment> SpatialMediaFragment::parse(StringView value)
{
    return readCharactersForParsing(value, [](auto buffer) {
        return parseSpatialMediaFragment(buffer);
    });
}

std::optional<FloatRect> SpatialMediaFragment::viewBox(const NaturalDimensions& naturalDimensions) const
{
    auto result = rect;
    if (unit == Unit::Percent) {
        // Percentages resolve against the concrete object size, with the CSS default object size
        // of 300x150 when the document has no natural size. https://github.com/w3c/svgwg/issues/1113
        auto size = ObjectSizeNegotiation::defaultSizingAlgorithm(naturalDimensions, ObjectSizeNegotiation::SpecifiedSize::none(), ObjectSizeNegotiation::defaultObjectSize).size();
        result.scale(size.width() / 100, size.height() / 100);
    } else {
        // A rectangle that goes past the document is clipped to it, and one that starts outside it is ignored.
        // This needs a natural size: without one, the pixels are used as they are.
        // https://www.w3.org/TR/media-frags/#error-media-spatial
        auto size = ObjectSizeNegotiation::defaultSizingAlgorithm(naturalDimensions, ObjectSizeNegotiation::SpecifiedSize::none(), { }).size();
        if (!size.isEmpty())
            result.intersect({ { }, size });
    }

    if (result.isEmpty())
        return std::nullopt;
    return result;
}

// MediaFragment ::= ( timesegment | spacesegment ) *( "&" ( timesegment | spacesegment ) )
std::optional<FloatRect> viewBoxForSpatialMediaFragment(StringView fragmentIdentifier, const NaturalDimensions& naturalDimensions)
{
    std::optional<SpatialMediaFragment> spatialFragment;
    for (auto segment : fragmentIdentifier.split('&')) {
        // A segment that is not a name-value pair is a BareName, which cannot be combined with a space segment.
        // https://github.com/w3c/svgwg/issues/1112
        auto separator = segment.find('=');
        if (separator == notFound)
            return std::nullopt;

        // The last valid xywh is used. Other names, and invalid xywh values, are ignored.
        // https://www.w3.org/TR/media-frags/#processing-name-value-lists
        // FIXME: Apply time segments (t=) too, through setCurrentTime().
        if (segment.left(separator) != "xywh"_s)
            continue;
        if (auto parsedFragment = SpatialMediaFragment::parse(segment.substring(separator + 1)))
            spatialFragment = parsedFragment;
    }

    if (!spatialFragment)
        return std::nullopt;
    return spatialFragment->viewBox(naturalDimensions);
}

} // namespace WebCore
