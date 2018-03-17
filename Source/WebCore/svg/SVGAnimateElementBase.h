/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
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

#include "SVGAnimatedType.h"
#include "SVGAnimatedTypeAnimator.h"
#include "SVGAnimationElement.h"
#include "SVGNames.h"

namespace WebCore {

class SVGAnimateElementBase : public SVGAnimationElement {
    WTF_MAKE_ISO_ALLOCATED(SVGAnimateElementBase);
public:
    virtual ~SVGAnimateElementBase();

    AnimatedPropertyType determineAnimatedPropertyType(SVGElement&) const;

protected:
    SVGAnimateElementBase(const QualifiedName&, Document&);

    void resetAnimatedType() override;
    void clearAnimatedType(SVGElement* targetElement) override;

    bool calculateToAtEndOfDurationValue(const String& toAtEndOfDurationString) override;
    bool calculateFromAndToValues(const String& fromString, const String& toString) override;
    bool calculateFromAndByValues(const String& fromString, const String& byString) override;
    void calculateAnimatedValue(float percentage, unsigned repeatCount, SVGSMILElement* resultElement) override;
    void applyResultsToTarget() override;
    float calculateDistance(const String& fromString, const String& toString) override;
    bool isAdditive() const override;

    void setTargetElement(SVGElement*) override;
    void setAttributeName(const QualifiedName&) override;
    void resetAnimatedPropertyType() override;

    AnimatedPropertyType m_animatedPropertyType;

private:
    SVGAnimatedTypeAnimator* ensureAnimator();
    bool animatedPropertyTypeSupportsAddition() const;

    bool hasValidAttributeType() override;

    std::unique_ptr<SVGAnimatedType> m_fromType;
    std::unique_ptr<SVGAnimatedType> m_toType;
    std::unique_ptr<SVGAnimatedType> m_toAtEndOfDurationType;
    std::unique_ptr<SVGAnimatedType> m_animatedType;

    SVGElementAnimatedPropertyList m_animatedProperties;
    std::unique_ptr<SVGAnimatedTypeAnimator> m_animator;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::SVGAnimateElementBase)
    static bool isType(const WebCore::SVGElement& element)
    {
        return element.hasTagName(WebCore::SVGNames::animateTag) || element.hasTagName(WebCore::SVGNames::animateColorTag)
            || element.hasTagName(WebCore::SVGNames::animateTransformTag) || element.hasTagName(WebCore::SVGNames::setTag);
    }
    static bool isType(const WebCore::Node& node) { return is<WebCore::SVGElement>(node) && isType(downcast<WebCore::SVGElement>(node)); }
SPECIALIZE_TYPE_TRAITS_END()
