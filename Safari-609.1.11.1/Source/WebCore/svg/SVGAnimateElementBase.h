/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2008-2019 Apple Inc. All rights reserved.
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

#include "SVGAnimationElement.h"
#include "SVGNames.h"

namespace WebCore {

class SVGAttributeAnimator;

class SVGAnimateElementBase : public SVGAnimationElement {
    WTF_MAKE_ISO_ALLOCATED(SVGAnimateElementBase);
public:
    bool isDiscreteAnimator() const;

protected:
    SVGAnimateElementBase(const QualifiedName&, Document&);

    bool hasValidAttributeType() const override;
    virtual String animateRangeString(const String& string) const { return string; }

private:
    SVGAttributeAnimator* animator() const;
    SVGAttributeAnimator* animatorIfExists() const { return m_animator.get(); }

    void setTargetElement(SVGElement*) override;
    void setAttributeName(const QualifiedName&) override;
    void resetAnimation() override;

    bool calculateFromAndToValues(const String& fromString, const String& toString) override;
    bool calculateFromAndByValues(const String& fromString, const String& byString) override;
    bool calculateToAtEndOfDurationValue(const String& toAtEndOfDurationString) override;

    void startAnimation() override;
    void calculateAnimatedValue(float progress, unsigned repeatCount) override;
    void applyResultsToTarget() override;
    void stopAnimation(SVGElement* targetElement) override;
    Optional<float> calculateDistance(const String& fromString, const String& toString) override;

    bool hasInvalidCSSAttributeType() const;

    mutable RefPtr<SVGAttributeAnimator> m_animator;
    mutable Optional<bool> m_hasInvalidCSSAttributeType;
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
