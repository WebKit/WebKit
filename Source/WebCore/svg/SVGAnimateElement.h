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
#include "SVGNames.h"

namespace WebCore {

class SVGAnimatedProperty;

class SVGAnimateElement : public SVGAnimationElement {
public:
    static PassRefPtr<SVGAnimateElement> create(const QualifiedName&, Document&);
    virtual ~SVGAnimateElement();

    AnimatedPropertyType determineAnimatedPropertyType(SVGElement*) const;

protected:
    SVGAnimateElement(const QualifiedName&, Document&);

    virtual void resetAnimatedType();
    virtual void clearAnimatedType(SVGElement* targetElement);

    virtual bool calculateToAtEndOfDurationValue(const String& toAtEndOfDurationString) override;
    virtual bool calculateFromAndToValues(const String& fromString, const String& toString) override;
    virtual bool calculateFromAndByValues(const String& fromString, const String& byString) override;
    virtual void calculateAnimatedValue(float percentage, unsigned repeatCount, SVGSMILElement* resultElement) override;
    virtual void applyResultsToTarget() override;
    virtual float calculateDistance(const String& fromString, const String& toString) override;
    virtual bool isAdditive() const override;

    virtual void setTargetElement(SVGElement*) override;
    virtual void setAttributeName(const QualifiedName&) override;

    AnimatedPropertyType m_animatedPropertyType;

private:
    void resetAnimatedPropertyType();
    SVGAnimatedTypeAnimator* ensureAnimator();
    bool animatedPropertyTypeSupportsAddition() const;

    virtual bool hasValidAttributeType() override;

    std::unique_ptr<SVGAnimatedType> m_fromType;
    std::unique_ptr<SVGAnimatedType> m_toType;
    std::unique_ptr<SVGAnimatedType> m_toAtEndOfDurationType;
    std::unique_ptr<SVGAnimatedType> m_animatedType;

    SVGElementAnimatedPropertyList m_animatedProperties;
    std::unique_ptr<SVGAnimatedTypeAnimator> m_animator;
};

void isSVGAnimateElement(const SVGAnimateElement&); // Catch unnecessary runtime check of type known at compile time.
bool isSVGAnimateElement(const Node&);
NODE_TYPE_CASTS(SVGAnimateElement)

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGAnimateElement_h
