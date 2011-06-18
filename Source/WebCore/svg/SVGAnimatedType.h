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
#include "SVGAngle.h"
#include "SVGElement.h"
#include "SVGLength.h"

namespace WebCore {

class SVGAnimatedType {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<SVGAnimatedType> create(AnimatedAttributeType type)
    {
        return adoptPtr(new SVGAnimatedType(type));
    }

    static PassOwnPtr<SVGAnimatedType> createAngle(SVGAngle* angle)
    {
        ASSERT(angle);
        OwnPtr<SVGAnimatedType> animatedType = create(AnimatedAngle);
        animatedType->m_data.angle = angle;
        return animatedType.release();
    }

    static PassOwnPtr<SVGAnimatedType> createLength(SVGLength* length)
    {
        ASSERT(length);
        OwnPtr<SVGAnimatedType> animatedType = create(AnimatedLength);
        animatedType->m_data.length = length;
        return animatedType.release();
    }

    virtual ~SVGAnimatedType()
    {
        switch (m_type) {
        case AnimatedAngle:
            delete m_data.angle;
            break;
        case AnimatedLength:
            delete m_data.length;
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }
    
    AnimatedAttributeType type() const { return m_type; }

    SVGAngle& angle()
    {
        ASSERT(m_type == AnimatedAngle);
        return *m_data.angle;
    }

    SVGLength& length()
    {
        ASSERT(m_type == AnimatedLength);
        return *m_data.length;
    }
    
    String valueAsString()
    {
        switch (m_type) {
        case AnimatedAngle:
            ASSERT(m_data.angle);
            return m_data.angle->valueAsString();
        case AnimatedLength:
            ASSERT(m_data.length);
            return m_data.length->valueAsString();
        default:
            break;
        }
        ASSERT_NOT_REACHED();
        return String();
    }
    
    bool setValueAsString(const QualifiedName& attrName, const String& value)
    {
        ExceptionCode ec = 0;
        switch (m_type) {
        case AnimatedAngle:
            ASSERT(m_data.angle);
            m_data.angle->setValueAsString(value, ec);
            break;
        case AnimatedLength:
            ASSERT(m_data.length);
            m_data.length->setValueAsString(value, SVGLength::lengthModeForAnimatedLengthAttribute(attrName), ec);
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
        return !ec;
    }
    
private:
    SVGAnimatedType(AnimatedAttributeType type)
        : m_type(type)
    {
    }
    
    AnimatedAttributeType m_type;
    
    union DataUnion {
        DataUnion()
            : length(0)
        {
        }
        
        // FIXME: More SVG primitive types need to be added step by step.
        SVGAngle* angle;
        SVGLength* length;
    } m_data;
};
    
} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_ANIMATION)
#endif // SVGAnimatedType_h
