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

#include "FEMorphology.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedNumber.h"
#include "SVGFilterPrimitiveStandardAttributes.h"

namespace WebCore {

template<>
struct SVGPropertyTraits<MorphologyOperatorType> {
    static unsigned highestEnumValue() { return FEMORPHOLOGY_OPERATOR_DILATE; }

    static String toString(MorphologyOperatorType type)
    {
        switch (type) {
        case FEMORPHOLOGY_OPERATOR_UNKNOWN:
            return emptyString();
        case FEMORPHOLOGY_OPERATOR_ERODE:
            return "erode"_s;
        case FEMORPHOLOGY_OPERATOR_DILATE:
            return "dilate"_s;
        }

        ASSERT_NOT_REACHED();
        return emptyString();
    }

    static MorphologyOperatorType fromString(const String& value)
    {
        if (value == "erode")
            return FEMORPHOLOGY_OPERATOR_ERODE;
        if (value == "dilate")
            return FEMORPHOLOGY_OPERATOR_DILATE;
        return FEMORPHOLOGY_OPERATOR_UNKNOWN;
    }
};

class SVGFEMorphologyElement final : public SVGFilterPrimitiveStandardAttributes {
    WTF_MAKE_ISO_ALLOCATED(SVGFEMorphologyElement);
public:
    static Ref<SVGFEMorphologyElement> create(const QualifiedName&, Document&);

    void setRadius(float radiusX, float radiusY);

    String in1() const { return m_in1.currentValue(attributeOwnerProxy()); }
    MorphologyOperatorType svgOperator() const { return m_svgOperator.currentValue(attributeOwnerProxy()); }
    float radiusX() const { return m_radiusX.currentValue(attributeOwnerProxy()); }
    float radiusY() const { return m_radiusY.currentValue(attributeOwnerProxy()); }

    RefPtr<SVGAnimatedString> in1Animated() { return m_in1.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedEnumeration> svgOperatorAnimated() { return m_svgOperator.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedNumber> radiusXAnimated() { return m_radiusX.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedNumber> radiusYAnimated() { return m_radiusY.animatedProperty(attributeOwnerProxy()); }

private:
    SVGFEMorphologyElement(const QualifiedName&, Document&);

    using AttributeOwnerProxy = SVGAttributeOwnerProxyImpl<SVGFEMorphologyElement, SVGFilterPrimitiveStandardAttributes>;
    static AttributeOwnerProxy::AttributeRegistry& attributeRegistry() { return AttributeOwnerProxy::attributeRegistry(); }
    static bool isKnownAttribute(const QualifiedName& attributeName) { return AttributeOwnerProxy::isKnownAttribute(attributeName); }
    static void registerAttributes();

    const SVGAttributeOwnerProxy& attributeOwnerProxy() const final { return m_attributeOwnerProxy; }
    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    void svgAttributeChanged(const QualifiedName&) override;

    bool setFilterEffectAttribute(FilterEffect*, const QualifiedName&) override;
    RefPtr<FilterEffect> build(SVGFilterBuilder*, Filter&) override;

    static const AtomicString& radiusXIdentifier();
    static const AtomicString& radiusYIdentifier();

    AttributeOwnerProxy m_attributeOwnerProxy { *this };
    SVGAnimatedStringAttribute m_in1;
    SVGAnimatedEnumerationAttribute<MorphologyOperatorType> m_svgOperator { FEMORPHOLOGY_OPERATOR_ERODE };
    SVGAnimatedNumberAttribute m_radiusX;
    SVGAnimatedNumberAttribute m_radiusY;
};

} // namespace WebCore
