/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedNumber.h"
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

    String in1() { return m_in1.currentValue(attributeOwnerProxy()); }
    String in2() { return m_in2.currentValue(attributeOwnerProxy()); }
    CompositeOperationType svgOperator() const { return m_svgOperator.currentValue(attributeOwnerProxy()); }
    float k1() const { return m_k1.currentValue(attributeOwnerProxy()); }
    float k2() const { return m_k2.currentValue(attributeOwnerProxy()); }
    float k3() const { return m_k3.currentValue(attributeOwnerProxy()); }
    float k4() const { return m_k4.currentValue(attributeOwnerProxy()); }

    RefPtr<SVGAnimatedString> in1Animated() { return m_in1.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedString> in2Animated() { return m_in2.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedEnumeration> svgOperatorAnimated() { return m_svgOperator.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedNumber> k1Animated() { return m_k1.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedNumber> k2Animated() { return m_k2.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedNumber> k3Animated() { return m_k3.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedNumber> k4Animated() { return m_k4.animatedProperty(attributeOwnerProxy()); }

private:
    SVGFECompositeElement(const QualifiedName&, Document&);

    using AttributeOwnerProxy = SVGAttributeOwnerProxyImpl<SVGFECompositeElement, SVGFilterPrimitiveStandardAttributes>;
    static AttributeOwnerProxy::AttributeRegistry& attributeRegistry() { return AttributeOwnerProxy::attributeRegistry(); }
    static bool isKnownAttribute(const QualifiedName& attributeName) { return AttributeOwnerProxy::isKnownAttribute(attributeName); }
    static void registerAttributes();

    const SVGAttributeOwnerProxy& attributeOwnerProxy() const final { return m_attributeOwnerProxy; }
    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    void svgAttributeChanged(const QualifiedName&) override;

    bool setFilterEffectAttribute(FilterEffect*, const QualifiedName&) override;
    RefPtr<FilterEffect> build(SVGFilterBuilder*, Filter&) override;

    AttributeOwnerProxy m_attributeOwnerProxy { *this };
    SVGAnimatedStringAttribute m_in1;
    SVGAnimatedStringAttribute m_in2;
    SVGAnimatedEnumerationAttribute<CompositeOperationType> m_svgOperator { FECOMPOSITE_OPERATOR_OVER };
    SVGAnimatedNumberAttribute m_k1;
    SVGAnimatedNumberAttribute m_k2;
    SVGAnimatedNumberAttribute m_k3;
    SVGAnimatedNumberAttribute m_k4;
};

} // namespace WebCore
