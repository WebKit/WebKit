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
#include "SVGAnimatedPreserveAspectRatio.h"

#include "SVGAnimateElement.h"

namespace WebCore {
    
SVGAnimatedPreserveAspectRatioAnimator::SVGAnimatedPreserveAspectRatioAnimator(SVGAnimationElement* animationElement, SVGElement* contextElement)
    : SVGAnimatedTypeAnimator(AnimatedPreserveAspectRatio, animationElement, contextElement)
{
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedPreserveAspectRatioAnimator::constructFromString(const String& string)
{
    OwnPtr<SVGAnimatedType> animatedType = SVGAnimatedType::createPreserveAspectRatio(new SVGPreserveAspectRatio);
    SVGPreserveAspectRatio::parsePreserveAspectRatio(animatedType.get(), string);
    return animatedType.release();
}

void SVGAnimatedPreserveAspectRatioAnimator::calculateFromAndToValues(OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, const String& fromString, const String& toString)
{
    ASSERT(m_contextElement);
    ASSERT(m_animationElement);
    
    from = constructFromString(fromString);
    to = constructFromString(toString);
}

void SVGAnimatedPreserveAspectRatioAnimator::calculateFromAndByValues(OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, const String& fromString, const String& byString)
{
    ASSERT(m_contextElement);
    ASSERT(m_animationElement);
    
    // Not specified what to do on 'by'-animations with SVGPreserveAspectRatio. Fallback to 'to'-animation right now. 
    from = constructFromString(fromString);
    to = constructFromString(byString);
}

void SVGAnimatedPreserveAspectRatioAnimator::calculateAnimatedValue(float percentage, unsigned,
                                                       OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, OwnPtr<SVGAnimatedType>& animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);
    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(m_animationElement);
    
    AnimationMode animationMode = animationElement->animationMode();
    SVGPreserveAspectRatio& animateString = animated->preserveAspectRatio();
    if ((animationMode == FromToAnimation && percentage > 0.5) || animationMode == ToAnimation || percentage == 1)
        animateString = to->preserveAspectRatio();
    else
        animateString = from->preserveAspectRatio();
}

float SVGAnimatedPreserveAspectRatioAnimator::calculateDistance(const String&, const String&)
{
    // No paced animations for SVGPreserveAspectRatio.
    return -1;
}

}

#endif // ENABLE(SVG)
