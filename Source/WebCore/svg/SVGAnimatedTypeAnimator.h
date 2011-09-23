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

#ifndef SVGAnimatedTypeAnimator_h
#define SVGAnimatedTypeAnimator_h

#if ENABLE(SVG)
#include "SVGAnimatedType.h"
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class SVGAnimationElement;

class SVGAnimatedTypeAnimator {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~SVGAnimatedTypeAnimator() { }
    virtual PassOwnPtr<SVGAnimatedType> constructFromString(const String&) = 0;
    
    virtual void calculateFromAndToValues(OwnPtr<SVGAnimatedType>& fromValue, OwnPtr<SVGAnimatedType>& toValue, const String& fromString, const String& toString) = 0;
    virtual void calculateFromAndByValues(OwnPtr<SVGAnimatedType>& fromValue, OwnPtr<SVGAnimatedType>& toValue, const String& fromString, const String& toString) = 0;
    virtual void calculateAnimatedValue(float percentage, unsigned repeatCount,
                                        OwnPtr<SVGAnimatedType>& fromValue, OwnPtr<SVGAnimatedType>& toValue, OwnPtr<SVGAnimatedType>& animatedValue) = 0;
    virtual float calculateDistance(const String& fromString, const String& toString) = 0;

    void setContextElement(SVGElement* contextElement) { m_contextElement = contextElement; }
    
protected:
    SVGAnimatedTypeAnimator(AnimatedPropertyType type, SVGAnimationElement* animationElement, SVGElement* contextElement)
        : m_type(type)
        , m_animationElement(animationElement)
        , m_contextElement(contextElement)
    {
    }

    AnimatedPropertyType m_type;
    SVGAnimationElement* m_animationElement;
    SVGElement* m_contextElement;
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGAnimatedTypeAnimator_h
