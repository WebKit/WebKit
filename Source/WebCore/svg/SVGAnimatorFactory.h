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

#pragma once

#include "SVGAnimatedLength.h"
#include "SVGAnimatedLengthList.h"
#include "SVGAnimatedPath.h"
#include "SVGAnimatedTransformList.h"

namespace WebCore {

class SVGAnimationElement;

class SVGAnimatorFactory {
public:
    static std::unique_ptr<SVGAnimatedTypeAnimator> create(SVGAnimationElement* animationElement, SVGElement* contextElement, AnimatedPropertyType attributeType)
    {
        ASSERT(animationElement);
        ASSERT(contextElement);

        switch (attributeType) {
        case AnimatedAngle:
        case AnimatedBoolean:
        case AnimatedColor:
        case AnimatedEnumeration:
        case AnimatedInteger:
        case AnimatedIntegerOptionalInteger:
        case AnimatedNumber:
        case AnimatedNumberList:
        case AnimatedNumberOptionalNumber:
        case AnimatedPoints:
        case AnimatedPreserveAspectRatio:
        case AnimatedRect:
        case AnimatedString:
            return nullptr;

        case AnimatedLength:
            return std::make_unique<SVGAnimatedLengthAnimator>(animationElement, contextElement);
        case AnimatedLengthList:
            return std::make_unique<SVGAnimatedLengthListAnimator>(animationElement, contextElement);
        case AnimatedPath:
            return std::make_unique<SVGAnimatedPathAnimator>(animationElement, contextElement);
        case AnimatedTransformList:
            return std::make_unique<SVGAnimatedTransformListAnimator>(animationElement, contextElement);
        case AnimatedUnknown:
            break;
        }

        ASSERT_NOT_REACHED();
        return nullptr;
    }

private:
    SVGAnimatorFactory() { }
};

} // namespace WebCore
