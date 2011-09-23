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
#include "SVGAnimatedInteger.h"

#include "SVGAnimateElement.h"
#include "SVGAnimatedNumber.h"
#include <wtf/MathExtras.h>

namespace WebCore {

SVGAnimatedIntegerAnimator::SVGAnimatedIntegerAnimator(SVGAnimationElement* animationElement, SVGElement* contextElement)
    : SVGAnimatedTypeAnimator(AnimatedInteger, animationElement, contextElement)
{
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedIntegerAnimator::constructFromString(const String& string)
{
    OwnPtr<SVGAnimatedType> animtedType = SVGAnimatedType::createInteger(new int);
    animtedType->integer() = string.toIntStrict();
    return animtedType.release();
}

void SVGAnimatedIntegerAnimator::calculateFromAndToValues(OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, const String& fromString, const String& toString)
{
    ASSERT(m_contextElement);
    ASSERT(m_animationElement);
    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(m_animationElement);
    animationElement->determinePropertyValueTypes(fromString, toString);
    
    from = constructFromString(fromString);
    to = constructFromString(toString);
}

void SVGAnimatedIntegerAnimator::calculateFromAndByValues(OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, const String& fromString, const String& byString)
{
    ASSERT(m_contextElement);
    ASSERT(m_animationElement);
    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(m_animationElement);
    animationElement->determinePropertyValueTypes(fromString, byString);
    
    from = constructFromString(fromString);
    to = constructFromString(byString);
    
    to->integer() += from->integer();
}

void SVGAnimatedIntegerAnimator::calculateAnimatedValue(float percentage, unsigned repeatCount,
                                                        OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, OwnPtr<SVGAnimatedType>& animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);

    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(m_animationElement);    
    AnimationMode animationMode = animationElement->animationMode();

    // To animation uses contributions from the lower priority animations as the base value.
    int& animatedInt = animated->integer();
    if (animationMode == ToAnimation)
        from->integer() = animatedInt;
    
    float result = animatedInt;
    SVGAnimatedNumberAnimator::calculateAnimatedNumber(animationElement, percentage, repeatCount, result, from->integer(), to->integer());
    animatedInt = static_cast<int>(roundf(result));
}

float SVGAnimatedIntegerAnimator::calculateDistance(const String& fromString, const String& toString)
{
    ASSERT(m_contextElement);
    int from = fromString.toIntStrict();
    int to = toString.toIntStrict();
    return abs(to - from);
}
    
}

#endif // ENABLE(SVG)
