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
#include "SVGAnimatedLength.h"

#include "SVGAnimateElement.h"
#include "SVGAnimatedNumber.h"

namespace WebCore {

SVGAnimatedLengthAnimator::SVGAnimatedLengthAnimator(SVGAnimationElement* animationElement, SVGElement* contextElement)
    : SVGAnimatedTypeAnimator(AnimatedLength, animationElement, contextElement)
    , m_lengthMode(SVGLength::lengthModeForAnimatedLengthAttribute(animationElement->attributeName()))
{
}

static inline SVGLength& sharedSVGLength(SVGLengthMode mode, const String& valueAsString)
{
    DEFINE_STATIC_LOCAL(SVGLength, sharedLength, ());
    ExceptionCode ec = 0;
    sharedLength.setValueAsString(valueAsString, mode, ec);
    ASSERT(!ec);
    return sharedLength;
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedLengthAnimator::constructFromString(const String& string)
{
    return SVGAnimatedType::createLength(new SVGLength(m_lengthMode, string));
}

void SVGAnimatedLengthAnimator::calculateFromAndToValues(OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, const String& fromString, const String& toString)
{
    ASSERT(m_contextElement);
    ASSERT(m_animationElement);
    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(m_animationElement);
    animationElement->determinePropertyValueTypes(fromString, toString);
    
    from = constructFromString(fromString);
    to = constructFromString(toString);
}

void SVGAnimatedLengthAnimator::calculateFromAndByValues(OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, const String& fromString, const String& byString)
{
    ASSERT(m_contextElement);
    ASSERT(m_animationElement);
    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(m_animationElement);
    animationElement->determinePropertyValueTypes(fromString, byString);
    
    from = constructFromString(fromString);
    to = constructFromString(byString);
    
    SVGLengthContext lengthContext(m_contextElement);
    SVGLength& fromLength = from->length();
    SVGLength& toLength = to->length();
    ExceptionCode ec = 0;
    toLength.setValue(toLength.value(lengthContext) + fromLength.value(lengthContext), lengthContext, ec);
    ASSERT(!ec);
}

void SVGAnimatedLengthAnimator::calculateAnimatedValue(float percentage, unsigned repeatCount,
                                                       OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, OwnPtr<SVGAnimatedType>& animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);

    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(m_animationElement);
    AnimationMode animationMode = animationElement->animationMode();

    // To animation uses contributions from the lower priority animations as the base value.
    SVGLength& animatedSVGLength = animated->length();
    SVGLength& fromSVGLength = from->length();
    if (animationMode == ToAnimation)
        fromSVGLength = animatedSVGLength;
    
    // Replace 'inherit' by their computed property values.
    SVGLength& toSVGLength = to->length();
    if (animationElement->fromPropertyValueType() == InheritValue) {
        String fromLengthString;
        animationElement->adjustForInheritance(m_contextElement, animationElement->attributeName(), fromLengthString);
        fromSVGLength = sharedSVGLength(m_lengthMode, fromLengthString);
    }
    if (animationElement->toPropertyValueType() == InheritValue) {
        String toLengthString;
        animationElement->adjustForInheritance(m_contextElement, animationElement->attributeName(), toLengthString);
        toSVGLength = sharedSVGLength(m_lengthMode, toLengthString); 
    }
    
    SVGLengthContext lengthContext(m_contextElement);
    float result = animatedSVGLength.value(lengthContext);
    SVGLengthType unitType = percentage < 0.5 ? fromSVGLength.unitType() : toSVGLength.unitType();
    SVGAnimatedNumberAnimator::calculateAnimatedNumber(animationElement, percentage, repeatCount, result, fromSVGLength.value(lengthContext), toSVGLength.value(lengthContext));

    ExceptionCode ec = 0;
    animatedSVGLength.setValue(lengthContext, result, m_lengthMode, unitType, ec);
    ASSERT(!ec);
}

float SVGAnimatedLengthAnimator::calculateDistance(const String& fromString, const String& toString)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);
    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(m_animationElement);    
    SVGLengthMode lengthMode = SVGLength::lengthModeForAnimatedLengthAttribute(animationElement->attributeName());
    SVGLength from = SVGLength(lengthMode, fromString);
    SVGLength to = SVGLength(lengthMode, toString);
    SVGLengthContext lengthContext(m_contextElement);
    return fabsf(to.value(lengthContext) - from.value(lengthContext));
}

}

#endif // ENABLE(SVG)
