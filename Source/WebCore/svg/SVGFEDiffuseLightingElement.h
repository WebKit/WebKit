/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
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

#include "SVGFELightElement.h"
#include "SVGFilterPrimitiveStandardAttributes.h"

namespace WebCore {

class FEDiffuseLighting;
class SVGColor;

class SVGFEDiffuseLightingElement final : public SVGFilterPrimitiveStandardAttributes {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(SVGFEDiffuseLightingElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(SVGFEDiffuseLightingElement);
public:
    static Ref<SVGFEDiffuseLightingElement> create(const QualifiedName&, Document&);
    void lightElementAttributeChanged(const SVGFELightElement*, const QualifiedName&);

    String in1() const { return m_in1->currentValue(); }
    float diffuseConstant() const { return m_diffuseConstant->currentValue(); }
    float surfaceScale() const { return m_surfaceScale->currentValue(); }
    float kernelUnitLengthX() const { return m_kernelUnitLengthX->currentValue(); }
    float kernelUnitLengthY() const { return m_kernelUnitLengthY->currentValue(); }

    SVGAnimatedString& in1Animated() { return m_in1; }
    SVGAnimatedNumber& diffuseConstantAnimated() { return m_diffuseConstant; }
    SVGAnimatedNumber& surfaceScaleAnimated() { return m_surfaceScale; }
    SVGAnimatedNumber& kernelUnitLengthXAnimated() { return m_kernelUnitLengthX; }
    SVGAnimatedNumber& kernelUnitLengthYAnimated() { return m_kernelUnitLengthY; }

private:
    SVGFEDiffuseLightingElement(const QualifiedName&, Document&);

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGFEDiffuseLightingElement, SVGFilterPrimitiveStandardAttributes>;

    void attributeChanged(const QualifiedName&, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason) override;
    void svgAttributeChanged(const QualifiedName&) override;

    bool setFilterEffectAttribute(FilterEffect&, const QualifiedName&) override;
    Vector<AtomString> filterEffectInputsNames() const override { return { AtomString { in1() } }; }
    RefPtr<FilterEffect> createFilterEffect(const FilterEffectVector&, const GraphicsContext& destinationContext) const override;

    Ref<SVGAnimatedString> m_in1 { SVGAnimatedString::create(this) };
    Ref<SVGAnimatedNumber> m_diffuseConstant { SVGAnimatedNumber::create(this, 1) };
    Ref<SVGAnimatedNumber> m_surfaceScale { SVGAnimatedNumber::create(this, 1) };
    Ref<SVGAnimatedNumber> m_kernelUnitLengthX { SVGAnimatedNumber::create(this) };
    Ref<SVGAnimatedNumber> m_kernelUnitLengthY { SVGAnimatedNumber::create(this) };
};

} // namespace WebCore
