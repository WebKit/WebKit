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
#include "SVGAnimatedNumber.h"

#include "SVGAnimateElement.h"
#include "SVGParserUtilities.h"

namespace WebCore {

SVGAnimatedNumberAnimator::SVGAnimatedNumberAnimator(SVGAnimationElement* animationElement, SVGElement* contextElement)
    : SVGAnimatedTypeAnimator(AnimatedNumber, animationElement, contextElement)
{
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedNumberAnimator::constructFromString(const String& string)
{
    auto animatedType = SVGAnimatedType::createNumber(std::make_unique<float>());
    float& animatedNumber = animatedType->number();
    if (!parseNumberFromString(string, animatedNumber))
        animatedNumber = 0;
    return animatedType;
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedNumberAnimator::startAnimValAnimation(const SVGElementAnimatedPropertyList& animatedTypes)
{
    return SVGAnimatedType::createNumber(constructFromBaseValue<SVGAnimatedNumber>(animatedTypes));
}

void SVGAnimatedNumberAnimator::stopAnimValAnimation(const SVGElementAnimatedPropertyList& animatedTypes)
{
    stopAnimValAnimationForType<SVGAnimatedNumber>(animatedTypes);
}

void SVGAnimatedNumberAnimator::resetAnimValToBaseVal(const SVGElementAnimatedPropertyList& animatedTypes, SVGAnimatedType* type)
{
    resetFromBaseValue<SVGAnimatedNumber>(animatedTypes, type, &SVGAnimatedType::number);
}

void SVGAnimatedNumberAnimator::animValWillChange(const SVGElementAnimatedPropertyList& animatedTypes)
{
    animValWillChangeForType<SVGAnimatedNumber>(animatedTypes);
}

void SVGAnimatedNumberAnimator::animValDidChange(const SVGElementAnimatedPropertyList& animatedTypes)
{
    animValDidChangeForType<SVGAnimatedNumber>(animatedTypes);
}

void SVGAnimatedNumberAnimator::addAnimatedTypes(SVGAnimatedType* from, SVGAnimatedType* to)
{
    ASSERT(from->type() == AnimatedNumber);
    ASSERT(from->type() == to->type());

    to->number() += from->number();
}

static float parseNumberFromString(SVGAnimationElement*, const String& string)
{
    float number = 0;
    parseNumberFromString(string, number);
    return number;
}

void SVGAnimatedNumberAnimator::calculateAnimatedValue(float percentage, unsigned repeatCount, SVGAnimatedType* from, SVGAnimatedType* to, SVGAnimatedType* toAtEndOfDuration, SVGAnimatedType* animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);

    float fromNumber = m_animationElement->animationMode() == ToAnimation ? animated->number() : from->number();
    float toNumber = to->number();
    float toAtEndOfDurationNumber = toAtEndOfDuration->number();
    float& animatedNumber = animated->number();

    // Apply CSS inheritance rules.
    m_animationElement->adjustForInheritance<float>(parseNumberFromString, m_animationElement->fromPropertyValueType(), fromNumber, m_contextElement);
    m_animationElement->adjustForInheritance<float>(parseNumberFromString, m_animationElement->toPropertyValueType(), toNumber, m_contextElement);

    m_animationElement->animateAdditiveNumber(percentage, repeatCount, fromNumber, toNumber, toAtEndOfDurationNumber, animatedNumber);
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
