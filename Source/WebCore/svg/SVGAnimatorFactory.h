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

#ifndef SVGAnimatorFactory_h
#define SVGAnimatorFactory_h

#include "SVGAnimatedAngle.h"
#include "SVGAnimatedBoolean.h"
#include "SVGAnimatedColor.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedInteger.h"
#include "SVGAnimatedIntegerOptionalInteger.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedLengthList.h"
#include "SVGAnimatedNumber.h"
#include "SVGAnimatedNumberList.h"
#include "SVGAnimatedNumberOptionalNumber.h"
#include "SVGAnimatedPath.h"
#include "SVGAnimatedPointList.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "SVGAnimatedRect.h"
#include "SVGAnimatedString.h"
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
            return std::make_unique<SVGAnimatedAngleAnimator>(animationElement, contextElement);
        case AnimatedBoolean:
            return std::make_unique<SVGAnimatedBooleanAnimator>(animationElement, contextElement);
        case AnimatedColor:
            return std::make_unique<SVGAnimatedColorAnimator>(animationElement, contextElement);
        case AnimatedEnumeration:
            return std::make_unique<SVGAnimatedEnumerationAnimator>(animationElement, contextElement);
        case AnimatedInteger:
            return std::make_unique<SVGAnimatedIntegerAnimator>(animationElement, contextElement);
        case AnimatedIntegerOptionalInteger:
            return std::make_unique<SVGAnimatedIntegerOptionalIntegerAnimator>(animationElement, contextElement);
        case AnimatedLength:
            return std::make_unique<SVGAnimatedLengthAnimator>(animationElement, contextElement);
        case AnimatedLengthList:
            return std::make_unique<SVGAnimatedLengthListAnimator>(animationElement, contextElement);
        case AnimatedNumber:
            return std::make_unique<SVGAnimatedNumberAnimator>(animationElement, contextElement);
        case AnimatedNumberList:
            return std::make_unique<SVGAnimatedNumberListAnimator>(animationElement, contextElement);
        case AnimatedNumberOptionalNumber:
            return std::make_unique<SVGAnimatedNumberOptionalNumberAnimator>(animationElement, contextElement);
        case AnimatedPath:
            return std::make_unique<SVGAnimatedPathAnimator>(animationElement, contextElement);
        case AnimatedPoints:
            return std::make_unique<SVGAnimatedPointListAnimator>(animationElement, contextElement);
        case AnimatedPreserveAspectRatio:
            return std::make_unique<SVGAnimatedPreserveAspectRatioAnimator>(animationElement, contextElement);
        case AnimatedRect:
            return std::make_unique<SVGAnimatedRectAnimator>(animationElement, contextElement);
        case AnimatedString:
            return std::make_unique<SVGAnimatedStringAnimator>(animationElement, contextElement);
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

#endif // SVGAnimatorFactory_h
