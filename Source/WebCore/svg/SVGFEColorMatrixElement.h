/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#include "FEColorMatrix.h"
#include "SVGFilterPrimitiveStandardAttributes.h"
#include <wtf/TZoneMalloc.h>

namespace WebCore {

template<>
struct SVGPropertyTraits<ColorMatrixType> {
    static unsigned highestEnumValue() { return std::to_underlying(ColorMatrixType::FECOLORMATRIX_TYPE_LUMINANCETOALPHA); }

    static RefPtr<StringImpl> toString(ColorMatrixType type)
    {
        switch (type) {
        case ColorMatrixType::FECOLORMATRIX_TYPE_UNKNOWN:
            return emptyString();
        case ColorMatrixType::FECOLORMATRIX_TYPE_MATRIX:
            return "matrix"_si;
        case ColorMatrixType::FECOLORMATRIX_TYPE_SATURATE:
            return "saturate"_si;
        case ColorMatrixType::FECOLORMATRIX_TYPE_HUEROTATE:
            return "hueRotate"_si;
        case ColorMatrixType::FECOLORMATRIX_TYPE_LUMINANCETOALPHA:
            return "luminanceToAlpha"_si;
        }

        ASSERT_NOT_REACHED();
        return emptyString();
    }

    static ColorMatrixType fromString(SVGElement&, StringImpl* value)
    {
        if (equal(value, "matrix"_s))
            return ColorMatrixType::FECOLORMATRIX_TYPE_MATRIX;
        if (equal(value, "saturate"_s))
            return ColorMatrixType::FECOLORMATRIX_TYPE_SATURATE;
        if (equal(value, "hueRotate"_s))
            return ColorMatrixType::FECOLORMATRIX_TYPE_HUEROTATE;
        if (equal(value, "luminanceToAlpha"_s))
            return ColorMatrixType::FECOLORMATRIX_TYPE_LUMINANCETOALPHA;
        return ColorMatrixType::FECOLORMATRIX_TYPE_UNKNOWN;
    }
};

class SVGFEColorMatrixElement final : public SVGFilterPrimitiveStandardAttributes {
    WTF_MAKE_TZONE_ALLOCATED(SVGFEColorMatrixElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(SVGFEColorMatrixElement);
public:
    static Ref<SVGFEColorMatrixElement> create(const QualifiedName&, Document&);

    RefPtr<StringImpl> in1() const { return m_in1->currentValue(); }
    ColorMatrixType type() const { return m_type->currentValue<ColorMatrixType>(); }
    const SVGNumberList& values() const LIFETIME_BOUND { return m_values->currentValue(); }

    SVGAnimatedString& in1Animated() { return m_in1; }
    SVGAnimatedEnumeration& typeAnimated() { return m_type; }
    SVGAnimatedNumberList& valuesAnimated() { return m_values; }

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGFEColorMatrixElement, SVGFilterPrimitiveStandardAttributes>;

private:
    SVGFEColorMatrixElement(const QualifiedName&, Document&);

    void attributeChanged(const QualifiedName&, AtomStringImpl* oldValue, AtomStringImpl* newValue, AttributeModificationReason) override;
    void svgAttributeChanged(const QualifiedName&) override;

    bool setFilterEffectAttribute(FilterEffect&, const QualifiedName&) override;
    Vector<RefPtr<AtomStringImpl>> filterEffectInputsNames() const override { return { AtomStringImpl::add(in1()) }; }
    RefPtr<FilterEffect> createFilterEffect(const FilterEffectVector&, const GraphicsContext& destinationContext) const override;

    const Ref<SVGAnimatedString> m_in1 { SVGAnimatedString::create(this) };
    const Ref<SVGAnimatedEnumeration> m_type { SVGAnimatedEnumeration::create(this, ColorMatrixType::FECOLORMATRIX_TYPE_MATRIX) };
    const Ref<SVGAnimatedNumberList> m_values { SVGAnimatedNumberList::create(this) };
};

} // namespace WebCore
