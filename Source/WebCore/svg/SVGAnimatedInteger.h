/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef SVGAnimatedInteger_h
#define SVGAnimatedInteger_h

#if ENABLE(SVG)
#include "SVGAnimatedPropertyMacros.h"
#include "SVGAnimatedStaticPropertyTearOff.h"
#include "SVGAnimatedTypeAnimator.h"

namespace WebCore {

typedef SVGAnimatedStaticPropertyTearOff<int> SVGAnimatedInteger;

// Helper macros to declare/define a SVGAnimatedInteger object
#define DECLARE_ANIMATED_INTEGER(UpperProperty, LowerProperty) \
DECLARE_ANIMATED_PROPERTY(SVGAnimatedInteger, int, UpperProperty, LowerProperty)

#define DEFINE_ANIMATED_INTEGER(OwnerType, DOMAttribute, UpperProperty, LowerProperty) \
DEFINE_ANIMATED_PROPERTY(AnimatedInteger, OwnerType, DOMAttribute, DOMAttribute.localName(), UpperProperty, LowerProperty)

#define DEFINE_ANIMATED_INTEGER_MULTIPLE_WRAPPERS(OwnerType, DOMAttribute, SVGDOMAttributeIdentifier, UpperProperty, LowerProperty) \
DEFINE_ANIMATED_PROPERTY(AnimatedIntegerOptionalInteger, OwnerType, DOMAttribute, SVGDOMAttributeIdentifier, UpperProperty, LowerProperty)

class SVGAnimationElement;

class SVGAnimatedIntegerAnimator FINAL : public SVGAnimatedTypeAnimator {
public:
    SVGAnimatedIntegerAnimator(SVGAnimationElement*, SVGElement*);

    static void calculateAnimatedInteger(SVGAnimationElement*, float percentage, unsigned repeatCount, int fromInteger, int toInteger, int toAtEndOfDurationInteger, int& animatedInteger);

    virtual std::unique_ptr<SVGAnimatedType> constructFromString(const String&) override;
    virtual std::unique_ptr<SVGAnimatedType> startAnimValAnimation(const SVGElementAnimatedPropertyList&) override;
    virtual void stopAnimValAnimation(const SVGElementAnimatedPropertyList&) override;
    virtual void resetAnimValToBaseVal(const SVGElementAnimatedPropertyList&, SVGAnimatedType*) override;
    virtual void animValWillChange(const SVGElementAnimatedPropertyList&) override;
    virtual void animValDidChange(const SVGElementAnimatedPropertyList&) override;

    virtual void addAnimatedTypes(SVGAnimatedType*, SVGAnimatedType*) override;
    virtual void calculateAnimatedValue(float percentage, unsigned repeatCount, SVGAnimatedType*, SVGAnimatedType*, SVGAnimatedType*, SVGAnimatedType*) override;
    virtual float calculateDistance(const String& fromString, const String& toString) override;
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
