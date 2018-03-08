/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include "SVGAnimatedTransformList.h"

#include "SVGAnimateTransformElement.h"
#include "SVGAnimatedNumber.h"
#include "SVGNames.h"
#include "SVGTransformDistance.h"

namespace WebCore {

SVGAnimatedTransformListAnimator::SVGAnimatedTransformListAnimator(SVGAnimationElement* animationElement, SVGElement* contextElement)
    : SVGAnimatedTypeAnimator(AnimatedTransformList, animationElement, contextElement)
    , m_transformTypeString(SVGTransformValue::transformTypePrefixForParsing(downcast<SVGAnimateTransformElement>(animationElement)->transformType()))
{
    // Only <animateTransform> uses this animator, as <animate> doesn't allow to animate transform lists directly.
    ASSERT(animationElement->hasTagName(SVGNames::animateTransformTag));
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedTransformListAnimator::constructFromString(const String& string)
{
    return SVGAnimatedType::create(SVGPropertyTraits<SVGTransformListValues>::fromString(m_transformTypeString + string + ')'));
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedTransformListAnimator::startAnimValAnimation(const SVGElementAnimatedPropertyList& animatedTypes)
{
    return constructFromBaseValue<SVGAnimatedTransformList>(animatedTypes);
}

void SVGAnimatedTransformListAnimator::stopAnimValAnimation(const SVGElementAnimatedPropertyList& animatedTypes)
{
    stopAnimValAnimationForType<SVGAnimatedTransformList>(animatedTypes);
}

void SVGAnimatedTransformListAnimator::resetAnimValToBaseVal(const SVGElementAnimatedPropertyList& animatedTypes, SVGAnimatedType& type)
{
    resetFromBaseValue<SVGAnimatedTransformList>(animatedTypes, type);
}

void SVGAnimatedTransformListAnimator::animValWillChange(const SVGElementAnimatedPropertyList& animatedTypes)
{
    animValWillChangeForType<SVGAnimatedTransformList>(animatedTypes);
}

void SVGAnimatedTransformListAnimator::animValDidChange(const SVGElementAnimatedPropertyList& animatedTypes)
{
    animValDidChangeForType<SVGAnimatedTransformList>(animatedTypes);
}

void SVGAnimatedTransformListAnimator::addAnimatedTypes(SVGAnimatedType* from, SVGAnimatedType* to)
{
    ASSERT(from->type() == AnimatedTransformList);
    ASSERT(from->type() == to->type());

    const auto& fromTransformList = from->as<SVGTransformListValues>();
    auto& toTransformList = to->as<SVGTransformListValues>();
    auto fromTransformListSize = fromTransformList.size();
    if (!fromTransformListSize || fromTransformListSize != toTransformList.size())
        return;

    ASSERT(fromTransformListSize == 1);
    const auto& fromTransform = fromTransformList[0];
    auto& toTransform = toTransformList[0];

    ASSERT(fromTransform.type() == toTransform.type());
    toTransform = SVGTransformDistance::addSVGTransforms(fromTransform, toTransform);
}

void SVGAnimatedTransformListAnimator::calculateAnimatedValue(float percentage, unsigned repeatCount, SVGAnimatedType* from, SVGAnimatedType* to, SVGAnimatedType* toAtEndOfDuration, SVGAnimatedType* animated)
{
    ASSERT(m_animationElement);

    // Spec: To animations provide specific functionality to get a smooth change from the underlying value to the
    // ‘to’ attribute value, which conflicts mathematically with the requirement for additive transform animations
    // to be post-multiplied. As a consequence, in SVG 1.1 the behavior of to animations for ‘animateTransform’ is undefined.
    const auto& fromTransformList = from->as<SVGTransformListValues>();
    const auto& toTransformList = to->as<SVGTransformListValues>();
    const auto& toAtEndOfDurationTransformList = toAtEndOfDuration->as<SVGTransformListValues>();
    auto& animatedTransformList = animated->as<SVGTransformListValues>();

    // Pass false to 'resizeAnimatedListIfNeeded' here, as the special post-multiplication behavior of <animateTransform> needs to be respected below.
    if (!m_animationElement->adjustFromToListValues<SVGTransformListValues>(fromTransformList, toTransformList, animatedTransformList, percentage, false))
        return;

    // Never resize the animatedTransformList to the toTransformList size, instead either clear the list or append to it.
    if (!animatedTransformList.isEmpty() && (!m_animationElement->isAdditive() || m_animationElement->animationMode() == ToAnimation))
        animatedTransformList.clear();

    auto fromTransformListSize = fromTransformList.size();
    const auto& toTransform = toTransformList[0];
    const auto effectiveFrom = fromTransformListSize ? fromTransformList[0] : SVGTransformValue(toTransform.type(), SVGTransformValue::ConstructZeroTransform);
    auto currentTransform = SVGTransformDistance(effectiveFrom, toTransform).scaledDistance(percentage).addToSVGTransform(effectiveFrom);
    if (m_animationElement->isAccumulated() && repeatCount) {
        const auto effectiveToAtEnd = toAtEndOfDurationTransformList.size() ? toAtEndOfDurationTransformList[0] : SVGTransformValue(toTransform.type(), SVGTransformValue::ConstructZeroTransform);
        animatedTransformList.append(SVGTransformDistance::addSVGTransforms(currentTransform, effectiveToAtEnd, repeatCount));
    } else
        animatedTransformList.append(currentTransform);
}

float SVGAnimatedTransformListAnimator::calculateDistance(const String& fromString, const String& toString)
{
    ASSERT(m_animationElement);

    // FIXME: This is not correct in all cases. The spec demands that each component (translate x and y for example)
    // is paced separately. To implement this we need to treat each component as individual animation everywhere.
    auto from = constructFromString(fromString);
    auto to = constructFromString(toString);

    const auto& fromTransformList = from->as<SVGTransformListValues>();
    const auto& toTransformList = to->as<SVGTransformListValues>();
    unsigned itemsCount = fromTransformList.size();
    if (!itemsCount || itemsCount != toTransformList.size())
        return -1;

    ASSERT(itemsCount == 1);
    if (fromTransformList[0].type() != toTransformList[0].type())
        return -1;

    // Spec: http://www.w3.org/TR/SVG/animate.html#complexDistances
    // Paced animations assume a notion of distance between the various animation values defined by the ‘to’, ‘from’, ‘by’ and ‘values’ attributes.
    // Distance is defined only for scalar types (such as <length>), colors and the subset of transformation types that are supported by ‘animateTransform’.
    return SVGTransformDistance(fromTransformList[0], toTransformList[0]).distance();
}

}
