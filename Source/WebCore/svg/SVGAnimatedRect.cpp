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
#include "SVGAnimatedRect.h"

#include "SVGAnimateElement.h"
#include "SVGParserUtilities.h"

namespace WebCore {

SVGAnimatedRectAnimator::SVGAnimatedRectAnimator(SVGAnimationElement* animationElement, SVGElement* contextElement)
    : SVGAnimatedTypeAnimator(AnimatedRect, animationElement, contextElement)
{
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedRectAnimator::constructFromString(const String& string)
{
    OwnPtr<SVGAnimatedType> animatedType = SVGAnimatedType::createRect(new FloatRect);
    parseRect(string, animatedType->rect());
    return animatedType.release();
}

void SVGAnimatedRectAnimator::calculateFromAndToValues(OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, const String& fromString, const String& toString)
{
    from = constructFromString(fromString);
    to = constructFromString(toString);
}

void SVGAnimatedRectAnimator::calculateFromAndByValues(OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, const String& fromString, const String& byString)
{
    ASSERT(m_contextElement);
    
    from = constructFromString(fromString);
    to = constructFromString(byString);

    to->rect() += from->rect();
}

void SVGAnimatedRectAnimator::calculateAnimatedValue(float percentage, unsigned repeatCount,
                                                       OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, OwnPtr<SVGAnimatedType>& animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);

    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(m_animationElement);
    AnimationMode animationMode = animationElement->animationMode();
    // To animation uses contributions from the lower priority animations as the base value.
    FloatRect& animatedRect = animated->rect();
    if (animationMode == ToAnimation)
        from->rect() = animatedRect;
    
    const FloatRect& fromRect = from->rect();
    const FloatRect& toRect = to->rect();
    FloatRect newRect;    
    if (animationElement->calcMode() == CalcModeDiscrete)
        newRect = percentage < 0.5 ? fromRect : toRect;
    else
        newRect = FloatRect((toRect.x() - fromRect.x()) * percentage + fromRect.x(),
                            (toRect.y() - fromRect.y()) * percentage + fromRect.y(),
                            (toRect.width() - fromRect.width()) * percentage + fromRect.width(),
                            (toRect.height() - fromRect.height()) * percentage + fromRect.height());
    
    // FIXME: This is not correct for values animation. Right now we transform values-animation to multiple from-to-animations and
    // accumulate every single value to the previous one. But accumulation should just take into account after a complete cycle
    // of values-animaiton. See example at: http://www.w3.org/TR/2001/REC-smil-animation-20010904/#RepeatingAnim
    if (animationElement->isAccumulated() && repeatCount) {
        newRect += toRect;
        newRect.scale(repeatCount);
    }
    
    if (animationElement->isAdditive() && animationMode != ToAnimation)
        animatedRect += newRect;
    else
        animatedRect = newRect;
}

float SVGAnimatedRectAnimator::calculateDistance(const String&, const String&)
{
    // FIXME: Distance calculation is not possible for SVGRect right now. We need the distance of for every single value.
    return -1;
}
    
}

#endif // ENABLE(SVG)
