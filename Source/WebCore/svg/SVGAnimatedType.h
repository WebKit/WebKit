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

#ifndef SVGAnimatedType_h
#define SVGAnimatedType_h

#include "FloatRect.h"
#include "SVGAngle.h"
#include "SVGColor.h"
#include "SVGLength.h"
#include "SVGLengthList.h"
#include "SVGNumberList.h"
#include "SVGPointList.h"
#include "SVGPreserveAspectRatio.h"
#include "SVGPropertyInfo.h"
#include "SVGTransformList.h"

namespace WebCore {

class SVGPathByteStream;

class SVGAnimatedType {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SVGAnimatedType(AnimatedPropertyType);
    virtual ~SVGAnimatedType();

    static std::unique_ptr<SVGAnimatedType> createAngleAndEnumeration(std::unique_ptr<std::pair<SVGAngle, unsigned>>);
    static std::unique_ptr<SVGAnimatedType> createBoolean(std::unique_ptr<bool>);
    static std::unique_ptr<SVGAnimatedType> createColor(std::unique_ptr<Color>);
    static std::unique_ptr<SVGAnimatedType> createEnumeration(std::unique_ptr<unsigned>);
    static std::unique_ptr<SVGAnimatedType> createInteger(std::unique_ptr<int>);
    static std::unique_ptr<SVGAnimatedType> createIntegerOptionalInteger(std::unique_ptr<std::pair<int, int>>);
    static std::unique_ptr<SVGAnimatedType> createLength(std::unique_ptr<SVGLength>);
    static std::unique_ptr<SVGAnimatedType> createLengthList(std::unique_ptr<SVGLengthList>);
    static std::unique_ptr<SVGAnimatedType> createNumber(std::unique_ptr<float>);
    static std::unique_ptr<SVGAnimatedType> createNumberList(std::unique_ptr<SVGNumberList>);
    static std::unique_ptr<SVGAnimatedType> createNumberOptionalNumber(std::unique_ptr<std::pair<float, float>>);
    static std::unique_ptr<SVGAnimatedType> createPath(std::unique_ptr<SVGPathByteStream>);
    static std::unique_ptr<SVGAnimatedType> createPointList(std::unique_ptr<SVGPointList>);
    static std::unique_ptr<SVGAnimatedType> createPreserveAspectRatio(std::unique_ptr<SVGPreserveAspectRatio>);
    static std::unique_ptr<SVGAnimatedType> createRect(std::unique_ptr<FloatRect>);
    static std::unique_ptr<SVGAnimatedType> createString(std::unique_ptr<String>);
    static std::unique_ptr<SVGAnimatedType> createTransformList(std::unique_ptr<SVGTransformList>);
    static bool supportsAnimVal(AnimatedPropertyType);

    AnimatedPropertyType type() const { return m_type; }

    // Non-mutable accessors.
    const std::pair<SVGAngle, unsigned>& angleAndEnumeration() const
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

    const SVGLength& length() const
    {
        ASSERT(m_type == AnimatedLength);
        return *m_data.length;
    }

    const SVGLengthList& lengthList() const
    {
        ASSERT(m_type == AnimatedLengthList);
        return *m_data.lengthList;
    }

    const float& number() const
    {
        ASSERT(m_type == AnimatedNumber);
        return *m_data.number;
    }

    const SVGNumberList& numberList() const
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

    const SVGPointList& pointList() const
    {
        ASSERT(m_type == AnimatedPoints);
        return *m_data.pointList;
    }

    const SVGPreserveAspectRatio& preserveAspectRatio() const
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

    const SVGTransformList& transformList() const
    {
        ASSERT(m_type == AnimatedTransformList);
        return *m_data.transformList;
    }

    // Mutable accessors.
    std::pair<SVGAngle, unsigned>& angleAndEnumeration()
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

    SVGLength& length()
    {
        ASSERT(m_type == AnimatedLength);
        return *m_data.length;
    }

    SVGLengthList& lengthList()
    {
        ASSERT(m_type == AnimatedLengthList);
        return *m_data.lengthList;
    }

    float& number()
    {
        ASSERT(m_type == AnimatedNumber);
        return *m_data.number;
    }

    SVGNumberList& numberList()
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

    SVGPointList& pointList()
    {
        ASSERT(m_type == AnimatedPoints);
        return *m_data.pointList;
    }

    SVGPreserveAspectRatio& preserveAspectRatio()
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

    SVGTransformList& transformList()
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
            : length(0)
        {
        }

        std::pair<SVGAngle, unsigned>* angleAndEnumeration;
        bool* boolean;
        Color* color;
        unsigned* enumeration;
        int* integer;
        std::pair<int, int>* integerOptionalInteger;
        SVGLength* length;
        SVGLengthList* lengthList;
        float* number;
        SVGNumberList* numberList;
        std::pair<float, float>* numberOptionalNumber;
        SVGPathByteStream* path;
        SVGPreserveAspectRatio* preserveAspectRatio;
        SVGPointList* pointList;
        FloatRect* rect;
        String* string;
        SVGTransformList* transformList;
    } m_data;
};

} // namespace WebCore

#endif // SVGAnimatedType_h
