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
#include "SVGAnimatedString.h"

#include "SVGAnimateElement.h"

namespace WebCore {

SVGAnimatedStringAnimator::SVGAnimatedStringAnimator(SVGAnimationElement* animationElement, SVGElement* contextElement)
    : SVGAnimatedTypeAnimator(AnimatedString, animationElement, contextElement)
{
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedStringAnimator::constructFromString(const String& string)
{
    OwnPtr<SVGAnimatedType> animatedType = SVGAnimatedType::createString(new String);
    animatedType->string() = string;
    return animatedType.release();
}

void SVGAnimatedStringAnimator::calculateFromAndToValues(OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, const String& fromString, const String& toString)
{
    ASSERT(m_contextElement);
    ASSERT(m_animationElement);

    from = constructFromString(fromString);
    to = constructFromString(toString);
}

void SVGAnimatedStringAnimator::calculateFromAndByValues(OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, const String& fromString, const String& byString)
{
    ASSERT(m_contextElement);
    ASSERT(m_animationElement);
    
    // Not specified what to do on 'by'-animations with string. Fallback to 'to'-animation right now. 
    from = constructFromString(fromString);
    to = constructFromString(byString);
}

void SVGAnimatedStringAnimator::calculateAnimatedValue(float percentage, unsigned,
                                                       OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, OwnPtr<SVGAnimatedType>& animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);
    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(m_animationElement);
    
    AnimationMode animationMode = animationElement->animationMode();
    String& animateString = animated->string();
    if ((animationMode == FromToAnimation && percentage > 0.5) || animationMode == ToAnimation || percentage == 1)
        animateString = to->string();
    else
        animateString = from->string();
}

float SVGAnimatedStringAnimator::calculateDistance(const String&, const String&)
{
    // No paced animations for strings.
    return -1;
}

}

#endif // ENABLE(SVG)
