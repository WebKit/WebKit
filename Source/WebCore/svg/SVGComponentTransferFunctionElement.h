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
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedNumberList.h"
#include "SVGElement.h"

namespace WebCore {

template<>
struct SVGPropertyTraits<ComponentTransferType> {
    static unsigned highestEnumValue() { return FECOMPONENTTRANSFER_TYPE_GAMMA; }

    static String toString(ComponentTransferType type)
    {
        switch (type) {
        case FECOMPONENTTRANSFER_TYPE_UNKNOWN:
            return emptyString();
        case FECOMPONENTTRANSFER_TYPE_IDENTITY:
            return "identity"_s;
        case FECOMPONENTTRANSFER_TYPE_TABLE:
            return "table"_s;
        case FECOMPONENTTRANSFER_TYPE_DISCRETE:
            return "discrete"_s;
        case FECOMPONENTTRANSFER_TYPE_LINEAR:
            return "linear"_s;
        case FECOMPONENTTRANSFER_TYPE_GAMMA:
            return "gamma"_s;
        }

        ASSERT_NOT_REACHED();
        return emptyString();
    }

    static ComponentTransferType fromString(const String& value)
    {
        if (value == "identity")
            return FECOMPONENTTRANSFER_TYPE_IDENTITY;
        if (value == "table")
            return FECOMPONENTTRANSFER_TYPE_TABLE;
        if (value == "discrete")
            return FECOMPONENTTRANSFER_TYPE_DISCRETE;
        if (value == "linear")
            return FECOMPONENTTRANSFER_TYPE_LINEAR;
        if (value == "gamma")
            return FECOMPONENTTRANSFER_TYPE_GAMMA;
        return FECOMPONENTTRANSFER_TYPE_UNKNOWN;
    }
};

class SVGComponentTransferFunctionElement : public SVGElement {
    WTF_MAKE_ISO_ALLOCATED(SVGComponentTransferFunctionElement);
public:
    ComponentTransferFunction transferFunction() const;

    ComponentTransferType type() const { return m_type.currentValue(attributeOwnerProxy()); }
    const SVGNumberListValues& tableValues() const { return m_tableValues.currentValue(attributeOwnerProxy()); }
    float slope() const { return m_slope->currentValue(); }
    float intercept() const { return m_intercept->currentValue(); }
    float amplitude() const { return m_amplitude->currentValue(); }
    float exponent() const { return m_exponent->currentValue(); }
    float offset() const { return m_offset->currentValue(); }

    RefPtr<SVGAnimatedEnumeration> typeAnimated() { return m_type.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedNumberList> tableValuesAnimated() { return m_tableValues.animatedProperty(attributeOwnerProxy()); }
    SVGAnimatedNumber& slopeAnimated() { return m_slope; }
    SVGAnimatedNumber& interceptAnimated() { return m_intercept; }
    SVGAnimatedNumber& amplitudeAnimated() { return m_amplitude; }
    SVGAnimatedNumber& exponentAnimated() { return m_exponent; }
    SVGAnimatedNumber& offsetAnimated() { return m_offset; }

protected:
    SVGComponentTransferFunctionElement(const QualifiedName&, Document&);

    using AttributeOwnerProxy = SVGAttributeOwnerProxyImpl<SVGComponentTransferFunctionElement, SVGElement>;
    static AttributeOwnerProxy::AttributeRegistry& attributeRegistry() { return AttributeOwnerProxy::attributeRegistry(); }
    static void registerAttributes();
    const SVGAttributeOwnerProxy& attributeOwnerProxy() const override { return m_attributeOwnerProxy; }

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGComponentTransferFunctionElement, SVGElement>;
    const SVGPropertyRegistry& propertyRegistry() const override { return m_propertyRegistry; }

    static bool isKnownAttribute(const QualifiedName& attributeName)
    {
        return AttributeOwnerProxy::isKnownAttribute(attributeName) || PropertyRegistry::isKnownAttribute(attributeName);
    }

    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    void svgAttributeChanged(const QualifiedName&) override;

    bool rendererIsNeeded(const RenderStyle&) override { return false; }
    
private:
    AttributeOwnerProxy m_attributeOwnerProxy { *this };
    PropertyRegistry m_propertyRegistry { *this };
    SVGAnimatedEnumerationAttribute<ComponentTransferType> m_type { FECOMPONENTTRANSFER_TYPE_IDENTITY };
    SVGAnimatedNumberListAttribute m_tableValues;
    Ref<SVGAnimatedNumber> m_slope { SVGAnimatedNumber::create(this, 1) };
    Ref<SVGAnimatedNumber> m_intercept { SVGAnimatedNumber::create(this) };
    Ref<SVGAnimatedNumber> m_amplitude { SVGAnimatedNumber::create(this, 1) };
    Ref<SVGAnimatedNumber> m_exponent { SVGAnimatedNumber::create(this, 1) };
    Ref<SVGAnimatedNumber> m_offset { SVGAnimatedNumber::create(this) };
};

} // namespace WebCore
