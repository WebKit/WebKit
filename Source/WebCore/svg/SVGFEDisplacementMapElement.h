/*
 * Copyright (C) 2006 Oliver Hunt <oliver@nerget.com>
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

#include "FEDisplacementMap.h"
#include "SVGFilterPrimitiveStandardAttributes.h"

namespace WebCore {
 
template<>
struct SVGPropertyTraits<ChannelSelectorType> {
    static unsigned highestEnumValue() { return CHANNEL_A; }

    static String toString(ChannelSelectorType type)
    {
        switch (type) {
        case CHANNEL_UNKNOWN:
            return emptyString();
        case CHANNEL_R:
            return "R"_s;
        case CHANNEL_G:
            return "G"_s;
        case CHANNEL_B:
            return "B"_s;
        case CHANNEL_A:
            return "A"_s;
        }

        ASSERT_NOT_REACHED();
        return emptyString();
    }

    static ChannelSelectorType fromString(const String& value)
    {
        if (value == "R")
            return CHANNEL_R;
        if (value == "G")
            return CHANNEL_G;
        if (value == "B")
            return CHANNEL_B;
        if (value == "A")
            return CHANNEL_A;
        return CHANNEL_UNKNOWN;
    }
};

class SVGFEDisplacementMapElement final : public SVGFilterPrimitiveStandardAttributes {
    WTF_MAKE_ISO_ALLOCATED(SVGFEDisplacementMapElement);
public:
    static Ref<SVGFEDisplacementMapElement> create(const QualifiedName&, Document&);

    static ChannelSelectorType stringToChannel(const String&);

    String in1() const { return m_in1->currentValue(); }
    String in2() const { return m_in2->currentValue(); }
    ChannelSelectorType xChannelSelector() const { return m_xChannelSelector->currentValue<ChannelSelectorType>(); }
    ChannelSelectorType yChannelSelector() const { return m_yChannelSelector->currentValue<ChannelSelectorType>(); }
    float scale() const { return m_scale->currentValue(); }

    SVGAnimatedString& in1Animated() { return m_in1; }
    SVGAnimatedString& in2Animated() { return m_in2; }
    SVGAnimatedEnumeration& xChannelSelectorAnimated() { return m_xChannelSelector; }
    SVGAnimatedEnumeration& yChannelSelectorAnimated() { return m_yChannelSelector; }
    SVGAnimatedNumber& scaleAnimated() { return m_scale; }

private:
    SVGFEDisplacementMapElement(const QualifiedName& tagName, Document&);

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGFEDisplacementMapElement, SVGFilterPrimitiveStandardAttributes>;
    const SVGPropertyRegistry& propertyRegistry() const final { return m_propertyRegistry; }

    void parseAttribute(const QualifiedName&, const AtomString&) override;
    void svgAttributeChanged(const QualifiedName&) override;

    bool setFilterEffectAttribute(FilterEffect*, const QualifiedName& attrName) override;
    RefPtr<FilterEffect> build(SVGFilterBuilder*, Filter&) const override;

    PropertyRegistry m_propertyRegistry { *this };
    Ref<SVGAnimatedString> m_in1 { SVGAnimatedString::create(this) };
    Ref<SVGAnimatedString> m_in2 { SVGAnimatedString::create(this) };
    Ref<SVGAnimatedEnumeration> m_xChannelSelector { SVGAnimatedEnumeration::create(this, CHANNEL_A) };
    Ref<SVGAnimatedEnumeration> m_yChannelSelector { SVGAnimatedEnumeration::create(this, CHANNEL_A) };
    Ref<SVGAnimatedNumber> m_scale { SVGAnimatedNumber::create(this) };
};

} // namespace WebCore
