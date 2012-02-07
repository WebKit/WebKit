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

void SVGAnimatedColorAnimator::calculateFromAndToValues(OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, const String& fromString, const String& toString)
{
    ASSERT(m_contextElement);
    ASSERT(m_animationElement);
    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(m_animationElement);
    animationElement->determinePropertyValueTypes(fromString, toString);

    from = constructFromString(fromString);
    to = constructFromString(toString);
}

void SVGAnimatedColorAnimator::calculateFromAndByValues(OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, const String& fromString, const String& byString)
{
    ASSERT(m_contextElement);
    ASSERT(m_animationElement);
    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(m_animationElement);
    animationElement->determinePropertyValueTypes(fromString, byString);

    from = constructFromString(fromString);
    to = constructFromString(byString);

    to->color() = ColorDistance::addColorsAndClamp(from->color(), to->color());
}

void SVGAnimatedColorAnimator::calculateAnimatedValue(float percentage, unsigned,
                                                      OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, OwnPtr<SVGAnimatedType>& animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);
    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(m_animationElement);
    
    AnimationMode animationMode = animationElement->animationMode();
    Color& fromColor = from->color();
    Color& toColor = to->color();
    Color& animatedColor = animated->color();
    // To animation uses contributions from the lower priority animations as the base value.
    if (animationMode == ToAnimation)
        fromColor = animatedColor;
    
    // Replace 'currentColor' / 'inherit' by their computed property values.
    AnimatedPropertyValueType fromPropertyValueType = animationElement->fromPropertyValueType();
    if (fromPropertyValueType == CurrentColorValue)
        animationElement->adjustForCurrentColor(m_contextElement, fromColor);
    else if (fromPropertyValueType == InheritValue) {
        String fromColorString;
        animationElement->adjustForInheritance(m_contextElement, animationElement->attributeName(), fromColorString);
        fromColor = SVGColor::colorFromRGBColorString(fromColorString);
    }
    AnimatedPropertyValueType toPropertyValueType = animationElement->toPropertyValueType();
    if (toPropertyValueType == CurrentColorValue)
        animationElement->adjustForCurrentColor(m_contextElement, toColor);
    else if (toPropertyValueType == InheritValue) {
        String toColorString;
        animationElement->adjustForInheritance(m_contextElement, animationElement->attributeName(), toColorString);
        toColor = SVGColor::colorFromRGBColorString(toColorString);
    }
    
    Color color;
    if (animationElement->calcMode() == CalcModeDiscrete)
        color = percentage < 0.5 ? fromColor : toColor;
    else
        color = ColorDistance(fromColor, toColor).scaledDistance(percentage).addToColorAndClamp(fromColor);
    
    // FIXME: Accumulate colors.
    if (animationElement->isAdditive() && animationMode != ToAnimation)
        animatedColor = ColorDistance::addColorsAndClamp(animatedColor, color);
    else
        animatedColor = color;
    return;
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
