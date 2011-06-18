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

#if ENABLE(SVG) && ENABLE(SVG_ANIMATION)
#include "SVGAnimatedLength.h"

namespace WebCore {

SVGAnimatedLengthAnimator::SVGAnimatedLengthAnimator(SVGElement* contextElement, const QualifiedName& attributeName)
    : SVGAnimatedTypeAnimator(AnimatedLength, contextElement)
    , m_lengthMode(SVGLength::lengthModeForAnimatedLengthAttribute(attributeName))
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
    from = constructFromString(fromString);
    to = constructFromString(toString);
}

void SVGAnimatedLengthAnimator::calculateFromAndByValues(OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, const String& fromString, const String& byString)
{
    ASSERT(m_contextElement);
    
    from = constructFromString(fromString);
    to = constructFromString(byString);
    
    SVGLength& fromLength = from->length();
    SVGLength& toLength = to->length();
    ExceptionCode ec = 0;
    toLength.setValue(toLength.value(m_contextElement) + fromLength.value(m_contextElement), m_contextElement, ec);
    ASSERT(!ec);
}

void SVGAnimatedLengthAnimator::calculateAnimatedValue(SVGSMILElement* smilElement, float percentage, unsigned repeatCount,
                                                       OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, OwnPtr<SVGAnimatedType>& animated,
                                                       bool fromPropertyInherits, bool toPropertyInherits)
{
    ASSERT(smilElement);
    ASSERT(m_contextElement);
    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(smilElement);
    
    AnimationMode animationMode = animationElement->animationMode();
    // To animation uses contributions from the lower priority animations as the base value.
    // FIXME: Avoid string parsing.
    if (animationMode == ToAnimation)
        from = constructFromString(animated->length().valueAsString());
    
    // Replace 'inherit' by their computed property values.
    float number;
    float fromLength = from->length().value(m_contextElement);
    float toLength = to->length().value(m_contextElement);
    
    if (fromPropertyInherits) {
        String fromLengthString;
        animationElement->adjustForInheritance(m_contextElement, animationElement->attributeName(), fromLengthString);
        fromLength = sharedSVGLength(m_lengthMode, fromLengthString).value(m_contextElement);
    }
    if (toPropertyInherits) {
        String toLengthString;
        animationElement->adjustForInheritance(m_contextElement, animationElement->attributeName(), toLengthString);
        toLength = sharedSVGLength(m_lengthMode, toLengthString).value(m_contextElement); 
    }
    
    if (animationElement->calcMode() == CalcModeDiscrete)
        number = percentage < 0.5f ? fromLength : toLength;
    else
        number = (toLength - fromLength) * percentage + fromLength;
    
    // FIXME: This is not correct for values animation.
    if (animationElement->isAccumulated() && repeatCount)
        number += toLength * repeatCount;
    ExceptionCode ec = 0;
    SVGLength& animatedSVGLength = animated->length();
    if (animationElement->isAdditive() && animationMode != ToAnimation) {
        float animatedSVGLengthValue = animatedSVGLength.value(m_contextElement);
        animatedSVGLengthValue += number;
        animatedSVGLength.setValue(animatedSVGLengthValue, m_contextElement, ec);
    } else
        animatedSVGLength.setValue(number, m_contextElement, ec);
    ASSERT(!ec);
}

float SVGAnimatedLengthAnimator::calculateDistance(SVGSMILElement* smilElement, const String& fromString, const String& toString)
{
    ASSERT(smilElement);
    ASSERT(m_contextElement);
    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(smilElement);    
    SVGLengthMode lengthMode = SVGLength::lengthModeForAnimatedLengthAttribute(animationElement->attributeName());
    SVGLength from = SVGLength(lengthMode, fromString);
    SVGLength to = SVGLength(lengthMode, toString);
    return fabsf(to.value(m_contextElement) - from.value(m_contextElement));
}

}

#endif // ENABLE(SVG) && ENABLE(SVG_ANIMATION)
