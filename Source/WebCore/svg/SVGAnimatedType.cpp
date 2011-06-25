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

#if ENABLE(SVG) && ENABLE(SVG_ANIMATION)
#include "SVGAnimatedType.h"

#include "FloatRect.h"
#include "SVGAngle.h"
#include "SVGColor.h"
#include "SVGLength.h"
#include "SVGNumberList.h"
#include "SVGParserUtilities.h"
#include "SVGPathParserFactory.h"
#include "SVGPointList.h"

namespace WebCore {

SVGAnimatedType::SVGAnimatedType(AnimatedAttributeType type)
    : m_type(type)
{
}

SVGAnimatedType::~SVGAnimatedType()
{
    switch (m_type) {
    case AnimatedAngle:
        delete m_data.angle;
        break;
    case AnimatedColor:
        delete m_data.color;
        break;
    case AnimatedLength:
        delete m_data.length;
        break;
    case AnimatedNumber:
        delete m_data.number;
        break;
    case AnimatedNumberList:
        delete m_data.numberList;
        break;
    case AnimatedPath:
        delete m_data.path;
        break;
    case AnimatedPoints:
        delete m_data.pointList;
        break;
    case AnimatedRect:
        delete m_data.rect;
        break;
    case AnimatedString:
        delete m_data.string;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedType::createAngle(SVGAngle* angle)
{
    ASSERT(angle);
    OwnPtr<SVGAnimatedType> animatedType = adoptPtr(new SVGAnimatedType(AnimatedAngle));
    animatedType->m_data.angle = angle;
    return animatedType.release();
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedType::createColor(Color* color)
{
    ASSERT(color);
    OwnPtr<SVGAnimatedType> animatedType = adoptPtr(new SVGAnimatedType(AnimatedColor));
    animatedType->m_data.color = color;
    return animatedType.release();
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedType::createLength(SVGLength* length)
{
    ASSERT(length);
    OwnPtr<SVGAnimatedType> animatedType = adoptPtr(new SVGAnimatedType(AnimatedLength));
    animatedType->m_data.length = length;
    return animatedType.release();
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedType::createNumber(float* number)
{
    ASSERT(number);
    OwnPtr<SVGAnimatedType> animatedType = adoptPtr(new SVGAnimatedType(AnimatedNumber));
    animatedType->m_data.number = number;
    return animatedType.release();
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedType::createNumberList(SVGNumberList* numberList)
{
    ASSERT(numberList);
    OwnPtr<SVGAnimatedType> animatedType = adoptPtr(new SVGAnimatedType(AnimatedNumberList));
    animatedType->m_data.numberList = numberList;
    return animatedType.release();
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedType::createPath(PassOwnPtr<SVGPathByteStream> path)
{
    ASSERT(path);
    OwnPtr<SVGAnimatedType> animatedType = adoptPtr(new SVGAnimatedType(AnimatedPath));
    animatedType->m_data.path = path.leakPtr();
    return animatedType.release();
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedType::createPointList(SVGPointList* pointList)
{
    ASSERT(pointList);
    OwnPtr<SVGAnimatedType> animatedType = adoptPtr(new SVGAnimatedType(AnimatedPoints));
    animatedType->m_data.pointList = pointList;
    return animatedType.release();
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedType::createRect(FloatRect* rect)
{
    ASSERT(rect);
    OwnPtr<SVGAnimatedType> animatedType = adoptPtr(new SVGAnimatedType(AnimatedRect));
    animatedType->m_data.rect = rect;
    return animatedType.release();
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedType::createString(String* string)
{
    ASSERT(string);
    OwnPtr<SVGAnimatedType> animatedType = adoptPtr(new SVGAnimatedType(AnimatedString));
    animatedType->m_data.string = string;
    return animatedType.release();
}

SVGAngle& SVGAnimatedType::angle()
{
    ASSERT(m_type == AnimatedAngle);
    return *m_data.angle;
}

Color& SVGAnimatedType::color()
{
    ASSERT(m_type == AnimatedColor);
    return *m_data.color;
}

SVGLength& SVGAnimatedType::length()
{
    ASSERT(m_type == AnimatedLength);
    return *m_data.length;
}

float& SVGAnimatedType::number()
{
    ASSERT(m_type == AnimatedNumber);
    return *m_data.number;
}

SVGNumberList& SVGAnimatedType::numberList()
{
    ASSERT(m_type == AnimatedNumberList);
    return *m_data.numberList;
}

SVGPathByteStream* SVGAnimatedType::path()
{
    ASSERT(m_type == AnimatedPath);
    return m_data.path;
}

SVGPointList& SVGAnimatedType::pointList()
{
    ASSERT(m_type == AnimatedPoints);
    return *m_data.pointList;
}

FloatRect& SVGAnimatedType::rect()
{
    ASSERT(m_type == AnimatedRect);
    return *m_data.rect;
}

String& SVGAnimatedType::string()
{
    ASSERT(m_type == AnimatedString);
    return *m_data.string;
}

String SVGAnimatedType::valueAsString()
{
    switch (m_type) {
    case AnimatedAngle:
        ASSERT(m_data.angle);
        return m_data.angle->valueAsString();
    case AnimatedColor:
        ASSERT(m_data.color);
        return m_data.color->serialized();
    case AnimatedLength:
        ASSERT(m_data.length);
        return m_data.length->valueAsString();
    case AnimatedNumber:
        ASSERT(m_data.number);
        return String::number(*m_data.number);
    case AnimatedNumberList:
        ASSERT(m_data.numberList);
        return m_data.numberList->valueAsString();
    case AnimatedPath: {
        ASSERT(m_data.path);
        String result;
        SVGPathParserFactory::self()->buildStringFromByteStream(m_data.path, result, UnalteredParsing);
        return result;
    }
    case AnimatedPoints:
        ASSERT(m_data.pointList);
        return m_data.pointList->valueAsString();
    case AnimatedRect:
        ASSERT(m_data.rect);
        return String::number(m_data.rect->x()) + ' ' + String::number(m_data.rect->y()) + ' '
             + String::number(m_data.rect->width()) + ' ' + String::number(m_data.rect->height());
    case AnimatedString:
        ASSERT(m_data.string);
        return *m_data.string;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return String();
}

bool SVGAnimatedType::setValueAsString(const QualifiedName& attrName, const String& value)
{
    ExceptionCode ec = 0;
    switch (m_type) {
    case AnimatedAngle:
        ASSERT(m_data.angle);
        m_data.angle->setValueAsString(value, ec);
        break;
    case AnimatedColor:
        ASSERT(m_data.color);
        *m_data.color = value.isEmpty() ? Color() : SVGColor::colorFromRGBColorString(value);
        break;
    case AnimatedLength:
        ASSERT(m_data.length);
        m_data.length->setValueAsString(value, SVGLength::lengthModeForAnimatedLengthAttribute(attrName), ec);
        break;
    case AnimatedNumber:
        ASSERT(m_data.number);
        parseNumberFromString(value, *m_data.number);
        break;
    case AnimatedNumberList:
        ASSERT(m_data.numberList);
        m_data.numberList->parse(value);
        break;
    case AnimatedPath: {
        ASSERT(m_data.path);
        OwnPtr<SVGPathByteStream> pathByteStream = adoptPtr(m_data.path);
        if (!SVGPathParserFactory::self()->buildSVGPathByteStreamFromString(value, pathByteStream, UnalteredParsing))
            ec = 1; // Arbitary value > 0, it doesn't matter as we don't report the exception code.
        m_data.path = pathByteStream.leakPtr();
        break;
    }
    case AnimatedPoints:
        ASSERT(m_data.pointList);
        m_data.pointList->clear();
        pointsListFromSVGData(*m_data.pointList, value);
        break;
    case AnimatedRect:
        ASSERT(m_data.rect);
        parseRect(value, *m_data.rect);
        break;
    case AnimatedString:
        ASSERT(m_data.string);
        *m_data.string = value;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return !ec;
}

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_ANIMATION)
