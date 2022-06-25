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

#include "CommonAtomStrings.h"
#include "FEConvolveMatrix.h"
#include "SVGFilterPrimitiveStandardAttributes.h"

namespace WebCore {

template<>
struct SVGPropertyTraits<EdgeModeType> {
    static unsigned highestEnumValue() { return static_cast<unsigned>(EdgeModeType::None); }
    static EdgeModeType initialValue() { return EdgeModeType::None; }

    static String toString(EdgeModeType type)
    {
        switch (type) {
        case EdgeModeType::Unknown:
            return emptyString();
        case EdgeModeType::Duplicate:
            return "duplicate"_s;
        case EdgeModeType::Wrap:
            return "wrap"_s;
        case EdgeModeType::None:
            return noneAtom();
        }

        ASSERT_NOT_REACHED();
        return emptyString();
    }

    static EdgeModeType fromString(const String& value)
    {
        if (value == "duplicate"_s)
            return EdgeModeType::Duplicate;
        if (value == "wrap"_s)
            return EdgeModeType::Wrap;
        if (value == noneAtom())
            return EdgeModeType::None;
        return EdgeModeType::Unknown;
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

    bool setFilterEffectAttribute(FilterEffect&, const QualifiedName&) override;
    Vector<AtomString> filterEffectInputsNames() const override { return { AtomString { in1() } }; }
    RefPtr<FilterEffect> createFilterEffect(const FilterEffectVector&, const GraphicsContext& destinationContext) const override;

    PropertyRegistry m_propertyRegistry { *this };
    Ref<SVGAnimatedString> m_in1 { SVGAnimatedString::create(this) };
    Ref<SVGAnimatedInteger> m_orderX { SVGAnimatedInteger::create(this) };
    Ref<SVGAnimatedInteger> m_orderY { SVGAnimatedInteger::create(this) };
    Ref<SVGAnimatedNumberList> m_kernelMatrix { SVGAnimatedNumberList::create(this) };
    Ref<SVGAnimatedNumber> m_divisor { SVGAnimatedNumber::create(this) };
    Ref<SVGAnimatedNumber> m_bias { SVGAnimatedNumber::create(this) };
    Ref<SVGAnimatedInteger> m_targetX { SVGAnimatedInteger::create(this) };
    Ref<SVGAnimatedInteger> m_targetY { SVGAnimatedInteger::create(this) };
    Ref<SVGAnimatedEnumeration> m_edgeMode { SVGAnimatedEnumeration::create(this, EdgeModeType::Duplicate) };
    Ref<SVGAnimatedNumber> m_kernelUnitLengthX { SVGAnimatedNumber::create(this) };
    Ref<SVGAnimatedNumber> m_kernelUnitLengthY { SVGAnimatedNumber::create(this) };
    Ref<SVGAnimatedBoolean> m_preserveAlpha { SVGAnimatedBoolean::create(this) };
};

} // namespace WebCore
