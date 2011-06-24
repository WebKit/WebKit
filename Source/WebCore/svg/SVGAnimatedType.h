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

#if ENABLE(SVG) && ENABLE(SVG_ANIMATION)
#include "SVGElement.h"

namespace WebCore {

class Color;
class FloatRect;
class SVGAngle;
class SVGLength;
class SVGPointList;

class SVGAnimatedType {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~SVGAnimatedType();

    static PassOwnPtr<SVGAnimatedType> createAngle(SVGAngle*);
    static PassOwnPtr<SVGAnimatedType> createColor(Color*);
    static PassOwnPtr<SVGAnimatedType> createLength(SVGLength*);
    static PassOwnPtr<SVGAnimatedType> createNumber(float*);
    static PassOwnPtr<SVGAnimatedType> createPointList(SVGPointList*);
    static PassOwnPtr<SVGAnimatedType> createRect(FloatRect*);
    static PassOwnPtr<SVGAnimatedType> createString(String*);
    
    AnimatedAttributeType type() const { return m_type; }

    SVGAngle& angle();
    Color& color();
    SVGLength& length();
    float& number();
    SVGPointList& pointList();
    FloatRect& rect();
    String& string();

    String valueAsString();
    bool setValueAsString(const QualifiedName&, const String&);
    
private:
    SVGAnimatedType(AnimatedAttributeType);
    
    AnimatedAttributeType m_type;
    
    union DataUnion {
        DataUnion()
            : length(0)
        {
        }
        
        // FIXME: More SVG primitive types need to be added step by step.
        SVGAngle* angle;
        Color* color;
        SVGLength* length;
        float* number;
        SVGPointList* pointList;
        FloatRect* rect;
        String* string;
    } m_data;
};
    
} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_ANIMATION)
#endif // SVGAnimatedType_h
