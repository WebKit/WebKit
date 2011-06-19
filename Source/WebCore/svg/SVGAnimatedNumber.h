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

#ifndef SVGAnimatedNumber_h
#define SVGAnimatedNumber_h

#if ENABLE(SVG)
#include "SVGAnimateElement.h"
#include "SVGAnimatedPropertyMacros.h"
#include "SVGAnimatedStaticPropertyTearOff.h"

namespace WebCore {

typedef SVGAnimatedStaticPropertyTearOff<float> SVGAnimatedNumber;

// Helper macros to declare/define a SVGAnimatedNumber object
#define DECLARE_ANIMATED_NUMBER(UpperProperty, LowerProperty) \
DECLARE_ANIMATED_PROPERTY(SVGAnimatedNumber, float, UpperProperty, LowerProperty)

#define DEFINE_ANIMATED_NUMBER(OwnerType, DOMAttribute, UpperProperty, LowerProperty) \
DEFINE_ANIMATED_PROPERTY(OwnerType, DOMAttribute, DOMAttribute.localName(), SVGAnimatedNumber, float, UpperProperty, LowerProperty)

#define DEFINE_ANIMATED_NUMBER_MULTIPLE_WRAPPERS(OwnerType, DOMAttribute, SVGDOMAttributeIdentifier, UpperProperty, LowerProperty) \
DEFINE_ANIMATED_PROPERTY(OwnerType, DOMAttribute, SVGDOMAttributeIdentifier, SVGAnimatedNumber, float, UpperProperty, LowerProperty)

#if ENABLE(SVG_ANIMATION)
class SVGAnimatedNumberAnimator : public SVGAnimatedTypeAnimator {
    
public:
    SVGAnimatedNumberAnimator(SVGElement*, const QualifiedName&);
    virtual ~SVGAnimatedNumberAnimator() { }
    
    virtual PassOwnPtr<SVGAnimatedType> constructFromString(const String&);
    
    virtual void calculateFromAndToValues(OwnPtr<SVGAnimatedType>& fromValue, OwnPtr<SVGAnimatedType>& toValue, const String& fromString, const String& toString);
    virtual void calculateFromAndByValues(OwnPtr<SVGAnimatedType>& fromValue, OwnPtr<SVGAnimatedType>& toValue, const String& fromString, const String& byString);
    virtual void calculateAnimatedValue(SVGSMILElement*, float percentage, unsigned repeatCount,
                                        OwnPtr<SVGAnimatedType>& fromValue, OwnPtr<SVGAnimatedType>& toValue, OwnPtr<SVGAnimatedType>& animatedValue,
                                        bool fromPropertyInherits, bool toPropertyInherits);
    virtual float calculateDistance(SVGSMILElement*, const String& fromString, const String& toString);
};
} // namespace WebCore

#endif // ENABLE(SVG_ANIMATION)
#endif // ENABLE(SVG)
#endif
