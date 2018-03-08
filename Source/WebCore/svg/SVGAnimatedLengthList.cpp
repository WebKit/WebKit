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
#include "SVGAnimatedLengthList.h"

#include "SVGAnimateElementBase.h"
#include "SVGAnimatedNumber.h"

namespace WebCore {

SVGAnimatedLengthListAnimator::SVGAnimatedLengthListAnimator(SVGAnimationElement* animationElement, SVGElement* contextElement)
    : SVGAnimatedTypeAnimator(AnimatedLengthList, animationElement, contextElement)
    , m_lengthMode(SVGLengthValue::lengthModeForAnimatedLengthAttribute(animationElement->attributeName()))
{
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedLengthListAnimator::constructFromString(const String& string)
{
    return SVGAnimatedType::create(SVGPropertyTraits<SVGLengthListValues>::fromString(string, m_lengthMode));
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedLengthListAnimator::startAnimValAnimation(const SVGElementAnimatedPropertyList& animatedTypes)
{
    return constructFromBaseValue<SVGAnimatedLengthList>(animatedTypes);
}

void SVGAnimatedLengthListAnimator::stopAnimValAnimation(const SVGElementAnimatedPropertyList& animatedTypes)
{
    stopAnimValAnimationForType<SVGAnimatedLengthList>(animatedTypes);
}

void SVGAnimatedLengthListAnimator::resetAnimValToBaseVal(const SVGElementAnimatedPropertyList& animatedTypes, SVGAnimatedType& type)
{
    resetFromBaseValue<SVGAnimatedLengthList>(animatedTypes, type);
}

void SVGAnimatedLengthListAnimator::animValWillChange(const SVGElementAnimatedPropertyList& animatedTypes)
{
    animValWillChangeForType<SVGAnimatedLengthList>(animatedTypes);
}

void SVGAnimatedLengthListAnimator::animValDidChange(const SVGElementAnimatedPropertyList& animatedTypes)
{
    animValDidChangeForType<SVGAnimatedLengthList>(animatedTypes);
}

void SVGAnimatedLengthListAnimator::addAnimatedTypes(SVGAnimatedType* from, SVGAnimatedType* to)
{
    ASSERT(from->type() == AnimatedLengthList);
    ASSERT(from->type() == to->type());

    const auto& fromLengthList = from->as<SVGLengthListValues>();
    auto& toLengthList = to->as<SVGLengthListValues>();

    unsigned fromLengthListSize = fromLengthList.size();
    if (!fromLengthListSize || fromLengthListSize != toLengthList.size())
        return;

    SVGLengthContext lengthContext(m_contextElement);
    for (unsigned i = 0; i < fromLengthListSize; ++i)
        toLengthList[i].setValue(toLengthList[i].value(lengthContext) + fromLengthList[i].value(lengthContext), lengthContext);
}

static SVGLengthListValues parseLengthListFromString(SVGAnimationElement* animationElement, const String& string)
{
    SVGLengthListValues lengthList;
    lengthList.parse(string, SVGLengthValue::lengthModeForAnimatedLengthAttribute(animationElement->attributeName()));
    return lengthList;
}

void SVGAnimatedLengthListAnimator::calculateAnimatedValue(float percentage, unsigned repeatCount, SVGAnimatedType* from, SVGAnimatedType* to, SVGAnimatedType* toAtEndOfDuration, SVGAnimatedType* animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);

    auto fromLengthList = (m_animationElement->animationMode() == ToAnimation ? animated : from)->as<SVGLengthListValues>();
    auto toLengthList = to->as<SVGLengthListValues>();
    const auto& toAtEndOfDurationLengthList = toAtEndOfDuration->as<SVGLengthListValues>();
    auto& animatedLengthList = animated->as<SVGLengthListValues>();

    // Apply CSS inheritance rules.
    m_animationElement->adjustForInheritance<SVGLengthListValues>(parseLengthListFromString, m_animationElement->fromPropertyValueType(), fromLengthList, m_contextElement);
    m_animationElement->adjustForInheritance<SVGLengthListValues>(parseLengthListFromString, m_animationElement->toPropertyValueType(), toLengthList, m_contextElement);

    if (!m_animationElement->adjustFromToListValues<SVGLengthListValues>(fromLengthList, toLengthList, animatedLengthList, percentage))
        return;

    unsigned fromLengthListSize = fromLengthList.size();
    unsigned toLengthListSize = toLengthList.size();
    unsigned toAtEndOfDurationListSize = toAtEndOfDurationLengthList.size();

    SVGLengthContext lengthContext(m_contextElement);
    for (unsigned i = 0; i < toLengthListSize; ++i) {
        float animatedNumber = animatedLengthList[i].value(lengthContext);
        SVGLengthType unitType = toLengthList[i].unitType();
        float effectiveFrom = 0;
        if (fromLengthListSize) {
            if (percentage < 0.5)
                unitType = fromLengthList[i].unitType();
            effectiveFrom = fromLengthList[i].value(lengthContext);
        }
        float effectiveToAtEnd = i < toAtEndOfDurationListSize ? toAtEndOfDurationLengthList[i].value(lengthContext) : 0;

        m_animationElement->animateAdditiveNumber(percentage, repeatCount, effectiveFrom, toLengthList[i].value(lengthContext), effectiveToAtEnd, animatedNumber);
        animatedLengthList[i].setValue(lengthContext, animatedNumber, m_lengthMode, unitType);
    }
}

float SVGAnimatedLengthListAnimator::calculateDistance(const String&, const String&)
{
    // FIXME: Distance calculation is not possible for SVGLengthListValues right now. We need the distance for every single value.
    return -1;
}

}
