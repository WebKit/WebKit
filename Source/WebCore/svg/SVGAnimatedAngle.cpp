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

#include "SVGAnimateElement.h"
#include "SVGMarkerElement.h"

namespace WebCore {

SVGAnimatedAngleAnimator::SVGAnimatedAngleAnimator(SVGAnimationElement* animationElement, SVGElement* contextElement)
    : SVGAnimatedTypeAnimator(AnimatedAngle, animationElement, contextElement)
{
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedAngleAnimator::constructFromString(const String& string)
{
    auto animatedType = SVGAnimatedType::createAngleAndEnumeration(std::make_unique<std::pair<SVGAngle, unsigned>>());
    std::pair<SVGAngle, unsigned>& animatedPair = animatedType->angleAndEnumeration();

    SVGAngle angle;
    SVGMarkerOrientType orientType = SVGPropertyTraits<SVGMarkerOrientType>::fromString(string,  angle);
    if (orientType > 0)
        animatedPair.second = orientType;
    if (orientType == SVGMarkerOrientAngle)
        animatedPair.first = angle;

    return animatedType;
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedAngleAnimator::startAnimValAnimation(const SVGElementAnimatedPropertyList& animatedTypes)
{
    return SVGAnimatedType::createAngleAndEnumeration(constructFromBaseValues<SVGAnimatedAngle, SVGAnimatedEnumeration>(animatedTypes));
}

void SVGAnimatedAngleAnimator::stopAnimValAnimation(const SVGElementAnimatedPropertyList& animatedTypes)
{
    stopAnimValAnimationForTypes<SVGAnimatedAngle, SVGAnimatedEnumeration>(animatedTypes);
}

void SVGAnimatedAngleAnimator::resetAnimValToBaseVal(const SVGElementAnimatedPropertyList& animatedTypes, SVGAnimatedType* type)
{
    resetFromBaseValues<SVGAnimatedAngle, SVGAnimatedEnumeration>(animatedTypes, type, &SVGAnimatedType::angleAndEnumeration);
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

    const std::pair<SVGAngle, unsigned>& fromAngleAndEnumeration = from->angleAndEnumeration();
    std::pair<SVGAngle, unsigned>& toAngleAndEnumeration = to->angleAndEnumeration();
    // Only respect by animations, if from and by are both specified in angles (and not eg. 'auto').
    if (fromAngleAndEnumeration.second != toAngleAndEnumeration.second || fromAngleAndEnumeration.second != SVGMarkerOrientAngle)
        return;
    const SVGAngle& fromAngle = fromAngleAndEnumeration.first;
    SVGAngle& toAngle = toAngleAndEnumeration.first;
    toAngle.setValue(toAngle.value() + fromAngle.value());
}

void SVGAnimatedAngleAnimator::calculateAnimatedValue(float percentage, unsigned repeatCount, SVGAnimatedType* from, SVGAnimatedType* to, SVGAnimatedType* toAtEndOfDuration, SVGAnimatedType* animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);

    const std::pair<SVGAngle, unsigned>& fromAngleAndEnumeration = m_animationElement->animationMode() == ToAnimation ? animated->angleAndEnumeration() : from->angleAndEnumeration();
    const std::pair<SVGAngle, unsigned>& toAngleAndEnumeration = to->angleAndEnumeration();
    const std::pair<SVGAngle, unsigned>& toAtEndOfDurationAngleAndEnumeration = toAtEndOfDuration->angleAndEnumeration();
    std::pair<SVGAngle, unsigned>& animatedAngleAndEnumeration = animated->angleAndEnumeration();

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

    // From 'auto' to 'auto'.
    if (fromAngleAndEnumeration.second == SVGMarkerOrientAuto) {
        animatedAngleAndEnumeration.first.setValue(0);
        animatedAngleAndEnumeration.second = SVGMarkerOrientAuto;
        return;
    }

    // If the enumeration value is not angle or auto, its unknown.
    if (fromAngleAndEnumeration.second != SVGMarkerOrientAngle) {
        animatedAngleAndEnumeration.first.setValue(0);
        animatedAngleAndEnumeration.second = SVGMarkerOrientUnknown;
        return;
    }

    // Regular from angle to angle animation, with all features like additive etc.
    animatedAngleAndEnumeration.second = SVGMarkerOrientAngle;

    SVGAngle& animatedSVGAngle = animatedAngleAndEnumeration.first;
    const SVGAngle& toAtEndOfDurationSVGAngle = toAtEndOfDurationAngleAndEnumeration.first;
    float animatedAngle = animatedSVGAngle.value();
    m_animationElement->animateAdditiveNumber(percentage, repeatCount, fromAngleAndEnumeration.first.value(), toAngleAndEnumeration.first.value(), toAtEndOfDurationSVGAngle.value(), animatedAngle);
    animatedSVGAngle.setValue(animatedAngle);
}

float SVGAnimatedAngleAnimator::calculateDistance(const String& fromString, const String& toString)
{
    SVGAngle from = SVGAngle();
    from.setValueAsString(fromString, ASSERT_NO_EXCEPTION);
    SVGAngle to = SVGAngle();
    to.setValueAsString(toString, ASSERT_NO_EXCEPTION);
    return fabsf(to.value() - from.value());
}

}
