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

#ifndef SVGAnimatedAngle_h
#define SVGAnimatedAngle_h

#if ENABLE(SVG)
#include "SVGAngle.h"
#include "SVGAnimateElement.h"
#include "SVGAnimatedPropertyTearOff.h"

namespace WebCore {

typedef SVGAnimatedPropertyTearOff<SVGAngle> SVGAnimatedAngle;

// Helper macros to declare/define a SVGAnimatedAngle object
#define DECLARE_ANIMATED_ANGLE(UpperProperty, LowerProperty) \
DECLARE_ANIMATED_PROPERTY(SVGAnimatedAngle, SVGAngle, UpperProperty, LowerProperty)

#define DEFINE_ANIMATED_ANGLE_MULTIPLE_WRAPPERS(OwnerType, DOMAttribute, SVGDOMAttributeIdentifier, UpperProperty, LowerProperty) \
DEFINE_ANIMATED_PROPERTY(OwnerType, DOMAttribute, SVGDOMAttributeIdentifier, SVGAnimatedAngle, SVGAngle, UpperProperty, LowerProperty)

#if ENABLE(SVG_ANIMATION)
class SVGAnimatedAngleAnimator : public SVGAnimatedTypeAnimator {
    
public:
    SVGAnimatedAngleAnimator(SVGElement* contextElement, const QualifiedName&);
    virtual ~SVGAnimatedAngleAnimator() { }
    
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
