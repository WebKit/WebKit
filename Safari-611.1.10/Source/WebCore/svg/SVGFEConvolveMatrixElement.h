/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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

#include "FEConvolveMatrix.h"
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

    String in1() const { return m_in1->currentValue(); }
    int orderX() const { return m_orderX->currentValue(); }
    int orderY() const { return m_orderY->currentValue(); }
    const SVGNumberList& kernelMatrix() const { return m_kernelMatrix->currentValue(); }
    float divisor() const { return m_divisor->currentValue(); }
    float bias() const { return m_bias->currentValue(); }
    int targetX() const { return m_targetX->currentValue(); }
    int targetY() const { return m_targetY->currentValue(); }
    EdgeModeType edgeMode() const { return m_edgeMode->currentValue<EdgeModeType>(); }
    float kernelUnitLengthX() const { return m_kernelUnitLengthX->currentValue(); }
    float kernelUnitLengthY() const { return m_kernelUnitLengthY->currentValue(); }
    bool preserveAlpha() const { return m_preserveAlpha->currentValue(); }

    SVGAnimatedString& in1Animated() { return m_in1; }
    SVGAnimatedInteger& orderXAnimated() { return m_orderX; }
    SVGAnimatedInteger& orderYAnimated() { return m_orderY; }
    SVGAnimatedNumberList& kernelMatrixAnimated() { return m_kernelMatrix; }
    SVGAnimatedNumber& divisorAnimated() { return m_divisor; }
    SVGAnimatedNumber& biasAnimated() { return m_bias; }
    SVGAnimatedInteger& targetXAnimated() { return m_targetX; }
    SVGAnimatedInteger& targetYAnimated() { return m_targetY; }
    SVGAnimatedEnumeration& edgeModeAnimated() { return m_edgeMode; }
    SVGAnimatedNumber& kernelUnitLengthXAnimated() { return m_kernelUnitLengthX; }
    SVGAnimatedNumber& kernelUnitLengthYAnimated() { return m_kernelUnitLengthY; }
    SVGAnimatedBoolean& preserveAlphaAnimated() { return m_preserveAlpha; }

private:
    SVGFEConvolveMatrixElement(const QualifiedName&, Document&);

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGFEConvolveMatrixElement, SVGFilterPrimitiveStandardAttributes>;
    const SVGPropertyRegistry& propertyRegistry() const final { return m_propertyRegistry; }

    void parseAttribute(const QualifiedName&, const AtomString&) override;
    void svgAttributeChanged(const QualifiedName&) override;

    bool setFilterEffectAttribute(FilterEffect*, const QualifiedName&) override;
    RefPtr<FilterEffect> build(SVGFilterBuilder*, Filter&) const override;

    PropertyRegistry m_propertyRegistry { *this };
    Ref<SVGAnimatedString> m_in1 { SVGAnimatedString::create(this) };
    Ref<SVGAnimatedInteger> m_orderX { SVGAnimatedInteger::create(this) };
    Ref<SVGAnimatedInteger> m_orderY { SVGAnimatedInteger::create(this) };
    Ref<SVGAnimatedNumberList> m_kernelMatrix { SVGAnimatedNumberList::create(this) };
    Ref<SVGAnimatedNumber> m_divisor { SVGAnimatedNumber::create(this) };
    Ref<SVGAnimatedNumber> m_bias { SVGAnimatedNumber::create(this) };
    Ref<SVGAnimatedInteger> m_targetX { SVGAnimatedInteger::create(this) };
    Ref<SVGAnimatedInteger> m_targetY { SVGAnimatedInteger::create(this) };
    Ref<SVGAnimatedEnumeration> m_edgeMode { SVGAnimatedEnumeration::create(this, EDGEMODE_DUPLICATE) };
    Ref<SVGAnimatedNumber> m_kernelUnitLengthX { SVGAnimatedNumber::create(this) };
    Ref<SVGAnimatedNumber> m_kernelUnitLengthY { SVGAnimatedNumber::create(this) };
    Ref<SVGAnimatedBoolean> m_preserveAlpha { SVGAnimatedBoolean::create(this) };
};

} // namespace WebCore
