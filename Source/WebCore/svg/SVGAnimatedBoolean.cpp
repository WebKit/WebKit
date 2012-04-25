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
#include "SVGAnimatedBoolean.h"

#include "SVGAnimateElement.h"

namespace WebCore {

SVGAnimatedBooleanAnimator::SVGAnimatedBooleanAnimator(SVGAnimationElement* animationElement, SVGElement* contextElement)
    : SVGAnimatedTypeAnimator(AnimatedBoolean, animationElement, contextElement)
{
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedBooleanAnimator::constructFromString(const String& string)
{
    DEFINE_STATIC_LOCAL(const String, trueString, ("true"));
    OwnPtr<SVGAnimatedType> animtedType = SVGAnimatedType::createBoolean(new bool);
    animtedType->boolean() = string == trueString;
    return animtedType.release();
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedBooleanAnimator::startAnimValAnimation(const Vector<SVGAnimatedProperty*>& properties)
{
    return SVGAnimatedType::createBoolean(constructFromBaseValue<SVGAnimatedBoolean>(properties));
}

void SVGAnimatedBooleanAnimator::stopAnimValAnimation(const Vector<SVGAnimatedProperty*>& properties)
{
    stopAnimValAnimationForType<SVGAnimatedBoolean>(properties);
}

void SVGAnimatedBooleanAnimator::resetAnimValToBaseVal(const Vector<SVGAnimatedProperty*>& properties, SVGAnimatedType* type)
{
    resetFromBaseValue<SVGAnimatedBoolean>(properties, type, &SVGAnimatedType::boolean);
}

void SVGAnimatedBooleanAnimator::animValWillChange(const Vector<SVGAnimatedProperty*>& properties)
{
    animValWillChangeForType<SVGAnimatedBoolean>(properties);
}

void SVGAnimatedBooleanAnimator::animValDidChange(const Vector<SVGAnimatedProperty*>& properties)
{
    animValDidChangeForType<SVGAnimatedBoolean>(properties);
}

void SVGAnimatedBooleanAnimator::addAnimatedTypes(SVGAnimatedType* from, SVGAnimatedType* to)
{
    ASSERT_UNUSED(from, from->type() == AnimatedBoolean);
    ASSERT_UNUSED(to, from->type() == to->type());
}

void SVGAnimatedBooleanAnimator::calculateAnimatedValue(float percentage, unsigned, OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, OwnPtr<SVGAnimatedType>& animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);

    bool& fromBoolean = from->boolean();
    bool& toBoolean = to->boolean();
    bool& animatedBoolean = animated->boolean();
    m_animationElement->adjustFromToValues<bool>(0, fromBoolean, toBoolean, animatedBoolean, percentage, m_contextElement);

    AnimationMode animationMode = m_animationElement->animationMode();
    if ((animationMode == FromToAnimation && percentage > 0.5) || animationMode == ToAnimation || percentage == 1)
        animatedBoolean = bool(toBoolean);
    else
        animatedBoolean = bool(fromBoolean);
}

float SVGAnimatedBooleanAnimator::calculateDistance(const String&, const String&)
{
    // No paced animations for boolean.
    return -1;
}

}

#endif // ENABLE(SVG)
