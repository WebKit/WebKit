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

#pragma once

#include "Color.h"
#include "FloatRect.h"
#include "SVGAngleValue.h"
#include "SVGLengthListValues.h"
#include "SVGLengthValue.h"
#include "SVGNumberListValues.h"
#include "SVGPointListValues.h"
#include "SVGPreserveAspectRatioValue.h"
#include "SVGPropertyInfo.h"
#include "SVGTransformListValues.h"

namespace WebCore {

class SVGPathByteStream;

class SVGAnimatedType {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SVGAnimatedType(AnimatedPropertyType);
    virtual ~SVGAnimatedType();

    static std::unique_ptr<SVGAnimatedType> createAngleAndEnumeration(std::unique_ptr<std::pair<SVGAngleValue, unsigned>>);
    static std::unique_ptr<SVGAnimatedType> createBoolean(std::unique_ptr<bool>);
    static std::unique_ptr<SVGAnimatedType> createColor(std::unique_ptr<Color>);
    static std::unique_ptr<SVGAnimatedType> createEnumeration(std::unique_ptr<unsigned>);
    static std::unique_ptr<SVGAnimatedType> createInteger(std::unique_ptr<int>);
    static std::unique_ptr<SVGAnimatedType> createIntegerOptionalInteger(std::unique_ptr<std::pair<int, int>>);
    static std::unique_ptr<SVGAnimatedType> createLength(std::unique_ptr<SVGLengthValue>);
    static std::unique_ptr<SVGAnimatedType> createLengthList(std::unique_ptr<SVGLengthListValues>);
    static std::unique_ptr<SVGAnimatedType> createNumber(std::unique_ptr<float>);
    static std::unique_ptr<SVGAnimatedType> createNumberList(std::unique_ptr<SVGNumberListValues>);
    static std::unique_ptr<SVGAnimatedType> createNumberOptionalNumber(std::unique_ptr<std::pair<float, float>>);
    static std::unique_ptr<SVGAnimatedType> createPath(std::unique_ptr<SVGPathByteStream>);
    static std::unique_ptr<SVGAnimatedType> createPointList(std::unique_ptr<SVGPointListValues>);
    static std::unique_ptr<SVGAnimatedType> createPreserveAspectRatio(std::unique_ptr<SVGPreserveAspectRatioValue>);
    static std::unique_ptr<SVGAnimatedType> createRect(std::unique_ptr<FloatRect>);
    static std::unique_ptr<SVGAnimatedType> createString(std::unique_ptr<String>);
    static std::unique_ptr<SVGAnimatedType> createTransformList(std::unique_ptr<SVGTransformListValues>);
    static bool supportsAnimVal(AnimatedPropertyType);

    AnimatedPropertyType type() const { return m_type; }

    // Non-mutable accessors.
    const std::pair<SVGAngleValue, unsigned>& angleAndEnumeration() const
    {
        ASSERT(m_type == AnimatedAngle);
        return *m_data.angleAndEnumeration;
    }

    const bool& boolean() const
    {
        ASSERT(m_type == AnimatedBoolean);
        return *m_data.boolean;
    }

    const Color& color() const
    {
        ASSERT(m_type == AnimatedColor);
        return *m_data.color;
    }

    const unsigned& enumeration() const
    {
        ASSERT(m_type == AnimatedEnumeration);
        return *m_data.enumeration;
    }

    const int& integer() const
    {
        ASSERT(m_type == AnimatedInteger);
        return *m_data.integer;
    }

    const std::pair<int, int>& integerOptionalInteger() const
    {
        ASSERT(m_type == AnimatedIntegerOptionalInteger);
        return *m_data.integerOptionalInteger;
    }

    const SVGLengthValue& length() const
    {
        ASSERT(m_type == AnimatedLength);
        return *m_data.length;
    }

    const SVGLengthListValues& lengthList() const
    {
        ASSERT(m_type == AnimatedLengthList);
        return *m_data.lengthList;
    }

    const float& number() const
    {
        ASSERT(m_type == AnimatedNumber);
        return *m_data.number;
    }

    const SVGNumberListValues& numberList() const
    {
        ASSERT(m_type == AnimatedNumberList);
        return *m_data.numberList;
    }

