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
#include "SVGAnimatedPath.h"

#include "SVGAnimateElement.h"
#include "SVGPathParserFactory.h"

namespace WebCore {

SVGAnimatedPathAnimator::SVGAnimatedPathAnimator(SVGAnimationElement* animationElement, SVGElement* contextElement)
    : SVGAnimatedTypeAnimator(AnimatedPath, animationElement, contextElement)
{
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedPathAnimator::constructFromString(const String& string)
{
    bool result = false;
    return constructFromString(string, result);
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedPathAnimator::constructFromString(const String& string, bool& result)
{
    OwnPtr<SVGPathByteStream> byteStream = SVGPathByteStream::create();
    result = SVGPathParserFactory::self()->buildSVGPathByteStreamFromString(string, byteStream, UnalteredParsing);
    return SVGAnimatedType::createPath(byteStream.release());
}

void SVGAnimatedPathAnimator::calculateFromAndToValues(OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, const String& fromString, const String& toString)
{
    ASSERT(m_contextElement);
    ASSERT(m_animationElement);

    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(m_animationElement);
    AnimationMode animationMode = animationElement->animationMode();

    bool success = false;
    to = constructFromString(toString, success);

    // For to-animations the from number is calculated later.
    if (!success || animationMode == ToAnimation) {
        from = SVGAnimatedType::createPath(SVGPathByteStream::create());
        return;
    }

    from = constructFromString(fromString, success);
    if (success)
        return;

    from = SVGAnimatedType::createPath(SVGPathByteStream::create());
}

void SVGAnimatedPathAnimator::calculateFromAndByValues(OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& by, const String& fromString, const String& byString)
{
    // Fallback to from-to animation, since from-by animation does not make sense.
    calculateFromAndToValues(from, by, fromString, byString);
}

void SVGAnimatedPathAnimator::calculateAnimatedValue(float percentage, unsigned, OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, OwnPtr<SVGAnimatedType>& animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);

    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(m_animationElement);
    AnimationMode animationMode = animationElement->animationMode();

    SVGPathByteStream* toPath = to->path();
    ASSERT(toPath);

    SVGPathByteStream* fromPath = from->path();
    ASSERT(fromPath);

    SVGPathByteStream* animatedPath = animated->path();
    ASSERT(animatedPath);

    if (animationMode == ToAnimation)
        fromPath->initializeFrom(animatedPath);

    if (!percentage) {
        animatedPath->initializeFrom(fromPath);
        return;
    }

    if (percentage == 1) {
        animatedPath->initializeFrom(toPath);
        return;
    }

    OwnPtr<SVGPathByteStream> newAnimatedPath = adoptPtr(animatedPath);
    bool success = SVGPathParserFactory::self()->buildAnimatedSVGPathByteStream(fromPath, toPath, newAnimatedPath, percentage);
    animatedPath = newAnimatedPath.leakPtr();
    if (success)
        return;

    if ((animationMode == FromToAnimation && percentage > 0.5) || animationMode == ToAnimation)
        animatedPath->initializeFrom(toPath);
    else
        animatedPath->initializeFrom(fromPath);
}
   
float SVGAnimatedPathAnimator::calculateDistance(const String&, const String&)
{
    // FIXME: Support paced animations.
    return -1;
}

}

#endif // ENABLE(SVG)
