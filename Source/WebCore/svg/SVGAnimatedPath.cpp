/*
 * Copyright (C) Research In Motion Limited 2011, 2012. All rights reserved.
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
#include "SVGAnimatedPathSegListPropertyTearOff.h"
#include "SVGPathParserFactory.h"

namespace WebCore {

SVGAnimatedPathAnimator::SVGAnimatedPathAnimator(SVGAnimationElement* animationElement, SVGElement* contextElement)
    : SVGAnimatedTypeAnimator(AnimatedPath, animationElement, contextElement)
{
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedPathAnimator::constructFromString(const String& string)
{
    OwnPtr<SVGPathByteStream> byteStream = SVGPathByteStream::create();
    SVGPathParserFactory::self()->buildSVGPathByteStreamFromString(string, byteStream.get(), UnalteredParsing);
    return SVGAnimatedType::createPath(byteStream.release());
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedPathAnimator::startAnimValAnimation(const Vector<SVGAnimatedProperty*>& properties)
{
    ASSERT(properties.size() == 1);
    SVGAnimatedPathSegListPropertyTearOff* property = castAnimatedPropertyToActualType<SVGAnimatedPathSegListPropertyTearOff>(properties[0]);
    const SVGPathSegList& baseValue = property->currentBaseValue();

    // Build initial path byte stream.
    OwnPtr<SVGPathByteStream> byteStream = SVGPathByteStream::create();
    SVGPathParserFactory::self()->buildSVGPathByteStreamFromSVGPathSegList(baseValue, byteStream.get(), UnalteredParsing);

    Vector<SVGAnimatedPathSegListPropertyTearOff*> result;
    result.append(property);

    SVGElementInstance::InstanceUpdateBlocker blocker(property->contextElement());
    collectAnimatedPropertiesFromInstances(result, 0, property->contextElement(), property->attributeName());

    size_t resultSize = result.size();
    for (size_t i = 0; i < resultSize; ++i)
        result[i]->animationStarted(byteStream.get(), &baseValue);

    return SVGAnimatedType::createPath(byteStream.release());
}

void SVGAnimatedPathAnimator::stopAnimValAnimation(const Vector<SVGAnimatedProperty*>& properties)
{
    stopAnimValAnimationForType<SVGAnimatedPathSegListPropertyTearOff>(properties);
}

void SVGAnimatedPathAnimator::resetAnimValToBaseVal(const Vector<SVGAnimatedProperty*>& properties, SVGAnimatedType* type)
{
    ASSERT(properties.size() == 1);
    ASSERT(type);
    ASSERT(type->type() == m_type);
    const SVGPathSegList& baseValue = castAnimatedPropertyToActualType<SVGAnimatedPathSegListPropertyTearOff>(properties[0])->currentBaseValue();
    SVGPathParserFactory::self()->buildSVGPathByteStreamFromSVGPathSegList(baseValue, type->path(), UnalteredParsing);
}

void SVGAnimatedPathAnimator::animValWillChange(const Vector<SVGAnimatedProperty*>& properties)
{
    animValWillChangeForType<SVGAnimatedPathSegListPropertyTearOff>(properties);
}

void SVGAnimatedPathAnimator::animValDidChange(const Vector<SVGAnimatedProperty*>& properties)
{
    animValDidChangeForType<SVGAnimatedPathSegListPropertyTearOff>(properties);
}

void SVGAnimatedPathAnimator::addAnimatedTypes(SVGAnimatedType* from, SVGAnimatedType* to)
{
    ASSERT_UNUSED(from, from->type() == AnimatedPath);
    ASSERT_UNUSED(to, from->type() == to->type());

    // FIXME: Add additive support.
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

    bool success = SVGPathParserFactory::self()->buildAnimatedSVGPathByteStream(fromPath, toPath, animatedPath, percentage);
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
