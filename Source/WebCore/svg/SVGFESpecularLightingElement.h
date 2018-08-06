/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Oliver Hunt <oliver@nerget.com>
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

#include "FESpecularLighting.h"
#include "SVGAnimatedNumber.h"
#include "SVGFELightElement.h"
#include "SVGFilterPrimitiveStandardAttributes.h"

namespace WebCore {

class SVGFESpecularLightingElement final : public SVGFilterPrimitiveStandardAttributes {
    WTF_MAKE_ISO_ALLOCATED(SVGFESpecularLightingElement);
public:
    static Ref<SVGFESpecularLightingElement> create(const QualifiedName&, Document&);
    void lightElementAttributeChanged(const SVGFELightElement*, const QualifiedName&);

    String in1() const { return m_in1.currentValue(attributeOwnerProxy()); }
    float specularConstant() const  { return m_specularConstant.currentValue(attributeOwnerProxy()); }
    float specularExponent() const { return m_specularExponent.currentValue(attributeOwnerProxy()); }
    float surfaceScale() const { return m_surfaceScale.currentValue(attributeOwnerProxy()); }
    float kernelUnitLengthX() const { return m_kernelUnitLengthX.currentValue(attributeOwnerProxy()); }
    float kernelUnitLengthY() const { return m_kernelUnitLengthY.currentValue(attributeOwnerProxy()); }

    RefPtr<SVGAnimatedString> in1Animated() { return m_in1.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedNumber> specularConstantAnimated() { return m_specularConstant.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedNumber> specularExponentAnimated() { return m_specularExponent.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedNumber> surfaceScaleAnimated() { return m_surfaceScale.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedNumber> kernelUnitLengthXAnimated() { return m_kernelUnitLengthX.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedNumber> kernelUnitLengthYAnimated() { return m_kernelUnitLengthY.animatedProperty(attributeOwnerProxy()); }

private:
    SVGFESpecularLightingElement(const QualifiedName&, Document&);

    using AttributeOwnerProxy = SVGAttributeOwnerProxyImpl<SVGFESpecularLightingElement, SVGFilterPrimitiveStandardAttributes>;
    static AttributeOwnerProxy::AttributeRegistry& attributeRegistry() { return AttributeOwnerProxy::attributeRegistry(); }
    static bool isKnownAttribute(const QualifiedName& attributeName) { return AttributeOwnerProxy::isKnownAttribute(attributeName); }
    static void registerAttributes();

    const SVGAttributeOwnerProxy& attributeOwnerProxy() const final { return m_attributeOwnerProxy; }
    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    void svgAttributeChanged(const QualifiedName&) override;

    bool setFilterEffectAttribute(FilterEffect*, const QualifiedName&) override;
    RefPtr<FilterEffect> build(SVGFilterBuilder*, Filter&) override;

    static const AtomicString& kernelUnitLengthXIdentifier();
    static const AtomicString& kernelUnitLengthYIdentifier();

    AttributeOwnerProxy m_attributeOwnerProxy { *this };
    SVGAnimatedStringAttribute m_in1;
    SVGAnimatedNumberAttribute m_specularConstant { 1 };
    SVGAnimatedNumberAttribute m_specularExponent { 1 };
    SVGAnimatedNumberAttribute m_surfaceScale { 1 };
    SVGAnimatedNumberAttribute m_kernelUnitLengthX;
    SVGAnimatedNumberAttribute m_kernelUnitLengthY;
};

} // namespace WebCore
