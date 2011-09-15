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
#include "SVGAnimatedPointList.h"

#include "SVGAnimateElement.h"
#include "SVGParserUtilities.h"
#include "SVGPointList.h"

namespace WebCore {

SVGAnimatedPointListAnimator::SVGAnimatedPointListAnimator(SVGAnimationElement* animationElement, SVGElement* contextElement)
    : SVGAnimatedTypeAnimator(AnimatedPoints, animationElement, contextElement)
{
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedPointListAnimator::constructFromString(const String& string)
{
    OwnPtr<SVGAnimatedType> animtedType = SVGAnimatedType::createPointList(new SVGPointList);
    pointsListFromSVGData(animtedType->pointList(), string);
    return animtedType.release();
}

void SVGAnimatedPointListAnimator::calculateFromAndToValues(OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, const String& fromString, const String& toString)
{
    from = constructFromString(fromString);
    to = constructFromString(toString);
}

void SVGAnimatedPointListAnimator::calculateFromAndByValues(OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, const String& fromString, const String& byString)
{    
    from = constructFromString(fromString);
    to = constructFromString(byString);
    
    SVGPointList& fromPointList = from->pointList();
    SVGPointList& toPointList = to->pointList();
    unsigned itemCount = fromPointList.size();
    if (!itemCount || itemCount != toPointList.size())
        return;
    for (unsigned n = 0; n < itemCount; ++n) {
        FloatPoint& to = toPointList.at(n);
        to += fromPointList.at(n);
    }
}

void SVGAnimatedPointListAnimator::calculateAnimatedValue(float percentage, unsigned,
                                                       OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, OwnPtr<SVGAnimatedType>& animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);
    
    SVGPointList& animatedPointList = animated->pointList();
    SVGPointList& fromPointList = from->pointList();
    SVGPointList& toPointList = to->pointList();
    if (!percentage)
        animatedPointList = fromPointList;
    else if (percentage == 1)
        animatedPointList = toPointList;
    else {
        animatedPointList.clear();
        if (!fromPointList.isEmpty() && !toPointList.isEmpty())
            SVGPointList::createAnimated(fromPointList, toPointList, animatedPointList, percentage);
            
        // Fall back to discrete animation if the points are not compatible
        AnimationMode animationMode = static_cast<SVGAnimateElement*>(m_animationElement)->animationMode();
        if (animatedPointList.isEmpty())
            animatedPointList = ((animationMode == FromToAnimation && percentage > 0.5) || animationMode == ToAnimation || percentage == 1) 
            ? toPointList : fromPointList;
    }
    return;
}

float SVGAnimatedPointListAnimator::calculateDistance(const String&, const String&)
{
    // FIXME: Distance calculation is not possible for SVGPointList right now. We need the distance of for every single value.
    return -1;
}

}

#endif // ENABLE(SVG)
