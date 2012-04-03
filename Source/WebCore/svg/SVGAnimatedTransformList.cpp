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

#if ENABLE(SVG)
#include "SVGAnimatedTransformList.h"

#include "SVGAnimateTransformElement.h"
#include "SVGAnimatedNumber.h"
#include "SVGNames.h"
#include "SVGTransformDistance.h"

namespace WebCore {

SVGAnimatedTransformListAnimator::SVGAnimatedTransformListAnimator(SVGAnimationElement* animationElement, SVGElement* contextElement)
    : SVGAnimatedTypeAnimator(AnimatedTransformList, animationElement, contextElement)
{
    // Only <animateTransform> uses this animator, as <animate> doesn't allow to animate transform lists directly.
    ASSERT(animationElement->hasTagName(SVGNames::animateTransformTag));
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedTransformListAnimator::constructFromString(const String& string)
{
    OwnPtr<SVGAnimatedType> animatedType = SVGAnimatedType::createTransformList(new SVGTransformList);
    animatedType->transformList().parse(string);
    ASSERT(animatedType->transformList().size() <= 1);
    return animatedType.release();
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedTransformListAnimator::startAnimValAnimation(const Vector<SVGAnimatedProperty*>& properties)
{
    return SVGAnimatedType::createTransformList(constructFromBaseValue<SVGAnimatedTransformList>(properties));
}

void SVGAnimatedTransformListAnimator::stopAnimValAnimation(const Vector<SVGAnimatedProperty*>& properties)
{
    stopAnimValAnimationForType<SVGAnimatedTransformList>(properties);
}

inline PassOwnPtr<SVGAnimatedType> SVGAnimatedTransformListAnimator::constructFromString(SVGAnimateTransformElement* animateTransformElement, const String& string)
{
    ASSERT(animateTransformElement);
    return SVGAnimatedTransformListAnimator::constructFromString(SVGTransform::transformTypePrefixForParsing(animateTransformElement->transformType()) + string + ')');
}

void SVGAnimatedTransformListAnimator::resetAnimValToBaseVal(const Vector<SVGAnimatedProperty*>& properties, SVGAnimatedType* type)
{
    resetFromBaseValue<SVGAnimatedTransformList>(properties, type, &SVGAnimatedType::transformList);
}

void SVGAnimatedTransformListAnimator::animValWillChange(const Vector<SVGAnimatedProperty*>& properties)
{
    animValWillChangeForType<SVGAnimatedTransformList>(properties);
}

void SVGAnimatedTransformListAnimator::animValDidChange(const Vector<SVGAnimatedProperty*>& properties)
{
    animValDidChangeForType<SVGAnimatedTransformList>(properties);
}

void SVGAnimatedTransformListAnimator::calculateFromAndToValues(OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, const String& fromString, const String& toString)
{
    ASSERT(m_animationElement);
    SVGAnimateTransformElement* animateTransformElement = static_cast<SVGAnimateTransformElement*>(m_animationElement);

    from = constructFromString(animateTransformElement, fromString);
    to = constructFromString(animateTransformElement, toString);
}

void SVGAnimatedTransformListAnimator::calculateFromAndByValues(OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, const String& fromString, const String& byString)
{
    ASSERT(m_animationElement);
    SVGAnimateTransformElement* animateTransformElement = static_cast<SVGAnimateTransformElement*>(m_animationElement);

    from = constructFromString(animateTransformElement, fromString);
    to = constructFromString(animateTransformElement, byString);

    SVGTransformList& fromTransformList = from->transformList();
    SVGTransformList& toTransformList = to->transformList();
    unsigned itemsCount = fromTransformList.size();
    if (!itemsCount || itemsCount != toTransformList.size())
        return;

    ASSERT(itemsCount == 1);
    toTransformList[0] = SVGTransformDistance::addSVGTransforms(fromTransformList[0], toTransformList[0]);
}

void SVGAnimatedTransformListAnimator::calculateAnimatedValue(float percentage, unsigned repeatCount,
                                                       OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, OwnPtr<SVGAnimatedType>& animated)
{
    ASSERT(m_animationElement);

    // Spec: To animations provide specific functionality to get a smooth change from the underlying value to the
    // ‘to’ attribute value, which conflicts mathematically with the requirement for additive transform animations
    // to be post-multiplied. As a consequence, in SVG 1.1 the behavior of to animations for ‘animateTransform’ is undefined.
    // FIXME: This is not taken into account yet.
    SVGTransformList& fromTransformList = from->transformList();
    SVGTransformList& toTransformList = to->transformList();
    ASSERT(fromTransformList.size() <= 1);
    ASSERT(toTransformList.size() <= 1);
    ASSERT(fromTransformList[0].type() == toTransformList[0].type());

    SVGTransform fromTransform;
    SVGTransform toTransform;
    if (!toTransformList.isEmpty() && !fromTransformList.isEmpty()) {
        fromTransform = fromTransformList[0];
        toTransform = toTransformList[0];
        ASSERT(fromTransform.type() == toTransform.type());
    }

    if (m_animationElement->calcMode() == CalcModeDiscrete)
        percentage = percentage < 0.5 ? 0 : 1;

    if (m_animationElement->isAccumulated() && repeatCount)
        percentage += repeatCount;

    SVGTransformList& animatedTransformList = animated->transformList();
    if (!m_animationElement->isAdditive())
        animatedTransformList.clear();

    animatedTransformList.append(SVGTransformDistance(fromTransform, toTransform).scaledDistance(percentage).addToSVGTransform(fromTransform));
}

float SVGAnimatedTransformListAnimator::calculateDistance(const String& fromString, const String& toString)
{
    ASSERT(m_animationElement);
    SVGAnimateTransformElement* animateTransformElement = static_cast<SVGAnimateTransformElement*>(m_animationElement);

    // FIXME: This is not correct in all cases. The spec demands that each component (translate x and y for example)
    // is paced separately. To implement this we need to treat each component as individual animation everywhere.
    OwnPtr<SVGAnimatedType> from = constructFromString(animateTransformElement, fromString);
    OwnPtr<SVGAnimatedType> to = constructFromString(animateTransformElement, toString);

    SVGTransformList& fromTransformList = from->transformList();
    SVGTransformList& toTransformList = to->transformList();
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

#endif // ENABLE(SVG)
