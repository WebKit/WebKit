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
#include "SVGAnimatedLengthList.h"

#include "SVGAnimateElement.h"
#include "SVGAnimatedNumber.h"

namespace WebCore {

SVGAnimatedLengthListAnimator::SVGAnimatedLengthListAnimator(SVGAnimationElement* animationElement, SVGElement* contextElement)
    : SVGAnimatedTypeAnimator(AnimatedLengthList, animationElement, contextElement)
    , m_lengthMode(SVGLength::lengthModeForAnimatedLengthAttribute(animationElement->attributeName()))
{
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedLengthListAnimator::constructFromString(const String& string)
{
    OwnPtr<SVGAnimatedType> animateType = SVGAnimatedType::createLengthList(new SVGLengthList);
    animateType->lengthList().parse(string, m_lengthMode);
    return animateType.release();
}

void SVGAnimatedLengthListAnimator::calculateFromAndToValues(OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, const String& fromString, const String& toString)
{
    ASSERT(m_contextElement);
    ASSERT(m_animationElement);
    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(m_animationElement);
    animationElement->determinePropertyValueTypes(fromString, toString);
    
    from = constructFromString(fromString);
    to = constructFromString(toString);
}

void SVGAnimatedLengthListAnimator::calculateFromAndByValues(OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, const String& fromString, const String& byString)
{
    ASSERT(m_contextElement);
    ASSERT(m_animationElement);
    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(m_animationElement);
    animationElement->determinePropertyValueTypes(fromString, byString);
    
    from = constructFromString(fromString);
    to = constructFromString(byString);
    
    SVGLengthList& fromLengthList = from->lengthList();
    SVGLengthList& toLengthList = to->lengthList();
    unsigned itemsCount = fromLengthList.size();
    if (itemsCount != toLengthList.size())
        return;
    ExceptionCode ec = 0;
    for (unsigned i = 0; i < itemsCount; ++i) {
        toLengthList[i].setValue(toLengthList[i].value(m_contextElement) + fromLengthList[i].value(m_contextElement), m_contextElement, ec);
        ASSERT(!ec);
    }
}

void SVGAnimatedLengthListAnimator::calculateAnimatedValue(float percentage, unsigned repeatCount,
                                                       OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, OwnPtr<SVGAnimatedType>& animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);

    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(m_animationElement);
    AnimationMode animationMode = animationElement->animationMode();

    // To animation uses contributions from the lower priority animations as the base value.
    SVGLengthList& fromLengthList = from->lengthList();
    SVGLengthList& animatedLengthList = animated->lengthList();
    if (animationMode == ToAnimation)
        fromLengthList = animatedLengthList;
    
    // Replace 'inherit' by their computed property values.    
    SVGLengthList& toLengthList = to->lengthList();
    if (animationElement->fromPropertyValueType() == InheritValue) {
        String fromLengthString;
        animationElement->adjustForInheritance(m_contextElement, animationElement->attributeName(), fromLengthString);
        fromLengthList.parse(fromLengthString, m_lengthMode);
    }
    if (animationElement->toPropertyValueType() == InheritValue) {
        String toLengthString;
        animationElement->adjustForInheritance(m_contextElement, animationElement->attributeName(), toLengthString);
        toLengthList.parse(toLengthString, m_lengthMode);
    }

    unsigned itemsCount = fromLengthList.size();
    if (itemsCount != toLengthList.size()) {
        if (percentage < 0.5) {
            if (animationMode != ToAnimation)
                animatedLengthList = fromLengthList;
        } else
            animatedLengthList = toLengthList;
        return;
    }
    
    bool animatedListSizeEqual = itemsCount == animatedLengthList.size();
    if (!animatedListSizeEqual)
        animatedLengthList.clear();
    ExceptionCode ec = 0;
    for (unsigned i = 0; i < itemsCount; ++i) {
        float result = animatedListSizeEqual ? animatedLengthList[i].value(m_contextElement) : 0;
        SVGLengthType unitType = percentage < 0.5 ? fromLengthList[i].unitType() : toLengthList[i].unitType();
        SVGAnimatedNumberAnimator::calculateAnimatedNumber(animationElement, percentage, repeatCount, result, fromLengthList[i].value(m_contextElement), toLengthList[i].value(m_contextElement));
        if (!animatedListSizeEqual)
            animatedLengthList.append(SVGLength(m_contextElement, result, m_lengthMode, unitType));
        else {
            animatedLengthList[i].setValue(m_contextElement, result, m_lengthMode, unitType, ec);
            ASSERT(!ec);
        }
    }
}

float SVGAnimatedLengthListAnimator::calculateDistance(const String&, const String&)
{
    // FIXME: Distance calculation is not possible for SVGLengthList right now. We need the distance for every single value.
    return -1;
}

}

#endif // ENABLE(SVG)
