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
#include "SVGAnimatedNumberList.h"

#include "SVGAnimateElement.h"
#include "SVGAnimatedNumber.h"

namespace WebCore {

SVGAnimatedNumberListAnimator::SVGAnimatedNumberListAnimator(SVGAnimationElement* animationElement, SVGElement* contextElement)
    : SVGAnimatedTypeAnimator(AnimatedNumberList, animationElement, contextElement)
{
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedNumberListAnimator::constructFromString(const String& string)
{
    OwnPtr<SVGAnimatedType> animtedType = SVGAnimatedType::createNumberList(new SVGNumberList);
    animtedType->numberList().parse(string);
    return animtedType.release();
}

void SVGAnimatedNumberListAnimator::calculateFromAndToValues(OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, const String& fromString, const String& toString)
{
    ASSERT(m_contextElement);
    ASSERT(m_animationElement);
    
    from = constructFromString(fromString);
    to = constructFromString(toString);
}

void SVGAnimatedNumberListAnimator::calculateFromAndByValues(OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, const String& fromString, const String& byString)
{
    ASSERT(m_contextElement);
    ASSERT(m_animationElement);
    
    from = constructFromString(fromString);
    to = constructFromString(byString);
    
    SVGNumberList& fromNumberList = from->numberList();
    SVGNumberList& toNumberList = to->numberList();
    
    unsigned size = fromNumberList.size();
    if (size != toNumberList.size())
        return;

    for (unsigned i = 0; i < size; ++i)
        toNumberList[i] += fromNumberList[i];
}

void SVGAnimatedNumberListAnimator::calculateAnimatedValue(float percentage, unsigned repeatCount,
                                                           OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, OwnPtr<SVGAnimatedType>& animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);

    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(m_animationElement);    
    AnimationMode animationMode = animationElement->animationMode();

    // To animation uses contributions from the lower priority animations as the base value.
    SVGNumberList& fromNumberList = from->numberList();
    SVGNumberList& animatedNumberList = animated->numberList();
    if (animationMode == ToAnimation)
        fromNumberList = animatedNumberList;

    SVGNumberList& toNumberList = to->numberList();
    unsigned itemsCount = fromNumberList.size();
    if (itemsCount != toNumberList.size()) {
        if (percentage < 0.5) {
            if (animationMode != ToAnimation)
                animatedNumberList = fromNumberList;
        } else
            animatedNumberList = toNumberList;
        return;
    }

    if (itemsCount != animatedNumberList.size())
        animatedNumberList.resize(itemsCount);

    for (unsigned i = 0; i < itemsCount; ++i)
        SVGAnimatedNumberAnimator::calculateAnimatedNumber(animationElement, percentage, repeatCount, animatedNumberList[i], fromNumberList[i], toNumberList[i]);
}

float SVGAnimatedNumberListAnimator::calculateDistance(const String&, const String&)
{
    // FIXME: Distance calculation is not possible for SVGNumberList right now. We need the distance for every single value.
    return -1;
}

}

#endif // ENABLE(SVG)
