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

#include "FEComposite.h"
#include "SVGFilterPrimitiveStandardAttributes.h"

namespace WebCore {

template<>
inline unsigned SVGIDLEnumLimits<CompositeOperationType>::highestExposedEnumValue() { return FECOMPOSITE_OPERATOR_ARITHMETIC; }

template<>
struct SVGPropertyTraits<CompositeOperationType> {
    static unsigned highestEnumValue() { return FECOMPOSITE_OPERATOR_LIGHTER; }

    static String toString(CompositeOperationType type)
    {
        switch (type) {
        case FECOMPOSITE_OPERATOR_UNKNOWN:
            return emptyString();
        case FECOMPOSITE_OPERATOR_OVER:
            return "over"_s;
        case FECOMPOSITE_OPERATOR_IN:
            return "in"_s;
        case FECOMPOSITE_OPERATOR_OUT:
            return "out"_s;
        case FECOMPOSITE_OPERATOR_ATOP:
            return "atop"_s;
        case FECOMPOSITE_OPERATOR_XOR:
            return "xor"_s;
        case FECOMPOSITE_OPERATOR_ARITHMETIC:
            return "arithmetic"_s;
        case FECOMPOSITE_OPERATOR_LIGHTER:
            return "lighter"_s;
        }

        ASSERT_NOT_REACHED();
        return emptyString();
    }

    static CompositeOperationType fromString(const String& value)
    {
        if (value == "over")
            return FECOMPOSITE_OPERATOR_OVER;
        if (value == "in")
            return FECOMPOSITE_OPERATOR_IN;
        if (value == "out")
            return FECOMPOSITE_OPERATOR_OUT;
        if (value == "atop")
            return FECOMPOSITE_OPERATOR_ATOP;
        if (value == "xor")
            return FECOMPOSITE_OPERATOR_XOR;
        if (value == "arithmetic")
            return FECOMPOSITE_OPERATOR_ARITHMETIC;
        if (value == "lighter")
            return FECOMPOSITE_OPERATOR_LIGHTER;
        return FECOMPOSITE_OPERATOR_UNKNOWN;
    }
};

class SVGFECompositeElement final : public SVGFilterPrimitiveStandardAttributes {
    WTF_MAKE_ISO_ALLOCATED(SVGFECompositeElement);
public:
    static Ref<SVGFECompositeElement> create(const QualifiedName&, Document&);

    String in1() const { return m_in1->currentValue(); }
    String in2() const { return m_in2->currentValue(); }
    CompositeOperationType svgOperator() const { return m_svgOperator->currentValue<CompositeOperationType>(); }
    float k1() const { return m_k1->currentValue(); }
    float k2() const { return m_k2->currentValue(); }
    float k3() const { return m_k3->currentValue(); }
    float k4() const { return m_k4->currentValue(); }

    SVGAnimatedString& in1Animated() { return m_in1; }
    SVGAnimatedString& in2Animated() { return m_in2; }
    SVGAnimatedEnumeration& svgOperatorAnimated() { return m_svgOperator; }
    SVGAnimatedNumber& k1Animated() { return m_k1; }
    SVGAnimatedNumber& k2Animated() { return m_k2; }
    SVGAnimatedNumber& k3Animated() { return m_k3; }
    SVGAnimatedNumber& k4Animated() { return m_k4; }

private:
    SVGFECompositeElement(const QualifiedName&, Document&);

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGFECompositeElement, SVGFilterPrimitiveStandardAttributes>;
    const SVGPropertyRegistry& propertyRegistry() const final { return m_propertyRegistry; }

    void parseAttribute(const QualifiedName&, const AtomString&) override;
    void svgAttributeChanged(const QualifiedName&) override;

    bool setFilterEffectAttribute(FilterEffect*, const QualifiedName&) override;
    RefPtr<FilterEffect> build(SVGFilterBuilder*, Filter&) const override;

    PropertyRegistry m_propertyRegistry { *this };
    Ref<SVGAnimatedString> m_in1 { SVGAnimatedString::create(this) };
    Ref<SVGAnimatedString> m_in2 { SVGAnimatedString::create(this) };
    Ref<SVGAnimatedEnumeration> m_svgOperator { SVGAnimatedEnumeration::create(this, FECOMPOSITE_OPERATOR_OVER) };
    Ref<SVGAnimatedNumber> m_k1 { SVGAnimatedNumber::create(this) };
    Ref<SVGAnimatedNumber> m_k2 { SVGAnimatedNumber::create(this) };
    Ref<SVGAnimatedNumber> m_k3 { SVGAnimatedNumber::create(this) };
    Ref<SVGAnimatedNumber> m_k4 { SVGAnimatedNumber::create(this) };
};

} // namespace WebCore
