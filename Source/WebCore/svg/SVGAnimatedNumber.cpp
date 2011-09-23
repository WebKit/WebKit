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
#include "SVGAnimatedNumber.h"

#include "SVGAnimateElement.h"
#include "SVGParserUtilities.h"

using namespace std;

namespace WebCore {

SVGAnimatedNumberAnimator::SVGAnimatedNumberAnimator(SVGAnimationElement* animationElement, SVGElement* contextElement)
    : SVGAnimatedTypeAnimator(AnimatedNumber, animationElement, contextElement)
{
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedNumberAnimator::constructFromString(const String& string)
{
    OwnPtr<SVGAnimatedType> animtedType = SVGAnimatedType::createNumber(new float);
    float& animatedNumber = animtedType->number();
    if (!parseNumberFromString(string, animatedNumber))
        animatedNumber = 0;
    return animtedType.release();
}

void SVGAnimatedNumberAnimator::calculateFromAndToValues(OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, const String& fromString, const String& toString)
{
    ASSERT(m_contextElement);
    ASSERT(m_animationElement);
    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(m_animationElement);
    animationElement->determinePropertyValueTypes(fromString, toString);
    
    from = constructFromString(fromString);
    to = constructFromString(toString);
}

void SVGAnimatedNumberAnimator::calculateFromAndByValues(OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, const String& fromString, const String& byString)
{
    ASSERT(m_contextElement);
    ASSERT(m_animationElement);
    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(m_animationElement);
    animationElement->determinePropertyValueTypes(fromString, byString);
    
    from = constructFromString(fromString);
    to = constructFromString(byString);

    to->number() += from->number();
}

void SVGAnimatedNumberAnimator::calculateAnimatedNumber(SVGAnimationElement* animationElement, float percentage, unsigned repeatCount, float& animatedNumber, float fromNumber, float toNumber)
{
    float number;
    if (animationElement->calcMode() == CalcModeDiscrete)
        number = percentage < 0.5 ? fromNumber : toNumber;
    else
        number = (toNumber - fromNumber) * percentage + fromNumber;
        
    // FIXME: This is not correct for values animation. Right now we transform values-animation to multiple from-to-animations and
    // accumulate every single value to the previous one. But accumulation should just take into account after a complete cycle
    // of values-animaiton. See example at: http://www.w3.org/TR/2001/REC-smil-animation-20010904/#RepeatingAnim
    if (animationElement->isAccumulated() && repeatCount)
        number += toNumber * repeatCount;
    if (animationElement->isAdditive() && animationElement->animationMode() != ToAnimation)
        animatedNumber += number;
    else
        animatedNumber = number;
}

void SVGAnimatedNumberAnimator::calculateAnimatedValue(float percentage, unsigned repeatCount,
                                                       OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, OwnPtr<SVGAnimatedType>& animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);
    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(m_animationElement);
    
    AnimationMode animationMode = animationElement->animationMode();
    // To animation uses contributions from the lower priority animations as the base value.
    float& animatedNumber = animated->number();
    if (animationMode == ToAnimation)
        from->number() = animatedNumber;
    
    // Replace 'inherit' by their computed property values.
    float& fromNumber = from->number();
    float& toNumber = to->number();
    if (animationElement->fromPropertyValueType() == InheritValue) {
        String fromNumberString;
        animationElement->adjustForInheritance(m_contextElement, animationElement->attributeName(), fromNumberString);
        parseNumberFromString(fromNumberString, fromNumber); 
    }
    if (animationElement->toPropertyValueType() == InheritValue) {
        String toNumberString;
        animationElement->adjustForInheritance(m_contextElement, animationElement->attributeName(), toNumberString);
        parseNumberFromString(toNumberString, toNumber); 
    }
    
    calculateAnimatedNumber(animationElement, percentage, repeatCount, animatedNumber, fromNumber, toNumber);
}

float SVGAnimatedNumberAnimator::calculateDistance(const String& fromString, const String& toString)
{
    ASSERT(m_contextElement);
    float from = 0;
    float to = 0;
    parseNumberFromString(fromString, from);
    parseNumberFromString(toString, to);
    return fabsf(to - from);
}

}

#endif // ENABLE(SVG)
