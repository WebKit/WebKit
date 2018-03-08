/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "SVGAnimateElementBase.h"
#include "SVGParserUtilities.h"

namespace WebCore {

SVGAnimatedNumberAnimator::SVGAnimatedNumberAnimator(SVGAnimationElement* animationElement, SVGElement* contextElement)
    : SVGAnimatedTypeAnimator(AnimatedNumber, animationElement, contextElement)
{
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedNumberAnimator::constructFromString(const String& string)
{
    return SVGAnimatedType::create(SVGPropertyTraits<float>::fromString(string));
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedNumberAnimator::startAnimValAnimation(const SVGElementAnimatedPropertyList& animatedTypes)
{
    return constructFromBaseValue<SVGAnimatedNumber>(animatedTypes);
}

void SVGAnimatedNumberAnimator::stopAnimValAnimation(const SVGElementAnimatedPropertyList& animatedTypes)
{
    stopAnimValAnimationForType<SVGAnimatedNumber>(animatedTypes);
}

void SVGAnimatedNumberAnimator::resetAnimValToBaseVal(const SVGElementAnimatedPropertyList& animatedTypes, SVGAnimatedType& type)
{
    resetFromBaseValue<SVGAnimatedNumber>(animatedTypes, type);
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

    to->as<float>() += from->as<float>();
}

static float parseNumberFromString(SVGAnimationElement*, const String& string)
{
    return SVGPropertyTraits<float>::fromString(string);
}

void SVGAnimatedNumberAnimator::calculateAnimatedValue(float percentage, unsigned repeatCount, SVGAnimatedType* from, SVGAnimatedType* to, SVGAnimatedType* toAtEndOfDuration, SVGAnimatedType* animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);

    auto fromNumber = (m_animationElement->animationMode() == ToAnimation ? animated : from)->as<float>();
    auto toNumber = to->as<float>();
    const auto toAtEndOfDurationNumber = toAtEndOfDuration->as<float>();
    auto& animatedNumber = animated->as<float>();

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
