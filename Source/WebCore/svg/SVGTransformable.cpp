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

#include "config.h"
#include "SVGTransformable.h"

#include "AffineTransform.h"
#include "FloatConversion.h"
#include "SVGElement.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include <wtf/text/StringParsingBuffer.h>
#include <wtf/text/StringView.h>

namespace WebCore {

SVGTransformable::~SVGTransformable() = default;

template<typename CharacterType> static int parseTransformParamList(StringParsingBuffer<CharacterType>& buffer, float* values, int required, int optional)
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
static constexpr int requiredValuesForType[] =  { 0, 6, 1, 1, 1, 1, 1 };
static constexpr int optionalValuesForType[] =  { 0, 0, 1, 1, 2, 0, 0 };

template<typename CharacterType> static Optional<SVGTransformValue> parseTransformValueGeneric(SVGTransformValue::SVGTransformType type, StringParsingBuffer<CharacterType>& buffer)
{
    if (type == SVGTransformValue::SVG_TRANSFORM_UNKNOWN)
        return WTF::nullopt;

    int valueCount = 0;
    float values[] = {0, 0, 0, 0, 0, 0};
    if ((valueCount = parseTransformParamList(buffer, values, requiredValuesForType[type], optionalValuesForType[type])) < 0)
        return WTF::nullopt;

    SVGTransformValue transform;
    switch (type) {
    case SVGTransformValue::SVG_TRANSFORM_UNKNOWN:
        ASSERT_NOT_REACHED();
        break;
    case SVGTransformValue::SVG_TRANSFORM_SKEWX:
        transform.setSkewX(values[0]);
        break;
    case SVGTransformValue::SVG_TRANSFORM_SKEWY:
        transform.setSkewY(values[0]);
        break;
    case SVGTransformValue::SVG_TRANSFORM_SCALE:
        if (valueCount == 1) // Spec: if only one param given, assume uniform scaling
            transform.setScale(values[0], values[0]);
        else
            transform.setScale(values[0], values[1]);
        break;
    case SVGTransformValue::SVG_TRANSFORM_TRANSLATE:
        if (valueCount == 1) // Spec: if only one param given, assume 2nd param to be 0
            transform.setTranslate(values[0], 0);
        else
            transform.setTranslate(values[0], values[1]);
        break;
    case SVGTransformValue::SVG_TRANSFORM_ROTATE:
        if (valueCount == 1)
            transform.setRotate(values[0], 0, 0);
        else
            transform.setRotate(values[0], values[1], values[2]);
        break;
    case SVGTransformValue::SVG_TRANSFORM_MATRIX:
        transform.setMatrix(AffineTransform(values[0], values[1], values[2], values[3], values[4], values[5]));
        break;
    }

    return transform;
}

Optional<SVGTransformValue> SVGTransformable::parseTransformValue(SVGTransformValue::SVGTransformType type, StringParsingBuffer<LChar>& buffer)
{
    return parseTransformValueGeneric(type, buffer);
}

Optional<SVGTransformValue> SVGTransformable::parseTransformValue(SVGTransformValue::SVGTransformType type, StringParsingBuffer<UChar>& buffer)
{
    return parseTransformValueGeneric(type, buffer);
}

template<typename CharacterType> static constexpr CharacterType skewXDesc[] =  {'s', 'k', 'e', 'w', 'X'};
template<typename CharacterType> static constexpr CharacterType skewYDesc[] =  {'s', 'k', 'e', 'w', 'Y'};
template<typename CharacterType> static constexpr CharacterType scaleDesc[] =  {'s', 'c', 'a', 'l', 'e'};
template<typename CharacterType> static constexpr CharacterType translateDesc[] =  {'t', 'r', 'a', 'n', 's', 'l', 'a', 't', 'e'};
template<typename CharacterType> static constexpr CharacterType rotateDesc[] =  {'r', 'o', 't', 'a', 't', 'e'};
template<typename CharacterType> static constexpr CharacterType matrixDesc[] =  {'m', 'a', 't', 'r', 'i', 'x'};

template<typename CharacterType> static Optional<SVGTransformValue::SVGTransformType> parseTransformTypeGeneric(StringParsingBuffer<CharacterType>& buffer)
{
    if (buffer.atEnd())
        return WTF::nullopt;

    if (*buffer == 's') {
        if (skipCharactersExactly(buffer, skewXDesc<CharacterType>))
            return SVGTransformValue::SVG_TRANSFORM_SKEWX;
        if (skipCharactersExactly(buffer, skewYDesc<CharacterType>))
            return SVGTransformValue::SVG_TRANSFORM_SKEWY;
        if (skipCharactersExactly(buffer, scaleDesc<CharacterType>))
            return SVGTransformValue::SVG_TRANSFORM_SCALE;
        return WTF::nullopt;
    }

    if (skipCharactersExactly(buffer, translateDesc<CharacterType>))
        return SVGTransformValue::SVG_TRANSFORM_TRANSLATE;
    if (skipCharactersExactly(buffer, rotateDesc<CharacterType>))
        return SVGTransformValue::SVG_TRANSFORM_ROTATE;
    if (skipCharactersExactly(buffer, matrixDesc<CharacterType>))
        return SVGTransformValue::SVG_TRANSFORM_MATRIX;

    return WTF::nullopt;
}

Optional<SVGTransformValue::SVGTransformType> SVGTransformable::parseTransformType(StringView string)
{
    return readCharactersForParsing(string, [](auto buffer) {
        return parseTransformType(buffer);
    });
}

Optional<SVGTransformValue::SVGTransformType> SVGTransformable::parseTransformType(StringParsingBuffer<LChar>& buffer)
{
    return parseTransformTypeGeneric(buffer);
}

Optional<SVGTransformValue::SVGTransformType> SVGTransformable::parseTransformType(StringParsingBuffer<UChar>& buffer)
{
    return parseTransformTypeGeneric(buffer);
}

}
