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

PassOwnPtr<SVGAnimatedType> SVGAnimatedLengthAnimator::startAnimValAnimation(const Vector<SVGAnimatedProperty*>& properties)
{
    return SVGAnimatedType::createLength(constructFromBaseValue<SVGAnimatedLength>(properties));
}

void SVGAnimatedLengthAnimator::stopAnimValAnimation(const Vector<SVGAnimatedProperty*>& properties)
{
    stopAnimValAnimationForType<SVGAnimatedLength>(properties);
}

void SVGAnimatedLengthAnimator::resetAnimValToBaseVal(const Vector<SVGAnimatedProperty*>& properties, SVGAnimatedType* type)
{
    resetFromBaseValue<SVGAnimatedLength>(properties, type, &SVGAnimatedType::length);
}

void SVGAnimatedLengthAnimator::animValWillChange(const Vector<SVGAnimatedProperty*>& properties)
{
    animValWillChangeForType<SVGAnimatedLength>(properties);
}

void SVGAnimatedLengthAnimator::animValDidChange(const Vector<SVGAnimatedProperty*>& properties)
{
    animValDidChangeForType<SVGAnimatedLength>(properties);
}

void SVGAnimatedLengthAnimator::addAnimatedTypes(SVGAnimatedType* from, SVGAnimatedType* to)
{
    ASSERT(from->type() == AnimatedLength);
    ASSERT(from->type() == to->type());

    SVGLengthContext lengthContext(m_contextElement);
    SVGLength& fromLength = from->length();
    SVGLength& toLength = to->length();

    ExceptionCode ec = 0;
    toLength.setValue(toLength.value(lengthContext) + fromLength.value(lengthContext), lengthContext, ec);
    ASSERT(!ec);
}

static SVGLength parseLengthFromString(SVGAnimationElement* animationElement, const String& string)
{
    return sharedSVGLength(SVGLength::lengthModeForAnimatedLengthAttribute(animationElement->attributeName()), string);
}

void SVGAnimatedLengthAnimator::calculateAnimatedValue(float percentage, unsigned repeatCount, OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, OwnPtr<SVGAnimatedType>& animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);

    SVGLength& fromSVGLength = from->length();
    SVGLength& toSVGLength = to->length();
    SVGLength& animatedSVGLength = animated->length();
    m_animationElement->adjustFromToValues<SVGLength>(parseLengthFromString, fromSVGLength, toSVGLength, animatedSVGLength, percentage, m_contextElement);

    SVGLengthContext lengthContext(m_contextElement);
    float animatedNumber = animatedSVGLength.value(lengthContext);
    SVGLengthType unitType = percentage < 0.5 ? fromSVGLength.unitType() : toSVGLength.unitType();
    m_animationElement->animateAdditiveNumber(percentage, repeatCount, fromSVGLength.value(lengthContext), toSVGLength.value(lengthContext), animatedNumber);

    ExceptionCode ec = 0;
    animatedSVGLength.setValue(lengthContext, animatedNumber, m_lengthMode, unitType, ec);
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
