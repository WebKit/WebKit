/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#if ENABLE(SVG)
#include "SVGAnimatedType.h"

#include "SVGParserUtilities.h"
#include "SVGPathByteStream.h"

namespace WebCore {

SVGAnimatedType::SVGAnimatedType(AnimatedPropertyType type)
    : m_type(type)
{
}

SVGAnimatedType::~SVGAnimatedType()
{
    switch (m_type) {
    case AnimatedAngle:
        delete m_data.angleAndEnumeration;
        break;
    case AnimatedBoolean:
        delete m_data.boolean;
        break;
    case AnimatedColor:
        delete m_data.color;
        break;
    case AnimatedEnumeration:
        delete m_data.enumeration;
        break;
    case AnimatedInteger:
        delete m_data.integer;
        break;
    case AnimatedIntegerOptionalInteger:
        delete m_data.integerOptionalInteger;
        break;
    case AnimatedLength:
        delete m_data.length;
        break;
    case AnimatedLengthList:
        delete m_data.lengthList;
        break;
    case AnimatedNumber:
        delete m_data.number;
        break;
    case AnimatedNumberList:
        delete m_data.numberList;
        break;
    case AnimatedNumberOptionalNumber:
        delete m_data.numberOptionalNumber;
        break;
    case AnimatedPath:
        delete m_data.path;
        break;
    case AnimatedPoints:
        delete m_data.pointList;
        break;
    case AnimatedPreserveAspectRatio:
        delete m_data.preserveAspectRatio;
        break;
    case AnimatedRect:
        delete m_data.rect;
        break;
    case AnimatedString:
        delete m_data.string;
        break;
    case AnimatedTransformList:
        delete m_data.transformList;
        break;
    case AnimatedUnknown:
        ASSERT_NOT_REACHED();
        break;
    }
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedType::createAngleAndEnumeration(std::pair<SVGAngle, unsigned>* angleAndEnumeration)
{
    ASSERT(angleAndEnumeration);
    auto animatedType = std::make_unique<SVGAnimatedType>(AnimatedAngle);
    animatedType->m_data.angleAndEnumeration = angleAndEnumeration;
    return animatedType;
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedType::createBoolean(bool* boolean)
{
    ASSERT(boolean);
    auto animatedType = std::make_unique<SVGAnimatedType>(AnimatedBoolean);
    animatedType->m_data.boolean = boolean;
    return animatedType;
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedType::createColor(Color* color)
{
    ASSERT(color);
    auto animatedType = std::make_unique<SVGAnimatedType>(AnimatedColor);
    animatedType->m_data.color = color;
    return animatedType;
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedType::createEnumeration(unsigned* enumeration)
{
    ASSERT(enumeration);
    auto animatedType = std::make_unique<SVGAnimatedType>(AnimatedEnumeration);
    animatedType->m_data.enumeration = enumeration;
    return animatedType;
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedType::createInteger(int* integer)
{
    ASSERT(integer);
    auto animatedType = std::make_unique<SVGAnimatedType>(AnimatedInteger);
    animatedType->m_data.integer = integer;
    return animatedType;
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedType::createIntegerOptionalInteger(pair<int, int>* integerOptionalInteger)
{
    ASSERT(integerOptionalInteger);
    auto animatedType = std::make_unique<SVGAnimatedType>(AnimatedIntegerOptionalInteger);
    animatedType->m_data.integerOptionalInteger = integerOptionalInteger;
    return animatedType;
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedType::createLength(SVGLength* length)
{
    ASSERT(length);
    auto animatedType = std::make_unique<SVGAnimatedType>(AnimatedLength);
    animatedType->m_data.length = length;
    return animatedType;
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedType::createLengthList(SVGLengthList* lengthList)
{
    ASSERT(lengthList);
    auto animatedType = std::make_unique<SVGAnimatedType>(AnimatedLengthList);
    animatedType->m_data.lengthList = lengthList;
    return animatedType;
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedType::createNumber(float* number)
{
    ASSERT(number);
    auto animatedType = std::make_unique<SVGAnimatedType>(AnimatedNumber);
    animatedType->m_data.number = number;
    return animatedType;
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedType::createNumberList(SVGNumberList* numberList)
{
    ASSERT(numberList);
    auto animatedType = std::make_unique<SVGAnimatedType>(AnimatedNumberList);
    animatedType->m_data.numberList = numberList;
    return animatedType;
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedType::createNumberOptionalNumber(pair<float, float>* numberOptionalNumber)
{
    ASSERT(numberOptionalNumber);
    auto animatedType = std::make_unique<SVGAnimatedType>(AnimatedNumberOptionalNumber);
    animatedType->m_data.numberOptionalNumber = numberOptionalNumber;
    return animatedType;
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedType::createPath(std::unique_ptr<SVGPathByteStream> path)
{
    ASSERT(path);
    auto animatedType = std::make_unique<SVGAnimatedType>(AnimatedPath);
    animatedType->m_data.path = path.release();
    return animatedType;
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedType::createPointList(SVGPointList* pointList)
{
    ASSERT(pointList);
    auto animatedType = std::make_unique<SVGAnimatedType>(AnimatedPoints);
    animatedType->m_data.pointList = pointList;
    return animatedType;
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedType::createPreserveAspectRatio(SVGPreserveAspectRatio* preserveAspectRatio)
{
    ASSERT(preserveAspectRatio);
    auto animatedType = std::make_unique<SVGAnimatedType>(AnimatedPreserveAspectRatio);
    animatedType->m_data.preserveAspectRatio = preserveAspectRatio;
    return animatedType;
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedType::createRect(FloatRect* rect)
{
    ASSERT(rect);
    auto animatedType = std::make_unique<SVGAnimatedType>(AnimatedRect);
    animatedType->m_data.rect = rect;
    return animatedType;
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedType::createString(String* string)
{
    ASSERT(string);
    auto animatedType = std::make_unique<SVGAnimatedType>(AnimatedString);
    animatedType->m_data.string = string;
    return animatedType;
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedType::createTransformList(SVGTransformList* transformList)
{
    ASSERT(transformList);
    auto animatedType = std::make_unique<SVGAnimatedType>(AnimatedTransformList);
    animatedType->m_data.transformList = transformList;
    return animatedType;
}

String SVGAnimatedType::valueAsString()
{
    switch (m_type) {
    case AnimatedColor:
        ASSERT(m_data.color);
        return m_data.color->serialized();
    case AnimatedLength:
        ASSERT(m_data.length);
        return m_data.length->valueAsString();
    case AnimatedLengthList:
        ASSERT(m_data.lengthList);
        return m_data.lengthList->valueAsString();
    case AnimatedNumber:
        ASSERT(m_data.number);
        return String::number(*m_data.number);
    case AnimatedRect:
        ASSERT(m_data.rect);
        return String::number(m_data.rect->x()) + ' ' + String::number(m_data.rect->y()) + ' '
             + String::number(m_data.rect->width()) + ' ' + String::number(m_data.rect->height());
    case AnimatedString:
        ASSERT(m_data.string);
        return *m_data.string;

    // These types don't appear in the table in SVGElement::cssPropertyToTypeMap() and thus don't need valueAsString() support.
    case AnimatedAngle:
    case AnimatedBoolean:
    case AnimatedEnumeration:
    case AnimatedInteger:
    case AnimatedIntegerOptionalInteger:
    case AnimatedNumberList:
    case AnimatedNumberOptionalNumber:
    case AnimatedPath:
    case AnimatedPoints:
    case AnimatedPreserveAspectRatio:
    case AnimatedTransformList:
    case AnimatedUnknown:
        // Only SVG DOM animations use these property types - that means valueAsString() is never used for those.
        ASSERT_NOT_REACHED();
        break;
    }
    ASSERT_NOT_REACHED();
    return String();
}

bool SVGAnimatedType::setValueAsString(const QualifiedName& attrName, const String& value)
{
    switch (m_type) {
    case AnimatedColor:
        ASSERT(m_data.color);
        *m_data.color = value.isEmpty() ? Color() : SVGColor::colorFromRGBColorString(value);
        break;
    case AnimatedLength: {
        ASSERT(m_data.length);
        ExceptionCode ec = 0;
        m_data.length->setValueAsString(value, SVGLength::lengthModeForAnimatedLengthAttribute(attrName), ec);
        return !ec;
    }
    case AnimatedLengthList:
        ASSERT(m_data.lengthList);
        m_data.lengthList->parse(value, SVGLength::lengthModeForAnimatedLengthAttribute(attrName));
        break;
    case AnimatedNumber:
        ASSERT(m_data.number);
        parseNumberFromString(value, *m_data.number);
        break;
    case AnimatedRect:
        ASSERT(m_data.rect);
        parseRect(value, *m_data.rect);
        break;
    case AnimatedString:
        ASSERT(m_data.string);
        *m_data.string = value;
        break;

    // These types don't appear in the table in SVGElement::cssPropertyToTypeMap() and thus don't need setValueAsString() support. 
    case AnimatedAngle:
    case AnimatedBoolean:
    case AnimatedEnumeration:
    case AnimatedInteger:
    case AnimatedIntegerOptionalInteger:
    case AnimatedNumberList:
    case AnimatedNumberOptionalNumber:
    case AnimatedPath:
    case AnimatedPoints:
    case AnimatedPreserveAspectRatio:
    case AnimatedTransformList:
    case AnimatedUnknown:
        // Only SVG DOM animations use these property types - that means setValueAsString() is never used for those.
        ASSERT_NOT_REACHED();
        break;
    }
    return true;
}

bool SVGAnimatedType::supportsAnimVal(AnimatedPropertyType type)
{
    // AnimatedColor is only used for CSS property animations.
    return type != AnimatedUnknown && type != AnimatedColor;
}

} // namespace WebCore

#endif // ENABLE(SVG)
