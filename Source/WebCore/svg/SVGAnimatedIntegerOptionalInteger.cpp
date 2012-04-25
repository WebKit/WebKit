/*
 * Copyright (C) Research In Motion Limited 2012. All rights reserved.
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
#include "SVGAnimatedIntegerOptionalInteger.h"

#include "SVGAnimateElement.h"
#include "SVGAnimatedInteger.h"
#include "SVGParserUtilities.h"

namespace WebCore {

SVGAnimatedIntegerOptionalIntegerAnimator::SVGAnimatedIntegerOptionalIntegerAnimator(SVGAnimationElement* animationElement, SVGElement* contextElement)
    : SVGAnimatedTypeAnimator(AnimatedIntegerOptionalInteger, animationElement, contextElement)
{
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedIntegerOptionalIntegerAnimator::constructFromString(const String& string)
{
    OwnPtr<SVGAnimatedType> animtedType = SVGAnimatedType::createIntegerOptionalInteger(new pair<int, int>);
    pair<int, int>& animatedInteger = animtedType->integerOptionalInteger();
    float firstNumber = 0;
    float secondNumber = 0;
    if (!parseNumberOptionalNumber(string, firstNumber, secondNumber)) {
        animatedInteger.first = 0;
        animatedInteger.second = 0;
    } else {
        animatedInteger.first = static_cast<int>(roundf(firstNumber));
        animatedInteger.second = static_cast<int>(roundf(secondNumber));
    }
    return animtedType.release();
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedIntegerOptionalIntegerAnimator::startAnimValAnimation(const Vector<SVGAnimatedProperty*>& properties)
{
    return SVGAnimatedType::createIntegerOptionalInteger(constructFromBaseValues<SVGAnimatedInteger, SVGAnimatedInteger>(properties));
}

void SVGAnimatedIntegerOptionalIntegerAnimator::stopAnimValAnimation(const Vector<SVGAnimatedProperty*>& properties)
{
    stopAnimValAnimationForTypes<SVGAnimatedInteger, SVGAnimatedInteger>(properties);
}

void SVGAnimatedIntegerOptionalIntegerAnimator::resetAnimValToBaseVal(const Vector<SVGAnimatedProperty*>& properties, SVGAnimatedType* type)
{
    resetFromBaseValues<SVGAnimatedInteger, SVGAnimatedInteger>(properties, type, &SVGAnimatedType::integerOptionalInteger);
}

void SVGAnimatedIntegerOptionalIntegerAnimator::animValWillChange(const Vector<SVGAnimatedProperty*>& properties)
{
    animValWillChangeForTypes<SVGAnimatedInteger, SVGAnimatedInteger>(properties);
}

void SVGAnimatedIntegerOptionalIntegerAnimator::animValDidChange(const Vector<SVGAnimatedProperty*>& properties)
{
    animValDidChangeForTypes<SVGAnimatedInteger, SVGAnimatedInteger>(properties);
}

void SVGAnimatedIntegerOptionalIntegerAnimator::addAnimatedTypes(SVGAnimatedType* from, SVGAnimatedType* to)
{
    ASSERT(from->type() == AnimatedIntegerOptionalInteger);
    ASSERT(from->type() == to->type());

    pair<int, int>& fromNumberPair = from->integerOptionalInteger();
    pair<int, int>& toNumberPair = to->integerOptionalInteger();

    toNumberPair.first += fromNumberPair.first;
    toNumberPair.second += fromNumberPair.second;
}

void SVGAnimatedIntegerOptionalIntegerAnimator::calculateAnimatedValue(float percentage, unsigned repeatCount,
                                                       OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, OwnPtr<SVGAnimatedType>& animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);

    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(m_animationElement);
    AnimationMode animationMode = animationElement->animationMode();

    // To animation uses contributions from the lower priority animations as the base value.
    pair<int, int>& fromNumberPair = from->integerOptionalInteger();
    pair<int, int>& animatedNumberPair = animated->integerOptionalInteger();
    if (animationMode == ToAnimation)
        fromNumberPair = animatedNumberPair;

    pair<int, int>& toNumberPair = to->integerOptionalInteger();
    SVGAnimatedIntegerAnimator::calculateAnimatedInteger(animationElement, percentage, repeatCount, animatedNumberPair.first, fromNumberPair.first, toNumberPair.first);
    SVGAnimatedIntegerAnimator::calculateAnimatedInteger(animationElement, percentage, repeatCount, animatedNumberPair.second, fromNumberPair.second, toNumberPair.second);
}

float SVGAnimatedIntegerOptionalIntegerAnimator::calculateDistance(const String&, const String&)
{
    // FIXME: Distance calculation is not possible for SVGIntegerOptionalInteger right now. We need the distance for every single value.
    return -1;
}

}

#endif // ENABLE(SVG)
