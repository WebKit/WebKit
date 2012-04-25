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
#include "SVGAnimatedColor.h"

#include "ColorDistance.h"
#include "RenderObject.h"
#include "SVGAnimateElement.h"
#include "SVGColor.h"

namespace WebCore {

SVGAnimatedColorAnimator::SVGAnimatedColorAnimator(SVGAnimationElement* animationElement, SVGElement* contextElement)
    : SVGAnimatedTypeAnimator(AnimatedColor, animationElement, contextElement)
{
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedColorAnimator::constructFromString(const String& string)
{
    OwnPtr<SVGAnimatedType> animtedType = SVGAnimatedType::createColor(new Color);
    animtedType->color() = string.isEmpty() ? Color() : SVGColor::colorFromRGBColorString(string);
    return animtedType.release();
}

void SVGAnimatedColorAnimator::addAnimatedTypes(SVGAnimatedType* from, SVGAnimatedType* to)
{
    ASSERT(from->type() == AnimatedColor);
    ASSERT(from->type() == to->type());

    // FIXME: We clamp too early.
    to->color() = ColorDistance::addColorsAndClamp(from->color(), to->color());
}

static inline void adjustForCurrentColor(SVGElement* targetElement, Color& color)
{
    ASSERT(targetElement);

    if (RenderObject* targetRenderer = targetElement->renderer())
        color = targetRenderer->style()->visitedDependentColor(CSSPropertyColor);
    else
        color = Color();
}

static Color parseColorFromString(SVGAnimationElement*, const String& string)
{
    return SVGColor::colorFromRGBColorString(string);
}

void SVGAnimatedColorAnimator::calculateAnimatedValue(float percentage, unsigned, OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, OwnPtr<SVGAnimatedType>& animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);
    
    Color& fromColor = from->color();
    Color& toColor = to->color();
    Color& animatedColor = animated->color();
    m_animationElement->adjustFromToValues<Color>(parseColorFromString, fromColor, toColor, animatedColor, percentage, m_contextElement);

    // Apply <animateColor> rules.
    if (m_animationElement->fromPropertyValueType() == CurrentColorValue)
        adjustForCurrentColor(m_contextElement, fromColor);
    if (m_animationElement->toPropertyValueType() == CurrentColorValue)
        adjustForCurrentColor(m_contextElement, toColor);

    Color color;
    if (m_animationElement->calcMode() == CalcModeDiscrete)
        color = percentage < 0.5 ? fromColor : toColor;
    else
        color = ColorDistance(fromColor, toColor).scaledDistance(percentage).addToColorAndClamp(fromColor);
    
    // FIXME: Accumulate colors.
    if (m_animationElement->isAdditive() && m_animationElement->animationMode() != ToAnimation)
        animatedColor = ColorDistance::addColorsAndClamp(animatedColor, color);
    else
        animatedColor = color;
}

float SVGAnimatedColorAnimator::calculateDistance(const String& fromString, const String& toString)
{
    ASSERT(m_contextElement);
    Color from = SVGColor::colorFromRGBColorString(fromString);
    if (!from.isValid())
        return -1;
    Color to = SVGColor::colorFromRGBColorString(toString);
    if (!to.isValid())
        return -1;
    return ColorDistance(from, to).distance();
}

}

#endif // ENABLE(SVG)
