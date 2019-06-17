/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#include "FEColorMatrix.h"
#include "SVGFilterPrimitiveStandardAttributes.h"

namespace WebCore {

template<>
struct SVGPropertyTraits<ColorMatrixType> {
    static unsigned highestEnumValue() { return FECOLORMATRIX_TYPE_LUMINANCETOALPHA; }

    static String toString(ColorMatrixType type)
    {
        switch (type) {
        case FECOLORMATRIX_TYPE_UNKNOWN:
            return emptyString();
        case FECOLORMATRIX_TYPE_MATRIX:
            return "matrix"_s;
        case FECOLORMATRIX_TYPE_SATURATE:
            return "saturate"_s;
        case FECOLORMATRIX_TYPE_HUEROTATE:
            return "hueRotate"_s;
        case FECOLORMATRIX_TYPE_LUMINANCETOALPHA:
            return "luminanceToAlpha"_s;
        }

        ASSERT_NOT_REACHED();
        return emptyString();
    }

    static ColorMatrixType fromString(const String& value)
    {
        if (value == "matrix")
            return FECOLORMATRIX_TYPE_MATRIX;
        if (value == "saturate")
            return FECOLORMATRIX_TYPE_SATURATE;
        if (value == "hueRotate")
            return FECOLORMATRIX_TYPE_HUEROTATE;
        if (value == "luminanceToAlpha")
            return FECOLORMATRIX_TYPE_LUMINANCETOALPHA;
        return FECOLORMATRIX_TYPE_UNKNOWN;
    }
};

class SVGFEColorMatrixElement final : public SVGFilterPrimitiveStandardAttributes {
    WTF_MAKE_ISO_ALLOCATED(SVGFEColorMatrixElement);
public:
    static Ref<SVGFEColorMatrixElement> create(const QualifiedName&, Document&);

    String in1() const { return m_in1->currentValue(); }
    ColorMatrixType type() const { return m_type->currentValue<ColorMatrixType>(); }
    const SVGNumberList& values() const { return m_values->currentValue(); }

    SVGAnimatedString& in1Animated() { return m_in1; }
    SVGAnimatedEnumeration& typeAnimated() { return m_type; }
    SVGAnimatedNumberList& valuesAnimated() { return m_values; }

private:
    SVGFEColorMatrixElement(const QualifiedName&, Document&);

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGFEColorMatrixElement, SVGFilterPrimitiveStandardAttributes>;
    const SVGPropertyRegistry& propertyRegistry() const final { return m_propertyRegistry; }

    void parseAttribute(const QualifiedName&, const AtomString&) override;
    void svgAttributeChanged(const QualifiedName&) override;

    bool setFilterEffectAttribute(FilterEffect*, const QualifiedName&) override;
    RefPtr<FilterEffect> build(SVGFilterBuilder*, Filter&) const override;

    PropertyRegistry m_propertyRegistry { *this };
    Ref<SVGAnimatedString> m_in1 { SVGAnimatedString::create(this) };
    Ref<SVGAnimatedEnumeration> m_type { SVGAnimatedEnumeration::create(this, FECOLORMATRIX_TYPE_MATRIX) };
    Ref<SVGAnimatedNumberList> m_values { SVGAnimatedNumberList::create(this) };
};

} // namespace WebCore
