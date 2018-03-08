/*
 * Copyright (C) Research In Motion Limited 2011, 2012. All rights reserved.
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
#include "SVGAnimatedAngle.h"

#include "SVGAnimateElementBase.h"
#include "SVGMarkerElement.h"

namespace WebCore {

SVGAnimatedAngleAnimator::SVGAnimatedAngleAnimator(SVGAnimationElement* animationElement, SVGElement* contextElement)
    : SVGAnimatedTypeAnimator(AnimatedAngle, animationElement, contextElement)
{
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedAngleAnimator::constructFromString(const String& string)
{
    return SVGAnimatedType::create(SVGPropertyTraits<std::pair<SVGAngleValue, unsigned>>::fromString(string));
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedAngleAnimator::startAnimValAnimation(const SVGElementAnimatedPropertyList& animatedTypes)
{
    return constructFromBaseValues<SVGAnimatedAngle, SVGAnimatedEnumeration>(animatedTypes);
}

void SVGAnimatedAngleAnimator::stopAnimValAnimation(const SVGElementAnimatedPropertyList& animatedTypes)
{
    stopAnimValAnimationForTypes<SVGAnimatedAngle, SVGAnimatedEnumeration>(animatedTypes);
}

void SVGAnimatedAngleAnimator::resetAnimValToBaseVal(const SVGElementAnimatedPropertyList& animatedTypes, SVGAnimatedType& type)
{
    resetFromBaseValues<SVGAnimatedAngle, SVGAnimatedEnumeration>(animatedTypes, type);
}

void SVGAnimatedAngleAnimator::animValWillChange(const SVGElementAnimatedPropertyList& animatedTypes)
{
    animValWillChangeForTypes<SVGAnimatedAngle, SVGAnimatedEnumeration>(animatedTypes);
}

void SVGAnimatedAngleAnimator::animValDidChange(const SVGElementAnimatedPropertyList& animatedTypes)
{
    animValDidChangeForTypes<SVGAnimatedAngle, SVGAnimatedEnumeration>(animatedTypes);
}

void SVGAnimatedAngleAnimator::addAnimatedTypes(SVGAnimatedType* from, SVGAnimatedType* to)
{
    ASSERT(from->type() == AnimatedAngle);
    ASSERT(from->type() == to->type());

    const auto& fromAngleAndEnumeration = from->as<std::pair<SVGAngleValue, unsigned>>();
    auto& toAngleAndEnumeration = to->as<std::pair<SVGAngleValue, unsigned>>();
    // Only respect by animations, if from and by are both specified in angles (and not eg. 'auto').
    if (fromAngleAndEnumeration.second != toAngleAndEnumeration.second || fromAngleAndEnumeration.second != SVGMarkerOrientAngle)
        return;
    const auto& fromAngle = fromAngleAndEnumeration.first;
    auto& toAngle = toAngleAndEnumeration.first;
    toAngle.setValue(toAngle.value() + fromAngle.value());
}

void SVGAnimatedAngleAnimator::calculateAnimatedValue(float percentage, unsigned repeatCount, SVGAnimatedType* from, SVGAnimatedType* to, SVGAnimatedType* toAtEndOfDuration, SVGAnimatedType* animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);

    const auto& fromAngleAndEnumeration = (m_animationElement->animationMode() == ToAnimation ? animated : from)->as<std::pair<SVGAngleValue, unsigned>>();
    auto& toAngleAndEnumeration = to->as<std::pair<SVGAngleValue, unsigned>>();
    auto& toAtEndOfDurationAngleAndEnumeration = toAtEndOfDuration->as<std::pair<SVGAngleValue, unsigned>>();
    auto& animatedAngleAndEnumeration = animated->as<std::pair<SVGAngleValue, unsigned>>();

    if (fromAngleAndEnumeration.second != toAngleAndEnumeration.second) {
        // Discrete animation - no linear interpolation possible between values (e.g. auto to angle).
        if (percentage < 0.5f) {
            animatedAngleAndEnumeration.second = fromAngleAndEnumeration.second;
            if (fromAngleAndEnumeration.second == SVGMarkerOrientAngle)
                animatedAngleAndEnumeration.first = fromAngleAndEnumeration.first;
            else
                animatedAngleAndEnumeration.first.setValue(0);
            return;
        }
        animatedAngleAndEnumeration.second = toAngleAndEnumeration.second;
        if (toAngleAndEnumeration.second == SVGMarkerOrientAngle)
            animatedAngleAndEnumeration.first = toAngleAndEnumeration.first;
        else
            animatedAngleAndEnumeration.first.setValue(0);
        return;
    }

    if (fromAngleAndEnumeration.second == SVGMarkerOrientAngle) {
        // Regular from angle to angle animation, with support for smooth interpolation, and additive and accumulated animation.
        animatedAngleAndEnumeration.second = SVGMarkerOrientAngle;

        auto& animatedSVGAngle = animatedAngleAndEnumeration.first;
        const auto& toAtEndOfDurationSVGAngle = toAtEndOfDurationAngleAndEnumeration.first;
        float animatedAngle = animatedSVGAngle.value();
        m_animationElement->animateAdditiveNumber(percentage, repeatCount, fromAngleAndEnumeration.first.value(), toAngleAndEnumeration.first.value(), toAtEndOfDurationSVGAngle.value(), animatedAngle);
        animatedSVGAngle.setValue(animatedAngle);
        return;
    }

    // auto, auto-start-reverse, or unknown.
    animatedAngleAndEnumeration.first.setValue(0);

    if (fromAngleAndEnumeration.second == SVGMarkerOrientAuto || fromAngleAndEnumeration.second == SVGMarkerOrientAutoStartReverse)
        animatedAngleAndEnumeration.second = fromAngleAndEnumeration.second;
    else
        animatedAngleAndEnumeration.second = SVGMarkerOrientUnknown;
}

float SVGAnimatedAngleAnimator::calculateDistance(const String& fromString, const String& toString)
{
    auto from = SVGAngleValue();
    from.setValueAsString(fromString);
    auto to = SVGAngleValue();
    to.setValueAsString(toString);
    return fabsf(to.value() - from.value());
}

}
