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
#include "SVGAnimatedAngle.h"

#include "SVGAnimateElement.h"

namespace WebCore {

SVGAnimatedAngleAnimator::SVGAnimatedAngleAnimator(SVGAnimationElement* animationElement, SVGElement* contextElement)
    : SVGAnimatedTypeAnimator(AnimatedAngle, animationElement, contextElement)
{
}

static inline SVGAngle& sharedSVGAngle(const String& valueAsString)
{
    DEFINE_STATIC_LOCAL(SVGAngle, sharedAngle, ());
    ExceptionCode ec = 0;
    sharedAngle.setValueAsString(valueAsString, ec);
    ASSERT(!ec);
    return sharedAngle;
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedAngleAnimator::constructFromString(const String& string)
{
    OwnPtr<SVGAnimatedType> animatedType = SVGAnimatedType::createAngle(new SVGAngle);
    ExceptionCode ec = 0;
    animatedType->angle().setValueAsString(string, ec);
    ASSERT(!ec);
    return animatedType.release();
}

void SVGAnimatedAngleAnimator::calculateFromAndToValues(OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, const String& fromString, const String& toString)
{
    ASSERT(m_contextElement);
    ASSERT(m_animationElement);
    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(m_animationElement);
    animationElement->determinePropertyValueTypes(fromString, toString);

    from = constructFromString(fromString);
    to = constructFromString(toString);
}

void SVGAnimatedAngleAnimator::calculateFromAndByValues(OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, const String& fromString, const String& byString)
{
    ASSERT(m_contextElement);
    ASSERT(m_animationElement);
    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(m_animationElement);
    animationElement->determinePropertyValueTypes(fromString, byString);
    
    from = constructFromString(fromString);
    to = constructFromString(byString);
    
    SVGAngle& fromAngle = from->angle();
    SVGAngle& toAngle = to->angle();
    toAngle.setValue(toAngle.value() + fromAngle.value());
}

void SVGAnimatedAngleAnimator::calculateAnimatedValue(float percentage, unsigned repeatCount,
                                                      OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, OwnPtr<SVGAnimatedType>& animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);
    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(m_animationElement);
    
    AnimationMode animationMode = animationElement->animationMode();
    // To animation uses contributions from the lower priority animations as the base value.
    // FIXME: Avoid string parsing.
    if (animationMode == ToAnimation)
        from = constructFromString(animated->angle().valueAsString());
    
    // Replace 'inherit' by their computed property values.
    float number;
    float fromAngle = from->angle().value();
    float toAngle = to->angle().value();
    
    if (animationElement->fromPropertyValueType() == InheritValue) {
        String fromAngleString;
        animationElement->adjustForInheritance(m_contextElement, animationElement->attributeName(), fromAngleString);
        fromAngle = sharedSVGAngle(fromAngleString).value();
    }
    if (animationElement->toPropertyValueType() == InheritValue) {
        String toAngleString;
        animationElement->adjustForInheritance(m_contextElement, animationElement->attributeName(), toAngleString);
        toAngle = sharedSVGAngle(toAngleString).value(); 
    }
    
    if (animationElement->calcMode() == CalcModeDiscrete)
        number = percentage < 0.5f ? fromAngle : toAngle;
    else
        number = (toAngle - fromAngle) * percentage + fromAngle;
    
    // FIXME: This is not correct for values animation.
    if (animationElement->isAccumulated() && repeatCount)
        number += toAngle * repeatCount;
    SVGAngle& animatedSVGAngle = animated->angle();
    if (animationElement->isAdditive() && animationMode != ToAnimation) {
        float animatedSVGAngleValue = animatedSVGAngle.value();
        animatedSVGAngleValue += number;
        animatedSVGAngle.setValue(animatedSVGAngleValue);
    } else
        animatedSVGAngle.setValue(number);
}

float SVGAnimatedAngleAnimator::calculateDistance(const String& fromString, const String& toString)
{
    ExceptionCode ec = 0;
    SVGAngle from = SVGAngle();
    from.setValueAsString(fromString, ec);
    ASSERT(!ec);
    SVGAngle to = SVGAngle();
    to.setValueAsString(toString, ec);
    ASSERT(!ec);
    return fabsf(to.value() - from.value());
}

}

#endif // ENABLE(SVG)
