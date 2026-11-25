/*
 * Copyright (C) 2006 Oliver Hunt <oliver@nerget.com>
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

#include "FEDisplacementMap.h"
#include "SVGFilterPrimitiveStandardAttributes.h"
#include <wtf/TZoneMalloc.h>

namespace WebCore {
 
template<>
struct SVGPropertyTraits<ChannelSelectorType> {
    static unsigned highestEnumValue() { return std::to_underlying(ChannelSelectorType::CHANNEL_A); }

    static RefPtr<StringImpl> toString(ChannelSelectorType type)
    {
        switch (type) {
        case ChannelSelectorType::CHANNEL_UNKNOWN:
            return emptyString();
        case ChannelSelectorType::CHANNEL_R:
            return "R"_si;
        case ChannelSelectorType::CHANNEL_G:
            return "G"_si;
        case ChannelSelectorType::CHANNEL_B:
            return "B"_si;
        case ChannelSelectorType::CHANNEL_A:
            return "A"_si;
        }

        ASSERT_NOT_REACHED();
        return emptyString();
    }

    static ChannelSelectorType fromString(SVGElement&, StringImpl* value)
    {
        if (equal(value, "R"_s))
            return ChannelSelectorType::CHANNEL_R;
        if (equal(value, "G"_s))
            return ChannelSelectorType::CHANNEL_G;
        if (equal(value, "B"_s))
            return ChannelSelectorType::CHANNEL_B;
        if (equal(value, "A"_s))
            return ChannelSelectorType::CHANNEL_A;
        return ChannelSelectorType::CHANNEL_UNKNOWN;
    }
};

class SVGFEDisplacementMapElement final : public SVGFilterPrimitiveStandardAttributes {
    WTF_MAKE_TZONE_ALLOCATED(SVGFEDisplacementMapElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(SVGFEDisplacementMapElement);
public:
    static Ref<SVGFEDisplacementMapElement> create(const QualifiedName&, Document&);

    RefPtr<StringImpl> in1() const { return m_in1->currentValue(); }
    RefPtr<StringImpl> in2() const { return m_in2->currentValue(); }
    ChannelSelectorType xChannelSelector() const { return m_xChannelSelector->currentValue<ChannelSelectorType>(); }
    ChannelSelectorType yChannelSelector() const { return m_yChannelSelector->currentValue<ChannelSelectorType>(); }
    float scale() const { return m_scale->currentValue(); }

    SVGAnimatedString& in1Animated() { return m_in1; }
    SVGAnimatedString& in2Animated() { return m_in2; }
    SVGAnimatedEnumeration& xChannelSelectorAnimated() { return m_xChannelSelector; }
    SVGAnimatedEnumeration& yChannelSelectorAnimated() { return m_yChannelSelector; }
    SVGAnimatedNumber& scaleAnimated() { return m_scale; }

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGFEDisplacementMapElement, SVGFilterPrimitiveStandardAttributes>;

private:
    SVGFEDisplacementMapElement(const QualifiedName& tagName, Document&);

    void attributeChanged(const QualifiedName&, AtomStringImpl* oldValue, AtomStringImpl* newValue, AttributeModificationReason) override;
    void svgAttributeChanged(const QualifiedName&) override;

    bool setFilterEffectAttribute(FilterEffect&, const QualifiedName& attrName) override;
    Vector<RefPtr<AtomStringImpl>> filterEffectInputsNames() const override { return { AtomStringImpl::add(in1()), AtomStringImpl::add(in2()) }; }
    IntOutsets outsets(const FloatRect& targetBoundingBox, SVGUnitTypes::SVGUnitType primitiveUnits) const override;
    RefPtr<FilterEffect> createFilterEffect(const FilterEffectVector&, const GraphicsContext& destinationContext) const override;

    const Ref<SVGAnimatedString> m_in1 { SVGAnimatedString::create(this) };
    const Ref<SVGAnimatedString> m_in2 { SVGAnimatedString::create(this) };
    const Ref<SVGAnimatedEnumeration> m_xChannelSelector { SVGAnimatedEnumeration::create(this, ChannelSelectorType::CHANNEL_A) };
    const Ref<SVGAnimatedEnumeration> m_yChannelSelector { SVGAnimatedEnumeration::create(this, ChannelSelectorType::CHANNEL_A) };
    const Ref<SVGAnimatedNumber> m_scale { SVGAnimatedNumber::create(this) };
};

} // namespace WebCore