    const std::pair<float, float>& numberOptionalNumber() const
    {
        ASSERT(m_type == AnimatedNumberOptionalNumber);
        return *m_data.numberOptionalNumber;
    }

    const SVGPathByteStream* path() const
    {
        ASSERT(m_type == AnimatedPath);
        return m_data.path;
    }

    const SVGPointListValues& pointList() const
    {
        ASSERT(m_type == AnimatedPoints);
        return *m_data.pointList;
    }

    const SVGPreserveAspectRatioValue& preserveAspectRatio() const
    {
        ASSERT(m_type == AnimatedPreserveAspectRatio);
        return *m_data.preserveAspectRatio;
    }

    const FloatRect& rect() const
    {
        ASSERT(m_type == AnimatedRect);
        return *m_data.rect;
    }

    const String& string() const
    {
        ASSERT(m_type == AnimatedString);
        return *m_data.string;
    }

    const SVGTransformListValues& transformList() const
    {
        ASSERT(m_type == AnimatedTransformList);
        return *m_data.transformList;
    }

    // Mutable accessors.
    std::pair<SVGAngleValue, unsigned>& angleAndEnumeration()
    {
        ASSERT(m_type == AnimatedAngle);
        return *m_data.angleAndEnumeration;
    }

    bool& boolean()
    {
        ASSERT(m_type == AnimatedBoolean);
        return *m_data.boolean;
    }

    Color& color()
    {
        ASSERT(m_type == AnimatedColor);
        return *m_data.color;
    }

    unsigned& enumeration()
    {
        ASSERT(m_type == AnimatedEnumeration);
        return *m_data.enumeration;
    }

    int& integer()
    {
        ASSERT(m_type == AnimatedInteger);
        return *m_data.integer;
    }

    std::pair<int, int>& integerOptionalInteger()
    {
        ASSERT(m_type == AnimatedIntegerOptionalInteger);
        return *m_data.integerOptionalInteger;
    }

    SVGLengthValue& length()
    {
        ASSERT(m_type == AnimatedLength);
        return *m_data.length;
    }

    SVGLengthListValues& lengthList()
    {
        ASSERT(m_type == AnimatedLengthList);
        return *m_data.lengthList;
    }

    float& number()
    {
        ASSERT(m_type == AnimatedNumber);
        return *m_data.number;
    }

    SVGNumberListValues& numberList()
    {
        ASSERT(m_type == AnimatedNumberList);
        return *m_data.numberList;
    }

    std::pair<float, float>& numberOptionalNumber()
    {
        ASSERT(m_type == AnimatedNumberOptionalNumber);
        return *m_data.numberOptionalNumber;
    }

    SVGPathByteStream* path()
    {
        ASSERT(m_type == AnimatedPath);
        return m_data.path;
    }

    SVGPointListValues& pointList()
    {
        ASSERT(m_type == AnimatedPoints);
        return *m_data.pointList;
    }

    SVGPreserveAspectRatioValue& preserveAspectRatio()
    {
        ASSERT(m_type == AnimatedPreserveAspectRatio);
        return *m_data.preserveAspectRatio;
    }

    FloatRect& rect()
    {
        ASSERT(m_type == AnimatedRect);
        return *m_data.rect;
    }

    String& string()
    {
        ASSERT(m_type == AnimatedString);
        return *m_data.string;
    }

    SVGTransformListValues& transformList()
    {
        ASSERT(m_type == AnimatedTransformList);
        return *m_data.transformList;
    }

    String valueAsString();
    bool setValueAsString(const QualifiedName&, const String&);

private:
    AnimatedPropertyType m_type;

    union DataUnion {
        DataUnion()
            : length(nullptr)
        {
        }

        std::pair<SVGAngleValue, unsigned>* angleAndEnumeration;
        bool* boolean;
        Color* color;
        unsigned* enumeration;
        int* integer;
        std::pair<int, int>* integerOptionalInteger;
        SVGLengthValue* length;
        SVGLengthListValues* lengthList;
        float* number;
        SVGNumberListValues* numberList;
        std::pair<float, float>* numberOptionalNumber;
        SVGPathByteStream* path;
        SVGPreserveAspectRatioValue* preserveAspectRatio;
        SVGPointListValues* pointList;
        FloatRect* rect;
        String* string;
        SVGTransformListValues* transformList;
    } m_data;
};

} // namespace WebCore
