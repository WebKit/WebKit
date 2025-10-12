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

#include "SVGTransformable.h"

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
bool SVGTransformable::parseAndReplaceTransform(SVGTransformValue::SVGTransformType type, StringParsingBuffer<CharacterType>& buffer, SVGTransform& transform)
{
    ASSERT(type == transform.value().type());

    if (type == SVGTransformValue::SVG_TRANSFORM_UNKNOWN)
        return false;

    std::array<float, 6> values { 0, 0, 0, 0, 0, 0 };
    int valueCount = parseTransformParamList(buffer, values, requiredValuesForType[type], optionalValuesForType[type]);
    if (valueCount < 0)
        return false;

    switch (type) {
    case SVGTransformValue::SVG_TRANSFORM_UNKNOWN:
        ASSERT_NOT_REACHED();
        return false;

    case SVGTransformValue::SVG_TRANSFORM_SKEWX:
        transform.value().setSkewX(values[0]);
        return true;

    case SVGTransformValue::SVG_TRANSFORM_SKEWY:
        transform.value().setSkewY(values[0]);
        return true;

    case SVGTransformValue::SVG_TRANSFORM_SCALE:
        if (valueCount == 1)
            transform.value().setScale(values[0], values[0]);
        else
            transform.value().setScale(values[0], values[1]);
        return true;

    case SVGTransformValue::SVG_TRANSFORM_TRANSLATE:
        if (valueCount == 1)
            transform.value().setTranslate(values[0], 0);
        else
            transform.value().setTranslate(values[0], values[1]);
        return true;

    case SVGTransformValue::SVG_TRANSFORM_ROTATE:
        if (valueCount == 1)
            transform.value().setRotate(values[0], 0, 0);
        else
            transform.value().setRotate(values[0], values[1], values[2]);
        return true;

    case SVGTransformValue::SVG_TRANSFORM_MATRIX:
        transform.value().setMatrix(AffineTransform(values[0], values[1], values[2], values[3], values[4], values[5]));
        return true;
    }

    return false;
}

template<typename CharacterType>
RefPtr<SVGTransform> SVGTransformable::parseTransform(SVGTransformValue::SVGTransformType type, StringParsingBuffer<CharacterType>& buffer)
{
    if (type == SVGTransformValue::SVG_TRANSFORM_UNKNOWN)
        return nullptr;

    std::array<float, 6> values { 0, 0, 0, 0, 0, 0 };
    int valueCount = parseTransformParamList(buffer, values, requiredValuesForType[type], optionalValuesForType[type]);
    if (valueCount < 0)
        return nullptr;

    switch (type) {
    case SVGTransformValue::SVG_TRANSFORM_UNKNOWN:
        ASSERT_NOT_REACHED();
        return nullptr;

    case SVGTransformValue::SVG_TRANSFORM_SKEWX: {
        SVGTransformValue transform;
        transform.setSkewX(values[0]);
        return SVGTransform::create(WTFMove(transform));
    }
    case SVGTransformValue::SVG_TRANSFORM_SKEWY: {
        SVGTransformValue transform;
        transform.setSkewY(values[0]);
        return SVGTransform::create(WTFMove(transform));
    }
    case SVGTransformValue::SVG_TRANSFORM_SCALE: {
        auto resultValue = [&]() {
            if (valueCount == 1) // Spec: if only one param given, assume uniform scaling
                return SVGTransformValue::scaleTransformValue({ values[0], values[0] });

            return SVGTransformValue::scaleTransformValue({ values[0], values[1] });
        };

        return SVGTransform::create(resultValue());
    }
    case SVGTransformValue::SVG_TRANSFORM_TRANSLATE: {
        if (valueCount == 1) // Spec: if only one param given, assume 2nd param to be 0
            return SVGTransform::create(SVGTransformValue::translateTransformValue({ values[0], 0 }));

        return SVGTransform::create(SVGTransformValue::translateTransformValue({ values[0], values[1] }));
    }
    case SVGTransformValue::SVG_TRANSFORM_ROTATE: {
        auto resultValue = [&]() {
            if (valueCount == 1)
                return SVGTransformValue::rotateTransformValue(values[0], { });

            return SVGTransformValue::rotateTransformValue(values[0], { values[1], values[2] });
        };
        return SVGTransform::create(resultValue());
    }
    case SVGTransformValue::SVG_TRANSFORM_MATRIX: {
        SVGTransformValue transform;
        transform.setMatrix(AffineTransform(values[0], values[1], values[2], values[3], values[4], values[5]));
        return SVGTransform::create(transform);
    }
    }

    return nullptr;
}

}
