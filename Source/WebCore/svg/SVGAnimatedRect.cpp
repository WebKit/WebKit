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

PassOwnPtr<SVGAnimatedType> SVGAnimatedRectAnimator::startAnimValAnimation(const SVGElementAnimatedPropertyList& animatedTypes)
{
    return SVGAnimatedType::createRect(constructFromBaseValue<SVGAnimatedRect>(animatedTypes));
}

void SVGAnimatedRectAnimator::stopAnimValAnimation(const SVGElementAnimatedPropertyList& animatedTypes)
{
    stopAnimValAnimationForType<SVGAnimatedRect>(animatedTypes);
}

void SVGAnimatedRectAnimator::resetAnimValToBaseVal(const SVGElementAnimatedPropertyList& animatedTypes, SVGAnimatedType* type)
{
    resetFromBaseValue<SVGAnimatedRect>(animatedTypes, type, &SVGAnimatedType::rect);
}

void SVGAnimatedRectAnimator::animValWillChange(const SVGElementAnimatedPropertyList& animatedTypes)
{
    animValWillChangeForType<SVGAnimatedRect>(animatedTypes);
}

void SVGAnimatedRectAnimator::animValDidChange(const SVGElementAnimatedPropertyList& animatedTypes)
{
    animValDidChangeForType<SVGAnimatedRect>(animatedTypes);
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

    float animatedX = animatedRect.x();
    float animatedY = animatedRect.y();
    float animatedWidth = animatedRect.width();
    float animatedHeight = animatedRect.height();
    m_animationElement->animateAdditiveNumber(percentage, repeatCount, fromRect.x(), toRect.x(), animatedX);
    m_animationElement->animateAdditiveNumber(percentage, repeatCount, fromRect.y(), toRect.y(), animatedY);
    m_animationElement->animateAdditiveNumber(percentage, repeatCount, fromRect.width(), toRect.width(), animatedWidth);
    m_animationElement->animateAdditiveNumber(percentage, repeatCount, fromRect.height(), toRect.height(), animatedHeight);

    animatedRect = FloatRect(animatedX, animatedY, animatedWidth, animatedHeight);
}

float SVGAnimatedRectAnimator::calculateDistance(const String&, const String&)
{
    // FIXME: Distance calculation is not possible for SVGRect right now. We need the distance of for every single value.
    return -1;
}
    
}

#endif // ENABLE(SVG)
