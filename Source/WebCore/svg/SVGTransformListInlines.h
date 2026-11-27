/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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

#include "SVGTransformList.h"

namespace WebCore {

template<typename CharacterType> static int parseTransformParamList(StringParsingBuffer<CharacterType>& buffer, std::span<float, 6> values, int required, int optional)
{
    int optionalParams = 0, requiredParams = 0;

    if (!skipOptionalSVGSpaces(buffer) || *buffer != '(')
        return -1;

    ++buffer;

    skipOptionalSVGSpaces(buffer);

    while (requiredParams < required) {
        if (buffer.atEnd())
            return -1;
        auto parsedNumber = parseNumber(buffer, SuffixSkippingPolicy::DontSkip);
        if (!parsedNumber)
            return -1;
        values[requiredParams] = *parsedNumber;
        requiredParams++;
        if (requiredParams < required)
            skipOptionalSVGSpacesOrDelimiter(buffer);
    }
    if (!skipOptionalSVGSpaces(buffer))
        return -1;

    bool delimParsed = skipOptionalSVGSpacesOrDelimiter(buffer);

    if (buffer.atEnd())
        return -1;

    if (*buffer == ')') {
        // skip optionals
        ++buffer;
        if (delimParsed)
            return -1;
    } else {
        while (optionalParams < optional) {
            if (buffer.atEnd())
                return -1;
            auto parsedNumber = parseNumber(buffer, SuffixSkippingPolicy::DontSkip);
            if (!parsedNumber)
                return -1;
            values[requiredParams + optionalParams] = *parsedNumber;
            optionalParams++;
            if (optionalParams < optional)
                skipOptionalSVGSpacesOrDelimiter(buffer);
        }

        if (!skipOptionalSVGSpaces(buffer))
            return -1;

        delimParsed = skipOptionalSVGSpacesOrDelimiter(buffer);

        if (buffer.atEnd() || *buffer != ')' || delimParsed)
            return -1;
        ++buffer;
    }

    return requiredParams + optionalParams;
}

// These should be kept in sync with enum SVGTransformType
static constexpr std::array requiredValuesForType { 0, 6, 1, 1, 1, 1, 1 };
static constexpr std::array optionalValuesForType { 0, 0, 1, 1, 2, 0, 0 };

template<typename CharacterType>
std::optional<SVGTransformList::ParsedTransform> SVGTransformList::parseTransform(SVGTransformValue::SVGTransformType type, StringParsingBuffer<CharacterType>& buffer)
{
    if (type == SVGTransformValue::SVG_TRANSFORM_UNKNOWN)
        return std::nullopt;

    ParsedTransform transform { type, { 0, 0, 0, 0, 0, 0 }, 0 };
    transform.valueCount = parseTransformParamList(buffer, transform.values, requiredValuesForType[type], optionalValuesForType[type]);
    if (transform.valueCount < 0)
        return std::nullopt;

    return transform;
}

}
