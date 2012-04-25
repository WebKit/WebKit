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

#if ENABLE(SVG)
#include "SVGAnimatedPreserveAspectRatio.h"

#include "SVGAnimateElement.h"

namespace WebCore {
    
SVGAnimatedPreserveAspectRatioAnimator::SVGAnimatedPreserveAspectRatioAnimator(SVGAnimationElement* animationElement, SVGElement* contextElement)
    : SVGAnimatedTypeAnimator(AnimatedPreserveAspectRatio, animationElement, contextElement)
{
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedPreserveAspectRatioAnimator::constructFromString(const String& string)
{
    OwnPtr<SVGAnimatedType> animatedType = SVGAnimatedType::createPreserveAspectRatio(new SVGPreserveAspectRatio);
    animatedType->preserveAspectRatio().parse(string);
    return animatedType.release();
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedPreserveAspectRatioAnimator::startAnimValAnimation(const Vector<SVGAnimatedProperty*>& properties)
{
    return SVGAnimatedType::createPreserveAspectRatio(constructFromBaseValue<SVGAnimatedPreserveAspectRatio>(properties));
}

void SVGAnimatedPreserveAspectRatioAnimator::stopAnimValAnimation(const Vector<SVGAnimatedProperty*>& properties)
{
    stopAnimValAnimationForType<SVGAnimatedPreserveAspectRatio>(properties);
}

void SVGAnimatedPreserveAspectRatioAnimator::resetAnimValToBaseVal(const Vector<SVGAnimatedProperty*>& properties, SVGAnimatedType* type)
{
    resetFromBaseValue<SVGAnimatedPreserveAspectRatio>(properties, type, &SVGAnimatedType::preserveAspectRatio);
}

void SVGAnimatedPreserveAspectRatioAnimator::animValWillChange(const Vector<SVGAnimatedProperty*>& properties)
{
    animValWillChangeForType<SVGAnimatedPreserveAspectRatio>(properties);
}

void SVGAnimatedPreserveAspectRatioAnimator::animValDidChange(const Vector<SVGAnimatedProperty*>& properties)
{
    animValDidChangeForType<SVGAnimatedPreserveAspectRatio>(properties);
}

void SVGAnimatedPreserveAspectRatioAnimator::addAnimatedTypes(SVGAnimatedType* from, SVGAnimatedType* to)
{
    ASSERT_UNUSED(from, from->type() == AnimatedPreserveAspectRatio);
    ASSERT_UNUSED(to, from->type() == to->type());
}

void SVGAnimatedPreserveAspectRatioAnimator::calculateAnimatedValue(float percentage, unsigned,
                                                       OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, OwnPtr<SVGAnimatedType>& animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);
    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(m_animationElement);
    
    AnimationMode animationMode = animationElement->animationMode();
    SVGPreserveAspectRatio& animateString = animated->preserveAspectRatio();
    if ((animationMode == FromToAnimation && percentage > 0.5) || animationMode == ToAnimation || percentage == 1)
        animateString = to->preserveAspectRatio();
    else
        animateString = from->preserveAspectRatio();
}

float SVGAnimatedPreserveAspectRatioAnimator::calculateDistance(const String&, const String&)
{
    // No paced animations for SVGPreserveAspectRatio.
    return -1;
}

}

#endif // ENABLE(SVG)
