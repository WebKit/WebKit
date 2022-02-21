/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Oliver Hunt <oliver@nerget.com>
 * Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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

#include "FESpecularLighting.h"
#include "SVGFELightElement.h"
#include "SVGFilterPrimitiveStandardAttributes.h"

namespace WebCore {

class SVGFESpecularLightingElement final : public SVGFilterPrimitiveStandardAttributes {
    WTF_MAKE_ISO_ALLOCATED(SVGFESpecularLightingElement);
public:
    static Ref<SVGFESpecularLightingElement> create(const QualifiedName&, Document&);
    void lightElementAttributeChanged(const SVGFELightElement*, const QualifiedName&);

    String in1() const { return m_in1->currentValue(); }
    float specularConstant() const { return m_specularConstant->currentValue(); }
    float specularExponent() const { return m_specularExponent->currentValue(); }
    float surfaceScale() const { return m_surfaceScale->currentValue(); }
    float kernelUnitLengthX() const { return m_kernelUnitLengthX->currentValue(); }
    float kernelUnitLengthY() const { return m_kernelUnitLengthY->currentValue(); }

    SVGAnimatedString& in1Animated() { return m_in1; }
    SVGAnimatedNumber& specularConstantAnimated() { return m_specularConstant; }
    SVGAnimatedNumber& specularExponentAnimated() { return m_specularExponent; }
    SVGAnimatedNumber& surfaceScaleAnimated() { return m_surfaceScale; }
    SVGAnimatedNumber& kernelUnitLengthXAnimated() { return m_kernelUnitLengthX; }
    SVGAnimatedNumber& kernelUnitLengthYAnimated() { return m_kernelUnitLengthY; }

private:
    SVGFESpecularLightingElement(const QualifiedName&, Document&);

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGFESpecularLightingElement, SVGFilterPrimitiveStandardAttributes>;
    const SVGPropertyRegistry& propertyRegistry() const final { return m_propertyRegistry; }

    void parseAttribute(const QualifiedName&, const AtomString&) override;
    void svgAttributeChanged(const QualifiedName&) override;

    bool setFilterEffectAttribute(FilterEffect*, const QualifiedName&) override;
    Vector<AtomString> filterEffectInputsNames() const override { return { in1() }; }
    RefPtr<FilterEffect> filterEffect(const SVGFilterBuilder&, const FilterEffectVector&) const override;

    PropertyRegistry m_propertyRegistry { *this };
    Ref<SVGAnimatedString> m_in1 { SVGAnimatedString::create(this) };
    Ref<SVGAnimatedNumber> m_specularConstant { SVGAnimatedNumber::create(this, 1) };
    Ref<SVGAnimatedNumber> m_specularExponent { SVGAnimatedNumber::create(this, 1) };
    Ref<SVGAnimatedNumber> m_surfaceScale { SVGAnimatedNumber::create(this, 1) };
    Ref<SVGAnimatedNumber> m_kernelUnitLengthX { SVGAnimatedNumber::create(this) };
    Ref<SVGAnimatedNumber> m_kernelUnitLengthY { SVGAnimatedNumber::create(this) };
};

} // namespace WebCore
