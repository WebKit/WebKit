/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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

#include "FEMorphology.h"
#include "SVGFilterPrimitiveStandardAttributes.h"

namespace WebCore {

template<>
struct SVGPropertyTraits<MorphologyOperatorType> {
    static unsigned highestEnumValue() { return static_cast<unsigned>(MorphologyOperatorType::Dilate); }

    static String toString(MorphologyOperatorType type)
    {
        switch (type) {
        case MorphologyOperatorType::Unknown:
            return emptyString();
        case MorphologyOperatorType::Erode:
            return "erode"_s;
        case MorphologyOperatorType::Dilate:
            return "dilate"_s;
        }

        ASSERT_NOT_REACHED();
        return emptyString();
    }

    static MorphologyOperatorType fromString(const String& value)
    {
        if (value == "erode")
            return MorphologyOperatorType::Erode;
        if (value == "dilate")
            return MorphologyOperatorType::Dilate;
        return MorphologyOperatorType::Unknown;
    }
};

class SVGFEMorphologyElement final : public SVGFilterPrimitiveStandardAttributes {
    WTF_MAKE_ISO_ALLOCATED(SVGFEMorphologyElement);
public:
    static Ref<SVGFEMorphologyElement> create(const QualifiedName&, Document&);

    void setRadius(float radiusX, float radiusY);

    String in1() const { return m_in1->currentValue(); }
    MorphologyOperatorType svgOperator() const { return m_svgOperator->currentValue<MorphologyOperatorType>(); }
    float radiusX() const { return m_radiusX->currentValue(); }
    float radiusY() const { return m_radiusY->currentValue(); }

    SVGAnimatedString& in1Animated() { return m_in1; }
    SVGAnimatedEnumeration& svgOperatorAnimated() { return m_svgOperator; }
    SVGAnimatedNumber& radiusXAnimated() { return m_radiusX; }
    SVGAnimatedNumber& radiusYAnimated() { return m_radiusY; }

private:
    SVGFEMorphologyElement(const QualifiedName&, Document&);

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGFEMorphologyElement, SVGFilterPrimitiveStandardAttributes>;
    const SVGPropertyRegistry& propertyRegistry() const final { return m_propertyRegistry; }

    void parseAttribute(const QualifiedName&, const AtomString&) override;
    void svgAttributeChanged(const QualifiedName&) override;

    bool setFilterEffectAttribute(FilterEffect*, const QualifiedName&) override;
    Vector<AtomString> filterEffectInputsNames() const override { return { in1() }; }
    RefPtr<FilterEffect> filterEffect(const SVGFilterBuilder&, const FilterEffectVector&) const override;

    PropertyRegistry m_propertyRegistry { *this };
    Ref<SVGAnimatedString> m_in1 { SVGAnimatedString::create(this) };
    Ref<SVGAnimatedEnumeration> m_svgOperator { SVGAnimatedEnumeration::create(this, MorphologyOperatorType::Erode) };
    Ref<SVGAnimatedNumber> m_radiusX { SVGAnimatedNumber::create(this) };
    Ref<SVGAnimatedNumber> m_radiusY { SVGAnimatedNumber::create(this) };
};

} // namespace WebCore
