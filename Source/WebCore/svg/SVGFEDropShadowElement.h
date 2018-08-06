/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#include "FEDropShadow.h"
#include "SVGAnimatedNumber.h"
#include "SVGFilterPrimitiveStandardAttributes.h"

namespace WebCore {

class SVGFEDropShadowElement final : public SVGFilterPrimitiveStandardAttributes {
    WTF_MAKE_ISO_ALLOCATED(SVGFEDropShadowElement);
public:
    static Ref<SVGFEDropShadowElement> create(const QualifiedName&, Document&);
    
    void setStdDeviation(float stdDeviationX, float stdDeviationY);

    String in1() const { return m_in1.currentValue(attributeOwnerProxy()); }
    float dx() const { return m_dx.currentValue(attributeOwnerProxy()); }
    float dy() const { return m_dy.currentValue(attributeOwnerProxy()); }
    float stdDeviationX() const { return m_stdDeviationX.currentValue(attributeOwnerProxy()); }
    float stdDeviationY() const { return m_stdDeviationY.currentValue(attributeOwnerProxy()); }

    RefPtr<SVGAnimatedString> in1Animated() { return m_in1.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedNumber> dxAnimated() { return m_dx.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedNumber> dyAnimated() { return m_dy.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedNumber> stdDeviationXAnimated() { return m_stdDeviationX.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedNumber> stdDeviationYAnimated() { return m_stdDeviationY.animatedProperty(attributeOwnerProxy()); }

private:
    SVGFEDropShadowElement(const QualifiedName&, Document&);

    using AttributeOwnerProxy = SVGAttributeOwnerProxyImpl<SVGFEDropShadowElement, SVGFilterPrimitiveStandardAttributes>;
    static AttributeOwnerProxy::AttributeRegistry& attributeRegistry() { return AttributeOwnerProxy::attributeRegistry(); }
    static bool isKnownAttribute(const QualifiedName& attributeName) { return AttributeOwnerProxy::isKnownAttribute(attributeName); }
    static void registerAttributes();

    const SVGAttributeOwnerProxy& attributeOwnerProxy() const final { return m_attributeOwnerProxy; }
    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    void svgAttributeChanged(const QualifiedName&) override;

    RefPtr<FilterEffect> build(SVGFilterBuilder*, Filter&) override;

    static const AtomicString& stdDeviationXIdentifier();
    static const AtomicString& stdDeviationYIdentifier();

    AttributeOwnerProxy m_attributeOwnerProxy { *this };
    SVGAnimatedStringAttribute m_in1;
    SVGAnimatedNumberAttribute m_dx { 2 };
    SVGAnimatedNumberAttribute m_dy { 2 };
    SVGAnimatedNumberAttribute m_stdDeviationX { 2 };
    SVGAnimatedNumberAttribute m_stdDeviationY { 2 };
};
    
} // namespace WebCore
