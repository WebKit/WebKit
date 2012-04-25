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
#include "SVGAnimatedRect.h"

#include "SVGAnimateElement.h"
#include "SVGParserUtilities.h"

namespace WebCore {

SVGAnimatedRectAnimator::SVGAnimatedRectAnimator(SVGAnimationElement* animationElement, SVGElement* contextElement)
    : SVGAnimatedTypeAnimator(AnimatedRect, animationElement, contextElement)
{
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedRectAnimator::constructFromString(const String& string)
{
    OwnPtr<SVGAnimatedType> animatedType = SVGAnimatedType::createRect(new FloatRect);
    parseRect(string, animatedType->rect());
    return animatedType.release();
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedRectAnimator::startAnimValAnimation(const Vector<SVGAnimatedProperty*>& properties)
{
    return SVGAnimatedType::createRect(constructFromBaseValue<SVGAnimatedRect>(properties));
}

void SVGAnimatedRectAnimator::stopAnimValAnimation(const Vector<SVGAnimatedProperty*>& properties)
{
    stopAnimValAnimationForType<SVGAnimatedRect>(properties);
}

void SVGAnimatedRectAnimator::resetAnimValToBaseVal(const Vector<SVGAnimatedProperty*>& properties, SVGAnimatedType* type)
{
    resetFromBaseValue<SVGAnimatedRect>(properties, type, &SVGAnimatedType::rect);
}

void SVGAnimatedRectAnimator::animValWillChange(const Vector<SVGAnimatedProperty*>& properties)
{
    animValWillChangeForType<SVGAnimatedRect>(properties);
}

void SVGAnimatedRectAnimator::animValDidChange(const Vector<SVGAnimatedProperty*>& properties)
{
    animValDidChangeForType<SVGAnimatedRect>(properties);
}

void SVGAnimatedRectAnimator::addAnimatedTypes(SVGAnimatedType* from, SVGAnimatedType* to)
{
    ASSERT(from->type() == AnimatedRect);
    ASSERT(from->type() == to->type());

    to->rect() += from->rect();
}

void SVGAnimatedRectAnimator::calculateAnimatedValue(float percentage, unsigned repeatCount, OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, OwnPtr<SVGAnimatedType>& animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);

    FloatRect& fromRect = from->rect();
    FloatRect& toRect = to->rect();
    FloatRect& animatedRect = animated->rect();
    m_animationElement->adjustFromToValues<FloatRect>(0, fromRect, toRect, animatedRect, percentage, m_contextElement);

    FloatRect newRect;    
    if (m_animationElement->calcMode() == CalcModeDiscrete)
        newRect = percentage < 0.5 ? fromRect : toRect;
    else
        newRect = FloatRect((toRect.x() - fromRect.x()) * percentage + fromRect.x(),
                            (toRect.y() - fromRect.y()) * percentage + fromRect.y(),
                            (toRect.width() - fromRect.width()) * percentage + fromRect.width(),
                            (toRect.height() - fromRect.height()) * percentage + fromRect.height());

    // FIXME: This is not correct for values animation. Right now we transform values-animation to multiple from-to-animations and
    // accumulate every single value to the previous one. But accumulation should just take into account after a complete cycle
    // of values-animaiton. See example at: http://www.w3.org/TR/2001/REC-smil-animation-20010904/#RepeatingAnim
    if (m_animationElement->isAccumulated() && repeatCount) {
        newRect += toRect;
        newRect.scale(repeatCount);
    }
    
    if (m_animationElement->isAdditive() && m_animationElement->animationMode() != ToAnimation)
        animatedRect += newRect;
    else
        animatedRect = newRect;
}

float SVGAnimatedRectAnimator::calculateDistance(const String&, const String&)
{
    // FIXME: Distance calculation is not possible for SVGRect right now. We need the distance of for every single value.
    return -1;
}
    
}

#endif // ENABLE(SVG)
