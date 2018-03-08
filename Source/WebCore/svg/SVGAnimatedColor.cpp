/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "SVGAnimatedColor.h"

#include "CSSParser.h"
#include "RenderElement.h"
#include "SVGAnimateElementBase.h"

namespace WebCore {

SVGAnimatedColorAnimator::SVGAnimatedColorAnimator(SVGAnimationElement& animationElement, SVGElement& contextElement)
    : SVGAnimatedTypeAnimator(AnimatedColor, &animationElement, &contextElement)
{
}

std::unique_ptr<SVGAnimatedType> SVGAnimatedColorAnimator::constructFromString(const String& string)
{
    return SVGAnimatedType::create(SVGPropertyTraits<Color>::fromString(string));
}

void SVGAnimatedColorAnimator::addAnimatedTypes(SVGAnimatedType* from, SVGAnimatedType* to)
{
    ASSERT(from);
    ASSERT(to);
    ASSERT(from->type() == AnimatedColor);
    ASSERT(to->type() == AnimatedColor);

    // Ignores any alpha and sets alpha on result to 100% opaque.
    const auto& fromColor = from->as<Color>();
    auto& toColor = to->as<Color>();
    toColor = { roundAndClampColorChannel(toColor.red() + fromColor.red()),
        roundAndClampColorChannel(toColor.green() + fromColor.green()),
        roundAndClampColorChannel(toColor.blue() + fromColor.blue()) };
}

static inline Color currentColor(SVGElement& targetElement)
{
    RenderElement* targetRenderer = targetElement.renderer();
    if (!targetRenderer)
        return { };
    return targetRenderer->style().visitedDependentColor(CSSPropertyColor);
}

static Color parseColorFromString(SVGAnimationElement*, const String& string)
{
    return CSSParser::parseColor(string.stripWhiteSpace());
}

void SVGAnimatedColorAnimator::calculateAnimatedValue(float percentage, unsigned repeatCount, SVGAnimatedType* from, SVGAnimatedType* to, SVGAnimatedType* toAtEndOfDuration, SVGAnimatedType* animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);

    auto fromColor = (m_animationElement->animationMode() == ToAnimation ? animated : from)->as<Color>();
    auto toColor = to->as<Color>();

    // Apply CSS inheritance rules.
    m_animationElement->adjustForInheritance<Color>(parseColorFromString, m_animationElement->fromPropertyValueType(), fromColor, m_contextElement);
    m_animationElement->adjustForInheritance<Color>(parseColorFromString, m_animationElement->toPropertyValueType(), toColor, m_contextElement);

    // Apply <animateColor> rules.
    if (m_animationElement->fromPropertyValueType() == CurrentColorValue)
        fromColor = currentColor(*m_contextElement);
    if (m_animationElement->toPropertyValueType() == CurrentColorValue)
        toColor = currentColor(*m_contextElement);

    const auto& toAtEndOfDurationColor = toAtEndOfDuration->as<Color>();
    auto& animatedColor = animated->as<Color>();

    // FIXME: ExtendedColor - this will need to handle blending between colors in different color spaces,
    // as well as work with non [0-255] Colors.
    float red = animatedColor.red();
    m_animationElement->animateAdditiveNumber(percentage, repeatCount, fromColor.red(), toColor.red(), toAtEndOfDurationColor.red(), red);

    float green = animatedColor.green();
    m_animationElement->animateAdditiveNumber(percentage, repeatCount, fromColor.green(), toColor.green(), toAtEndOfDurationColor.green(), green);

    float blue = animatedColor.blue();
    m_animationElement->animateAdditiveNumber(percentage, repeatCount, fromColor.blue(), toColor.blue(), toAtEndOfDurationColor.blue(), blue);

    float alpha = animatedColor.alpha();
    m_animationElement->animateAdditiveNumber(percentage, repeatCount, fromColor.alpha(), toColor.alpha(), toAtEndOfDurationColor.alpha(), alpha);

    animatedColor = { roundAndClampColorChannel(red), roundAndClampColorChannel(green), roundAndClampColorChannel(blue), roundAndClampColorChannel(alpha) };
}

float SVGAnimatedColorAnimator::calculateDistance(const String& fromString, const String& toString)
{
    Color from = CSSParser::parseColor(fromString.stripWhiteSpace());
    if (!from.isValid())
        return -1;
    Color to = CSSParser::parseColor(toString.stripWhiteSpace());
    if (!to.isValid())
        return -1;
    float red = from.red() - to.red();
    float green = from.green() - to.green();
    float blue = from.blue() - to.blue();
    return sqrtf(red * red + green * green + blue * blue);
}

}
