/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

#include "FEDropShadow.h"
#include "SVGFilterPrimitiveStandardAttributes.h"

namespace WebCore {

class SVGFEDropShadowElement final : public SVGFilterPrimitiveStandardAttributes {
    WTF_MAKE_ISO_ALLOCATED(SVGFEDropShadowElement);
public:
    static Ref<SVGFEDropShadowElement> create(const QualifiedName&, Document&);
    
    void setStdDeviation(float stdDeviationX, float stdDeviationY);

    String in1() const { return m_in1->currentValue(); }
    float dx() const { return m_dx->currentValue(); }
    float dy() const { return m_dy->currentValue(); }
    float stdDeviationX() const { return m_stdDeviationX->currentValue(); }
    float stdDeviationY() const { return m_stdDeviationY->currentValue(); }

    SVGAnimatedString& in1Animated() { return m_in1; }
    SVGAnimatedNumber& dxAnimated() { return m_dx; }
    SVGAnimatedNumber& dyAnimated() { return m_dy; }
    SVGAnimatedNumber& stdDeviationXAnimated() { return m_stdDeviationX; }
    SVGAnimatedNumber& stdDeviationYAnimated() { return m_stdDeviationY; }

private:
    SVGFEDropShadowElement(const QualifiedName&, Document&);

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGFEDropShadowElement, SVGFilterPrimitiveStandardAttributes>;
    const SVGPropertyRegistry& propertyRegistry() const final { return m_propertyRegistry; }

    void parseAttribute(const QualifiedName&, const AtomString&) override;
    void svgAttributeChanged(const QualifiedName&) override;

    RefPtr<FilterEffect> build(SVGFilterBuilder*, Filter&) const override;

    PropertyRegistry m_propertyRegistry { *this };
    Ref<SVGAnimatedString> m_in1 { SVGAnimatedString::create(this) };
    Ref<SVGAnimatedNumber> m_dx { SVGAnimatedNumber::create(this, 2) };
    Ref<SVGAnimatedNumber> m_dy { SVGAnimatedNumber::create(this, 2) };
    Ref<SVGAnimatedNumber> m_stdDeviationX { SVGAnimatedNumber::create(this, 2) };
    Ref<SVGAnimatedNumber> m_stdDeviationY { SVGAnimatedNumber::create(this, 2) };
};
    
} // namespace WebCore
