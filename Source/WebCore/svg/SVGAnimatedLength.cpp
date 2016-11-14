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
#include "SVGAnimatedLength.h"

#include "SVGAnimateElementBase.h"
#include "SVGAnimatedNumber.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

SVGAnimatedLengthAnimator::SVGAnimatedLengthAnimator(SVGAnimationElement* animationElement, SVGElement* contextElement)
    : SVGAnimatedTypeAnimator(AnimatedLength, animationElement, contextElement)
    , m_lengthMode(SVGLengthValue::lengthModeForAnimatedLengthAttribute(animationElement->attributeName()))
{
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedLengthAnimator::constructFromString(const String& string)
{
    return SVGAnimatedType::createLength(std::make_unique<SVGLengthValue>(m_lengthMode, string));
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedLengthAnimator::startAnimValAnimation(const SVGElementAnimatedPropertyList& animatedTypes)
{
    return SVGAnimatedType::createLength(constructFromBaseValue<SVGAnimatedLength>(animatedTypes));
}

void SVGAnimatedLengthAnimator::stopAnimValAnimation(const SVGElementAnimatedPropertyList& animatedTypes)
{
    stopAnimValAnimationForType<SVGAnimatedLength>(animatedTypes);
}

void SVGAnimatedLengthAnimator::resetAnimValToBaseVal(const SVGElementAnimatedPropertyList& animatedTypes, SVGAnimatedType& type)
{
    resetFromBaseValue<SVGAnimatedLength>(animatedTypes, type, &SVGAnimatedType::length);
}

void SVGAnimatedLengthAnimator::animValWillChange(const SVGElementAnimatedPropertyList& animatedTypes)
{
    animValWillChangeForType<SVGAnimatedLength>(animatedTypes);
}

void SVGAnimatedLengthAnimator::animValDidChange(const SVGElementAnimatedPropertyList& animatedTypes)
{
    animValDidChangeForType<SVGAnimatedLength>(animatedTypes);
}

void SVGAnimatedLengthAnimator::addAnimatedTypes(SVGAnimatedType* from, SVGAnimatedType* to)
{
    ASSERT(from->type() == AnimatedLength);
    ASSERT(from->type() == to->type());

    SVGLengthContext lengthContext(m_contextElement);
    const auto& fromLength = from->length();
    auto& toLength = to->length();

    toLength.setValue(toLength.value(lengthContext) + fromLength.value(lengthContext), lengthContext);
}

static SVGLengthValue parseLengthFromString(SVGAnimationElement* animationElement, const String& string)
{
    SVGLengthValue length;
    length.setValueAsString(string, SVGLengthValue::lengthModeForAnimatedLengthAttribute(animationElement->attributeName()));
    return length;
}

void SVGAnimatedLengthAnimator::calculateAnimatedValue(float percentage, unsigned repeatCount, SVGAnimatedType* from, SVGAnimatedType* to, SVGAnimatedType* toAtEndOfDuration, SVGAnimatedType* animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);

    auto fromSVGLength = m_animationElement->animationMode() == ToAnimation ? animated->length() : from->length();
    auto toSVGLength = to->length();
    const auto& toAtEndOfDurationSVGLength = toAtEndOfDuration->length();
    auto& animatedSVGLength = animated->length();

    // Apply CSS inheritance rules.
    m_animationElement->adjustForInheritance<SVGLengthValue>(parseLengthFromString, m_animationElement->fromPropertyValueType(), fromSVGLength, m_contextElement);
    m_animationElement->adjustForInheritance<SVGLengthValue>(parseLengthFromString, m_animationElement->toPropertyValueType(), toSVGLength, m_contextElement);

    SVGLengthContext lengthContext(m_contextElement);
    float animatedNumber = animatedSVGLength.value(lengthContext);
    SVGLengthType unitType = percentage < 0.5 ? fromSVGLength.unitType() : toSVGLength.unitType();
    m_animationElement->animateAdditiveNumber(percentage, repeatCount, fromSVGLength.value(lengthContext), toSVGLength.value(lengthContext), toAtEndOfDurationSVGLength.value(lengthContext), animatedNumber);

    animatedSVGLength.setValue(lengthContext, animatedNumber, m_lengthMode, unitType);
}

float SVGAnimatedLengthAnimator::calculateDistance(const String& fromString, const String& toString)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);
    auto lengthMode = SVGLengthValue::lengthModeForAnimatedLengthAttribute(m_animationElement->attributeName());
    auto from = SVGLengthValue(lengthMode, fromString);
    auto to = SVGLengthValue(lengthMode, toString);
    SVGLengthContext lengthContext(m_contextElement);
    return fabsf(to.value(lengthContext) - from.value(lengthContext));
}

}
