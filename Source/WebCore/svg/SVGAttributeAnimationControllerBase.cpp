/*
 * Copyright (C) 2018-2019 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SVGAttributeAnimationControllerBase.h"

#include "SVGAnimationElement.h"
#include "SVGElement.h"

namespace WebCore {

SVGAttributeAnimationControllerBase::SVGAttributeAnimationControllerBase(SVGAnimationElement& animationElement, SVGElement& targetElement)
    : m_animationElement(animationElement)
    , m_targetElement(targetElement)
{
}

AnimatedPropertyType SVGAttributeAnimationControllerBase::determineAnimatedPropertyType(const SVGAnimationElement& animationElement, SVGElement& targetElement, const QualifiedName& attributeName)
{
    auto propertyTypes = targetElement.animatedPropertyTypesForAttribute(attributeName);
    if (propertyTypes.isEmpty())
        return AnimatedUnknown;

    ASSERT(propertyTypes.size() <= 2);
    AnimatedPropertyType type = propertyTypes[0];
    if (animationElement.hasTagName(SVGNames::animateColorTag) && type != AnimatedColor)
        return AnimatedUnknown;

    // Animations of transform lists are not allowed for <animate> or <set>
    // http://www.w3.org/TR/SVG/animate.html#AnimationAttributesAndProperties
    if (type == AnimatedTransformList && !animationElement.hasTagName(SVGNames::animateTransformTag))
        return AnimatedUnknown;

    // Fortunately there's just one special case needed here: SVGMarkerElements orientAttr, which
    // corresponds to SVGAnimatedAngle orientAngle and SVGAnimatedEnumeration orientType. We have to
    // figure out whose value to change here.
    if (targetElement.hasTagName(SVGNames::markerTag) && type == AnimatedAngle) {
        ASSERT(propertyTypes.size() == 2);
        ASSERT(propertyTypes[0] == AnimatedAngle);
        ASSERT(propertyTypes[1] == AnimatedEnumeration);
    } else if (propertyTypes.size() == 2)
        ASSERT(propertyTypes[0] == propertyTypes[1]);

    return type;
}

}
