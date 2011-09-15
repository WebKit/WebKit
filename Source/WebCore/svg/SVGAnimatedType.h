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

#if ENABLE(SVG)
#include "SVGElement.h"

namespace WebCore {

class Color;
class FloatRect;
class SVGAngle;
class SVGLength;
class SVGLengthList;
class SVGNumberList;
class SVGPathByteStream;
class SVGPointList;
class SVGPreserveAspectRatio;

class SVGAnimatedType {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~SVGAnimatedType();

    static PassOwnPtr<SVGAnimatedType> createAngle(SVGAngle*);
    static PassOwnPtr<SVGAnimatedType> createBoolean(bool*);
    static PassOwnPtr<SVGAnimatedType> createColor(Color*);
    static PassOwnPtr<SVGAnimatedType> createInteger(int*);
    static PassOwnPtr<SVGAnimatedType> createLength(SVGLength*);
    static PassOwnPtr<SVGAnimatedType> createLengthList(SVGLengthList*);
    static PassOwnPtr<SVGAnimatedType> createNumber(float*);
    static PassOwnPtr<SVGAnimatedType> createNumberList(SVGNumberList*);
    static PassOwnPtr<SVGAnimatedType> createNumberOptionalNumber(std::pair<float, float>*);
    static PassOwnPtr<SVGAnimatedType> createPath(PassOwnPtr<SVGPathByteStream>);
    static PassOwnPtr<SVGAnimatedType> createPointList(SVGPointList*);
    static PassOwnPtr<SVGAnimatedType> createPreserveAspectRatio(SVGPreserveAspectRatio*);
    static PassOwnPtr<SVGAnimatedType> createRect(FloatRect*);
    static PassOwnPtr<SVGAnimatedType> createString(String*);

    AnimatedPropertyType type() const { return m_type; }

    SVGAngle& angle();
    bool& boolean();
    Color& color();
    int& integer();
    SVGLength& length();
    SVGLengthList& lengthList();
    float& number();
    SVGNumberList& numberList();
    std::pair<float, float>& numberOptionalNumber();
    SVGPathByteStream* path();
    SVGPointList& pointList();
    SVGPreserveAspectRatio& preserveAspectRatio();
    FloatRect& rect();
    String& string();

    String valueAsString();
    bool setValueAsString(const QualifiedName&, const String&);
    
    // Used for parsing a String to a SVGPreserveAspectRatio object.
    void setPreserveAspectRatioBaseValue(const SVGPreserveAspectRatio&);

private:
    SVGAnimatedType(AnimatedPropertyType);

    AnimatedPropertyType m_type;

    union DataUnion {
        DataUnion()
            : length(0)
        {
        }

        SVGAngle* angle;
        bool* boolean;
        Color* color;
        int* integer;
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
    } m_data;
};
    
} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGAnimatedType_h
