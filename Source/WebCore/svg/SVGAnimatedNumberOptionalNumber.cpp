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
#include "SVGAnimatedNumberOptionalNumber.h"

#include "SVGAnimateElement.h"
#include "SVGAnimatedNumber.h"
#include "SVGParserUtilities.h"

using namespace std;

namespace WebCore {

SVGAnimatedNumberOptionalNumberAnimator::SVGAnimatedNumberOptionalNumberAnimator(SVGAnimationElement* animationElement, SVGElement* contextElement)
    : SVGAnimatedTypeAnimator(AnimatedNumberOptionalNumber, animationElement, contextElement)
{
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedNumberOptionalNumberAnimator::constructFromString(const String& string)
{
    OwnPtr<SVGAnimatedType> animtedType = SVGAnimatedType::createNumberOptionalNumber(new pair<float, float>);
    pair<float, float>& animatedNumber = animtedType->numberOptionalNumber();
    if (!parseNumberOptionalNumber(string, animatedNumber.first, animatedNumber.second)) {
        animatedNumber.first = 0;
        animatedNumber.second = 0;
    }
    return animtedType.release();
}

void SVGAnimatedNumberOptionalNumberAnimator::calculateFromAndToValues(OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, const String& fromString, const String& toString)
{
    ASSERT(m_contextElement);
    ASSERT(m_animationElement);
    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(m_animationElement);
    animationElement->determinePropertyValueTypes(fromString, toString);
    
    from = constructFromString(fromString);
    to = constructFromString(toString);
}

void SVGAnimatedNumberOptionalNumberAnimator::calculateFromAndByValues(OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, const String& fromString, const String& byString)
{
    ASSERT(m_contextElement);
    ASSERT(m_animationElement);
    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(m_animationElement);
    animationElement->determinePropertyValueTypes(fromString, byString);
    
    from = constructFromString(fromString);
    to = constructFromString(byString);

    pair<float, float>& fromNumberPair = from->numberOptionalNumber();
    pair<float, float>& toNumberPair = to->numberOptionalNumber();
    
    toNumberPair.first += fromNumberPair.first;
    toNumberPair.second += fromNumberPair.second;
}

void SVGAnimatedNumberOptionalNumberAnimator::calculateAnimatedValue(float percentage, unsigned repeatCount,
                                                       OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, OwnPtr<SVGAnimatedType>& animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);

    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(m_animationElement);
    AnimationMode animationMode = animationElement->animationMode();

    // To animation uses contributions from the lower priority animations as the base value.
    pair<float, float>& fromNumberPair = from->numberOptionalNumber();
    pair<float, float>& animatedNumberPair = animated->numberOptionalNumber();
    if (animationMode == ToAnimation)
        fromNumberPair = animatedNumberPair;

    pair<float, float>& toNumberPair = to->numberOptionalNumber();
    SVGAnimatedNumberAnimator::calculateAnimatedNumber(animationElement, percentage, repeatCount, animatedNumberPair.first, fromNumberPair.first, toNumberPair.first);
    SVGAnimatedNumberAnimator::calculateAnimatedNumber(animationElement, percentage, repeatCount, animatedNumberPair.second, fromNumberPair.second, toNumberPair.second);
}

float SVGAnimatedNumberOptionalNumberAnimator::calculateDistance(const String&, const String&)
{
    // FIXME: Distance calculation is not possible for SVGNumberOptionalNumber right now. We need the distance for every single value.
    return -1;
}

}

#endif // ENABLE(SVG)
