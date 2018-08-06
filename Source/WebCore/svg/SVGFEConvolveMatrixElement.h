/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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

#include "FEConvolveMatrix.h"
#include "SVGAnimatedBoolean.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedInteger.h"
#include "SVGAnimatedNumber.h"
#include "SVGAnimatedNumberList.h"
#include "SVGFilterPrimitiveStandardAttributes.h"

namespace WebCore {

template<>
struct SVGPropertyTraits<EdgeModeType> {
    static unsigned highestEnumValue() { return EDGEMODE_NONE; }
    static EdgeModeType initialValue() { return EDGEMODE_NONE; }

    static String toString(EdgeModeType type)
    {
        switch (type) {
        case EDGEMODE_UNKNOWN:
            return emptyString();
        case EDGEMODE_DUPLICATE:
            return "duplicate"_s;
        case EDGEMODE_WRAP:
            return "wrap"_s;
        case EDGEMODE_NONE:
            return "none"_s;
        }

        ASSERT_NOT_REACHED();
        return emptyString();
    }

    static EdgeModeType fromString(const String& value)
    {
        if (value == "duplicate")
            return EDGEMODE_DUPLICATE;
        if (value == "wrap")
            return EDGEMODE_WRAP;
        if (value == "none")
            return EDGEMODE_NONE;
        return EDGEMODE_UNKNOWN;
    }
};

class SVGFEConvolveMatrixElement final : public SVGFilterPrimitiveStandardAttributes {
    WTF_MAKE_ISO_ALLOCATED(SVGFEConvolveMatrixElement);
public:
    static Ref<SVGFEConvolveMatrixElement> create(const QualifiedName&, Document&);

    void setOrder(float orderX, float orderY);
    void setKernelUnitLength(float kernelUnitLengthX, float kernelUnitLengthY);

    String in1() const { return m_in1.currentValue(attributeOwnerProxy()); }
    int orderX() const { return m_orderX.currentValue(attributeOwnerProxy()); }
    int orderY() const { return m_orderY.currentValue(attributeOwnerProxy()); }
    const SVGNumberListValues& kernelMatrix() const { return m_kernelMatrix.currentValue(attributeOwnerProxy()); }
    float divisor() const { return m_divisor.currentValue(attributeOwnerProxy()); }
    float bias() const { return m_bias.currentValue(attributeOwnerProxy()); }
    int targetX() const { return m_targetX.currentValue(attributeOwnerProxy()); }
    int targetY() const { return m_targetY.currentValue(attributeOwnerProxy()); }
    EdgeModeType edgeMode() const { return m_edgeMode.currentValue(attributeOwnerProxy()); }
    float kernelUnitLengthX() const { return m_kernelUnitLengthX.currentValue(attributeOwnerProxy()); }
    float kernelUnitLengthY() const { return m_kernelUnitLengthY.currentValue(attributeOwnerProxy()); }
    bool preserveAlpha() const { return m_preserveAlpha.currentValue(attributeOwnerProxy()); }

    RefPtr<SVGAnimatedString> in1Animated() { return m_in1.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedInteger> orderXAnimated() { return m_orderX.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedInteger> orderYAnimated() { return m_orderY.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedNumberList> kernelMatrixAnimated() { return m_kernelMatrix.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedNumber> divisorAnimated() { return m_divisor.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedNumber> biasAnimated() { return m_bias.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedInteger> targetXAnimated() { return m_targetX.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedInteger> targetYAnimated() { return m_targetY.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedEnumeration> edgeModeAnimated() { return m_edgeMode.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedNumber> kernelUnitLengthXAnimated() { return m_kernelUnitLengthX.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedNumber> kernelUnitLengthYAnimated() { return m_kernelUnitLengthY.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedBoolean> preserveAlphaAnimated() { return m_preserveAlpha.animatedProperty(attributeOwnerProxy()); }

private:
    SVGFEConvolveMatrixElement(const QualifiedName&, Document&);

    using AttributeOwnerProxy = SVGAttributeOwnerProxyImpl<SVGFEConvolveMatrixElement, SVGFilterPrimitiveStandardAttributes>;
    static AttributeOwnerProxy::AttributeRegistry& attributeRegistry() { return AttributeOwnerProxy::attributeRegistry(); }
    static bool isKnownAttribute(const QualifiedName& attributeName) { return AttributeOwnerProxy::isKnownAttribute(attributeName); }
    static void registerAttributes();

    const SVGAttributeOwnerProxy& attributeOwnerProxy() const final { return m_attributeOwnerProxy; }
    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    void svgAttributeChanged(const QualifiedName&) override;

    bool setFilterEffectAttribute(FilterEffect*, const QualifiedName&) override;
    RefPtr<FilterEffect> build(SVGFilterBuilder*, Filter&) override;

    static const AtomicString& orderXIdentifier();
    static const AtomicString& orderYIdentifier();
    static const AtomicString& kernelUnitLengthXIdentifier();
    static const AtomicString& kernelUnitLengthYIdentifier();

    AttributeOwnerProxy m_attributeOwnerProxy { *this };
    SVGAnimatedStringAttribute m_in1;
    SVGAnimatedIntegerAttribute m_orderX;
    SVGAnimatedIntegerAttribute m_orderY;
    SVGAnimatedNumberListAttribute m_kernelMatrix;
    SVGAnimatedNumberAttribute m_divisor;
    SVGAnimatedNumberAttribute m_bias;
    SVGAnimatedIntegerAttribute m_targetX;
    SVGAnimatedIntegerAttribute m_targetY;
    SVGAnimatedEnumerationAttribute<EdgeModeType> m_edgeMode { EDGEMODE_DUPLICATE };
    SVGAnimatedNumberAttribute m_kernelUnitLengthX;
    SVGAnimatedNumberAttribute m_kernelUnitLengthY;
    SVGAnimatedBooleanAttribute m_preserveAlpha;
};

} // namespace WebCore
