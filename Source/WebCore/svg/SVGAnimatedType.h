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
#include "SVGAnimateElement.h"
#include "SVGElement.h"
#include "SVGLength.h"

namespace WebCore {

class SVGAnimatedType {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<SVGAnimatedType> createLength(SVGLength* length)
    {
        return adoptPtr(new SVGAnimatedType(AnimatedLength, length));
    }

    virtual ~SVGAnimatedType()
    {
        ASSERT(m_type == AnimatedLength);
        delete m_data.length;
    }
    
    AnimatedAttributeType type() const { return m_type; }
    
    SVGLength& length()
    {
        ASSERT(m_type == AnimatedLength);
        return *m_data.length;
    }
    
private:
    SVGAnimatedType(AnimatedAttributeType type, SVGLength* length)
        : m_type(type)
    {
        m_data.length = length;
    }
    
    AnimatedAttributeType m_type;
    
    union DataUnion {
        DataUnion()
            : length(0)
        {
        }
        
        // FIXME: More SVG primitive types need to be added step by step.
        SVGLength* length;
    } m_data;
};
    
} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_ANIMATION)
#endif // SVGAnimatedType_h
