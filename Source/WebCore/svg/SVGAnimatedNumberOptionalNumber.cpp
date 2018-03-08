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
#include "SVGAnimatedNumberOptionalNumber.h"

#include "SVGAnimateElementBase.h"
#include "SVGAnimatedNumber.h"
#include "SVGParserUtilities.h"

namespace WebCore {

SVGAnimatedNumberOptionalNumberAnimator::SVGAnimatedNumberOptionalNumberAnimator(SVGAnimationElement* animationElement, SVGElement* contextElement)
    : SVGAnimatedTypeAnimator(AnimatedNumberOptionalNumber, animationElement, contextElement)
{
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedNumberOptionalNumberAnimator::constructFromString(const String& string)
{
    return SVGAnimatedType::create(SVGPropertyTraits<std::pair<float, float>>::fromString(string));
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedNumberOptionalNumberAnimator::startAnimValAnimation(const SVGElementAnimatedPropertyList& animatedTypes)
{
    return constructFromBaseValues<SVGAnimatedNumber, SVGAnimatedNumber>(animatedTypes);
}

void SVGAnimatedNumberOptionalNumberAnimator::stopAnimValAnimation(const SVGElementAnimatedPropertyList& animatedTypes)
{
    stopAnimValAnimationForTypes<SVGAnimatedNumber, SVGAnimatedNumber>(animatedTypes);
}

void SVGAnimatedNumberOptionalNumberAnimator::resetAnimValToBaseVal(const SVGElementAnimatedPropertyList& animatedTypes, SVGAnimatedType& type)
{
    resetFromBaseValues<SVGAnimatedNumber, SVGAnimatedNumber>(animatedTypes, type);
}

void SVGAnimatedNumberOptionalNumberAnimator::animValWillChange(const SVGElementAnimatedPropertyList& animatedTypes)
{
    animValWillChangeForTypes<SVGAnimatedNumber, SVGAnimatedNumber>(animatedTypes);
}

void SVGAnimatedNumberOptionalNumberAnimator::animValDidChange(const SVGElementAnimatedPropertyList& animatedTypes)
{
    animValDidChangeForTypes<SVGAnimatedNumber, SVGAnimatedNumber>(animatedTypes);
}

void SVGAnimatedNumberOptionalNumberAnimator::addAnimatedTypes(SVGAnimatedType* from, SVGAnimatedType* to)
{
    ASSERT(from->type() == AnimatedNumberOptionalNumber);
    ASSERT(from->type() == to->type());

    const auto& fromNumberPair = from->as<std::pair<float, float>>();
    auto& toNumberPair = to->as<std::pair<float, float>>();

    toNumberPair.first += fromNumberPair.first;
    toNumberPair.second += fromNumberPair.second;
}

void SVGAnimatedNumberOptionalNumberAnimator::calculateAnimatedValue(float percentage, unsigned repeatCount, SVGAnimatedType* from, SVGAnimatedType* to, SVGAnimatedType* toAtEndOfDuration, SVGAnimatedType* animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);

    const auto& fromNumberPair = (m_animationElement->animationMode() == ToAnimation ? animated : from)->as<std::pair<float, float>>();
    const auto& toNumberPair = to->as<std::pair<float, float>>();
    const auto& toAtEndOfDurationNumberPair = toAtEndOfDuration->as<std::pair<float, float>>();
    auto& animatedNumberPair = animated->as<std::pair<float, float>>();

    m_animationElement->animateAdditiveNumber(percentage, repeatCount, fromNumberPair.first, toNumberPair.first, toAtEndOfDurationNumberPair.first, animatedNumberPair.first);
    m_animationElement->animateAdditiveNumber(percentage, repeatCount, fromNumberPair.second, toNumberPair.second, toAtEndOfDurationNumberPair.second, animatedNumberPair.second);
}

float SVGAnimatedNumberOptionalNumberAnimator::calculateDistance(const String&, const String&)
{
    // FIXME: Distance calculation is not possible for SVGNumberOptionalNumber right now. We need the distance for every single value.
    return -1;
}

}
