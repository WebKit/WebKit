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
#include "SVGAnimatedLengthList.h"

#include "SVGAnimateElement.h"
#include "SVGAnimatedNumber.h"

namespace WebCore {

SVGAnimatedLengthListAnimator::SVGAnimatedLengthListAnimator(SVGAnimationElement* animationElement, SVGElement* contextElement)
    : SVGAnimatedTypeAnimator(AnimatedLengthList, animationElement, contextElement)
    , m_lengthMode(SVGLength::lengthModeForAnimatedLengthAttribute(animationElement->attributeName()))
{
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedLengthListAnimator::constructFromString(const String& string)
{
    OwnPtr<SVGAnimatedType> animateType = SVGAnimatedType::createLengthList(new SVGLengthList);
    animateType->lengthList().parse(string, m_lengthMode);
    return animateType.release();
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedLengthListAnimator::startAnimValAnimation(const Vector<SVGAnimatedProperty*>& properties)
{
    return SVGAnimatedType::createLengthList(constructFromBaseValue<SVGAnimatedLengthList>(properties));
}

void SVGAnimatedLengthListAnimator::stopAnimValAnimation(const Vector<SVGAnimatedProperty*>& properties)
{
    stopAnimValAnimationForType<SVGAnimatedLengthList>(properties);
}

void SVGAnimatedLengthListAnimator::resetAnimValToBaseVal(const Vector<SVGAnimatedProperty*>& properties, SVGAnimatedType* type)
{
    resetFromBaseValue<SVGAnimatedLengthList>(properties, type, &SVGAnimatedType::lengthList);
}

void SVGAnimatedLengthListAnimator::animValWillChange(const Vector<SVGAnimatedProperty*>& properties)
{
    animValWillChangeForType<SVGAnimatedLengthList>(properties);
}

void SVGAnimatedLengthListAnimator::animValDidChange(const Vector<SVGAnimatedProperty*>& properties)
{
    animValDidChangeForType<SVGAnimatedLengthList>(properties);
}

void SVGAnimatedLengthListAnimator::addAnimatedTypes(SVGAnimatedType* from, SVGAnimatedType* to)
{
    ASSERT(from->type() == AnimatedLengthList);
    ASSERT(from->type() == to->type());

    SVGLengthList& fromLengthList = from->lengthList();
    SVGLengthList& toLengthList = to->lengthList();

    unsigned fromLengthListSize = fromLengthList.size();
    if (!fromLengthListSize || fromLengthListSize != toLengthList.size())
        return;

    SVGLengthContext lengthContext(m_contextElement);
    ExceptionCode ec = 0;
    for (unsigned i = 0; i < fromLengthListSize; ++i) {
        toLengthList[i].setValue(toLengthList[i].value(lengthContext) + fromLengthList[i].value(lengthContext), lengthContext, ec);
        ASSERT(!ec);
    }
}

static SVGLengthList parseLengthListFromString(SVGAnimationElement* animationElement, const String& string)
{
    SVGLengthList lengthList;
    lengthList.parse(string, SVGLength::lengthModeForAnimatedLengthAttribute(animationElement->attributeName()));
    return lengthList;
}

void SVGAnimatedLengthListAnimator::calculateAnimatedValue(float percentage, unsigned repeatCount, OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, OwnPtr<SVGAnimatedType>& animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);

    SVGLengthList& fromLengthList = from->lengthList();
    SVGLengthList& toLengthList = to->lengthList();
    SVGLengthList& animatedLengthList = animated->lengthList();
    if (!m_animationElement->adjustFromToListValues<SVGLengthList>(parseLengthListFromString, fromLengthList, toLengthList, animatedLengthList, percentage, m_contextElement))
        return;

    unsigned fromLengthListSize = fromLengthList.size();
    unsigned toLengthListSize = toLengthList.size();

    SVGLengthContext lengthContext(m_contextElement);
    ExceptionCode ec = 0;
    for (unsigned i = 0; i < toLengthListSize; ++i) {
        float animatedNumber = animatedLengthList[i].value(lengthContext);
        SVGLengthType unitType = toLengthList[i].unitType();
        float effectiveFrom = 0;
        if (fromLengthListSize) {
            if (percentage < 0.5)
                unitType = fromLengthList[i].unitType();
            effectiveFrom = fromLengthList[i].value(lengthContext);
        }

        m_animationElement->animateAdditiveNumber(percentage, repeatCount, effectiveFrom, toLengthList[i].value(lengthContext), animatedNumber);
        animatedLengthList[i].setValue(lengthContext, animatedNumber, m_lengthMode, unitType, ec);
        ASSERT(!ec);
    }
}

float SVGAnimatedLengthListAnimator::calculateDistance(const String&, const String&)
{
    // FIXME: Distance calculation is not possible for SVGLengthList right now. We need the distance for every single value.
    return -1;
}

}

#endif // ENABLE(SVG)
