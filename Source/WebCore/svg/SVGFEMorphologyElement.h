/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2018-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
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
#include <wtf/TZoneMalloc.h>

namespace WebCore {

template<>
struct SVGPropertyTraits<MorphologyOperatorType> {
    static unsigned highestEnumValue() { return static_cast<unsigned>(MorphologyOperatorType::Dilate); }

    static RefPtr<StringImpl> toString(MorphologyOperatorType type)
    {
        switch (type) {
        case MorphologyOperatorType::Unknown:
            return emptyString();
        case MorphologyOperatorType::Erode:
            return "erode"_si;
        case MorphologyOperatorType::Dilate:
            return "dilate"_si;
        }

        ASSERT_NOT_REACHED();
        return emptyString();
    }

    static MorphologyOperatorType fromString(SVGElement&, StringImpl* value)
    {
        if (equal(value, "erode"_s))
            return MorphologyOperatorType::Erode;
        if (equal(value, "dilate"_s))
            return MorphologyOperatorType::Dilate;
        return MorphologyOperatorType::Unknown;
    }
};

class SVGFEMorphologyElement final : public SVGFilterPrimitiveStandardAttributes {
    WTF_MAKE_TZONE_ALLOCATED(SVGFEMorphologyElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(SVGFEMorphologyElement);
public:
    static Ref<SVGFEMorphologyElement> create(const QualifiedName&, Document&);

    RefPtr<StringImpl> in1() const { return m_in1->currentValue(); }
    MorphologyOperatorType svgOperator() const { return m_svgOperator->currentValue<MorphologyOperatorType>(); }
    float radiusX() const { return m_radiusX->currentValue(); }
    float radiusY() const { return m_radiusY->currentValue(); }

    SVGAnimatedString& in1Animated() { return m_in1; }
    SVGAnimatedEnumeration& svgOperatorAnimated() { return m_svgOperator; }
    SVGAnimatedNumber& radiusXAnimated() { return m_radiusX; }
    SVGAnimatedNumber& radiusYAnimated() { return m_radiusY; }

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGFEMorphologyElement, SVGFilterPrimitiveStandardAttributes>;

private:
    SVGFEMorphologyElement(const QualifiedName&, Document&);

    void attributeChanged(const QualifiedName&, AtomStringImpl* oldValue, AtomStringImpl* newValue, AttributeModificationReason) override;
    void svgAttributeChanged(const QualifiedName&) override;

    bool setFilterEffectAttribute(FilterEffect&, const QualifiedName&) override;
    Vector<RefPtr<AtomStringImpl>> filterEffectInputsNames() const override { return { AtomStringImpl::add(in1()) }; }
    bool isIdentity() const override;
    IntOutsets outsets(const FloatRect& targetBoundingBox, SVGUnitTypes::SVGUnitType primitiveUnits) const override;
    RefPtr<FilterEffect> createFilterEffect(const FilterEffectVector&, const GraphicsContext& destinationContext) const override;

    const Ref<SVGAnimatedString> m_in1 { SVGAnimatedString::create(this) };
    const Ref<SVGAnimatedEnumeration> m_svgOperator { SVGAnimatedEnumeration::create(this, MorphologyOperatorType::Erode) };
    const Ref<SVGAnimatedNumber> m_radiusX { SVGAnimatedNumber::create(this) };
    const Ref<SVGAnimatedNumber> m_radiusY { SVGAnimatedNumber::create(this) };
};

} // namespace WebCore
