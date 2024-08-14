/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "FEComponentTransfer.h"
#include "NodeName.h"
#include "SVGElement.h"
#include <wtf/SortedArrayMap.h>

namespace WebCore {

template<>
struct SVGPropertyTraits<ComponentTransferType> {
    static unsigned highestEnumValue() { return enumToUnderlyingType(ComponentTransferType::FECOMPONENTTRANSFER_TYPE_GAMMA); }

    static String toString(ComponentTransferType type)
    {
        switch (type) {
        case ComponentTransferType::FECOMPONENTTRANSFER_TYPE_UNKNOWN:
            return emptyString();
        case ComponentTransferType::FECOMPONENTTRANSFER_TYPE_IDENTITY:
            return "identity"_s;
        case ComponentTransferType::FECOMPONENTTRANSFER_TYPE_TABLE:
            return "table"_s;
        case ComponentTransferType::FECOMPONENTTRANSFER_TYPE_DISCRETE:
            return "discrete"_s;
        case ComponentTransferType::FECOMPONENTTRANSFER_TYPE_LINEAR:
            return "linear"_s;
        case ComponentTransferType::FECOMPONENTTRANSFER_TYPE_GAMMA:
            return "gamma"_s;
        }

        ASSERT_NOT_REACHED();
        return emptyString();
    }

    static ComponentTransferType fromString(const String& value)
    {
        static constexpr std::pair<PackedASCIILiteral<uint64_t>, ComponentTransferType> mappings[] = {
            { "discrete", ComponentTransferType::FECOMPONENTTRANSFER_TYPE_DISCRETE },
            { "gamma", ComponentTransferType::FECOMPONENTTRANSFER_TYPE_GAMMA },
            { "identity", ComponentTransferType::FECOMPONENTTRANSFER_TYPE_IDENTITY },
            { "linear", ComponentTransferType::FECOMPONENTTRANSFER_TYPE_LINEAR },
            { "table", ComponentTransferType::FECOMPONENTTRANSFER_TYPE_TABLE }
        };
        static constexpr SortedArrayMap map { mappings };
        return map.get(value, ComponentTransferType::FECOMPONENTTRANSFER_TYPE_UNKNOWN);
    }
};

class SVGComponentTransferFunctionElement : public SVGElement {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(SVGComponentTransferFunctionElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(SVGComponentTransferFunctionElement);
public:
    virtual ComponentTransferChannel channel() const = 0;
    ComponentTransferFunction transferFunction() const;

    ComponentTransferType type() const { return m_type->currentValue<ComponentTransferType>(); }
    const SVGNumberList& tableValues() const { return m_tableValues->currentValue(); }
    float slope() const { return m_slope->currentValue(); }
    float intercept() const { return m_intercept->currentValue(); }
    float amplitude() const { return m_amplitude->currentValue(); }
    float exponent() const { return m_exponent->currentValue(); }
    float offset() const { return m_offset->currentValue(); }

    SVGAnimatedEnumeration& typeAnimated() { return m_type; }
    SVGAnimatedNumberList& tableValuesAnimated() { return m_tableValues; }
    SVGAnimatedNumber& slopeAnimated() { return m_slope; }
    SVGAnimatedNumber& interceptAnimated() { return m_intercept; }
    SVGAnimatedNumber& amplitudeAnimated() { return m_amplitude; }
    SVGAnimatedNumber& exponentAnimated() { return m_exponent; }
    SVGAnimatedNumber& offsetAnimated() { return m_offset; }

protected:
    SVGComponentTransferFunctionElement(const QualifiedName&, Document&);

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGComponentTransferFunctionElement, SVGElement>;

    void attributeChanged(const QualifiedName&, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason) override;
    void svgAttributeChanged(const QualifiedName&) override;

    bool rendererIsNeeded(const RenderStyle&) override { return false; }
    
private:
    Ref<SVGAnimatedEnumeration> m_type { SVGAnimatedEnumeration::create(this, ComponentTransferType::FECOMPONENTTRANSFER_TYPE_IDENTITY) };
    Ref<SVGAnimatedNumberList> m_tableValues { SVGAnimatedNumberList::create(this) };
    Ref<SVGAnimatedNumber> m_slope { SVGAnimatedNumber::create(this, 1) };
    Ref<SVGAnimatedNumber> m_intercept { SVGAnimatedNumber::create(this) };
    Ref<SVGAnimatedNumber> m_amplitude { SVGAnimatedNumber::create(this, 1) };
    Ref<SVGAnimatedNumber> m_exponent { SVGAnimatedNumber::create(this, 1) };
    Ref<SVGAnimatedNumber> m_offset { SVGAnimatedNumber::create(this) };
};

} // namespace WebCore
