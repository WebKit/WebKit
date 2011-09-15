/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef SVGAnimateElement_h
#define SVGAnimateElement_h

#if ENABLE(SVG)

#include "SVGAnimatedType.h"
#include "SVGAnimatedTypeAnimator.h"
#include "SVGAnimationElement.h"
#include <wtf/OwnPtr.h>

namespace WebCore {
    
// If we have 'currentColor' or 'inherit' as animation value, we need to grab the value during the animation
// since the value can be animated itself.
enum AnimatedPropertyValueType {
    RegularPropertyValue,
    CurrentColorValue,
    InheritValue
};

class SVGAnimateElement : public SVGAnimationElement {
public:
    static PassRefPtr<SVGAnimateElement> create(const QualifiedName&, Document*);

    virtual ~SVGAnimateElement();
    
    static void adjustForCurrentColor(SVGElement* targetElement, Color&);
    void adjustForInheritance(SVGElement* targetElement, const QualifiedName&, String& value);
    
    AnimatedPropertyType determineAnimatedPropertyType(SVGElement*) const;
    void determinePropertyValueTypes(const String&, const String&);
    
    AnimatedPropertyValueType fromPropertyValueType() { return m_fromPropertyValueType; }
    AnimatedPropertyValueType toPropertyValueType() { return m_toPropertyValueType; }

protected:
    SVGAnimateElement(const QualifiedName&, Document*);

    virtual void resetToBaseValue(const String&);
    virtual bool calculateFromAndToValues(const String& fromString, const String& toString);
    virtual bool calculateFromAndByValues(const String& fromString, const String& byString);
    virtual void calculateAnimatedValue(float percentage, unsigned repeat, SVGSMILElement* resultElement);
    virtual void applyResultsToTarget();
    virtual float calculateDistance(const String& fromString, const String& toString);

private:
    SVGAnimatedTypeAnimator* ensureAnimator();
    
    virtual bool hasValidAttributeType();
    AnimatedPropertyType m_animatedPropertyType;

    AnimatedPropertyValueType m_fromPropertyValueType;
    AnimatedPropertyValueType m_toPropertyValueType;

    OwnPtr<SVGAnimatedType> m_fromType;
    OwnPtr<SVGAnimatedType> m_toType;
    OwnPtr<SVGAnimatedType> m_animatedType;
    
    OwnPtr<SVGAnimatedTypeAnimator> m_animator;
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGAnimateElement_h
