/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#pragma once

#include "SVGAnimatedStaticPropertyTearOff.h"
#include "SVGAnimatedTypeAnimator.h"
#include "SVGAttributeAccessor.h"

namespace WebCore {

class SVGAnimationElement;

using SVGAnimatedBoolean = SVGAnimatedStaticPropertyTearOff<bool>;
using SVGAnimatedBooleanAttribute = SVGAnimatedAttribute<SVGAnimatedBoolean>;

template<typename OwnerType>
using SVGAnimatedBooleanAttributeAccessor = SVGAnimatedAttributeAccessor<OwnerType, SVGAnimatedBooleanAttribute, AnimatedBoolean>;

class SVGAnimatedBooleanAnimator final : public SVGAnimatedTypeAnimator {
public:
    SVGAnimatedBooleanAnimator(SVGAnimationElement*, SVGElement*);

    std::unique_ptr<SVGAnimatedType> constructFromString(const String&) override;
    std::unique_ptr<SVGAnimatedType> startAnimValAnimation(const SVGElementAnimatedPropertyList&) override;
    void stopAnimValAnimation(const SVGElementAnimatedPropertyList&) override;
    void resetAnimValToBaseVal(const SVGElementAnimatedPropertyList&, SVGAnimatedType&) override;
    void animValWillChange(const SVGElementAnimatedPropertyList&) override;
    void animValDidChange(const SVGElementAnimatedPropertyList&) override;

    void addAnimatedTypes(SVGAnimatedType*, SVGAnimatedType*) override;
    void calculateAnimatedValue(float percentage, unsigned repeatCount, SVGAnimatedType*, SVGAnimatedType*, SVGAnimatedType*, SVGAnimatedType*) override;
    float calculateDistance(const String& fromString, const String& toString) override;
};

} // namespace WebCore
