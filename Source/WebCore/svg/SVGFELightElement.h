/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "LightSource.h"
#include "SVGAnimatedNumber.h"
#include "SVGElement.h"

namespace WebCore {

class SVGFilterBuilder;

class SVGFELightElement : public SVGElement {
    WTF_MAKE_ISO_ALLOCATED(SVGFELightElement);
public:
    virtual Ref<LightSource> lightSource(SVGFilterBuilder&) const = 0;
    static SVGFELightElement* findLightElement(const SVGElement*);

    float azimuth() const { return m_azimuth.currentValue(attributeOwnerProxy()); }
    float elevation() const { return m_elevation.currentValue(attributeOwnerProxy()); }
    float x() const { return m_x.currentValue(attributeOwnerProxy()); }
    float y() const { return m_y.currentValue(attributeOwnerProxy()); }
    float z() const { return m_z.currentValue(attributeOwnerProxy()); }
    float pointsAtX() const { return m_pointsAtX.currentValue(attributeOwnerProxy()); }
    float pointsAtY() const { return m_pointsAtY.currentValue(attributeOwnerProxy()); }
    float pointsAtZ() const { return m_pointsAtZ.currentValue(attributeOwnerProxy()); }
    float specularExponent() const { return m_specularExponent.currentValue(attributeOwnerProxy()); }
    float limitingConeAngle() const { return m_limitingConeAngle.currentValue(attributeOwnerProxy()); }

    RefPtr<SVGAnimatedNumber> azimuthAnimated() { return m_azimuth.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedNumber> elevationAnimated() { return m_elevation.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedNumber> xAnimated() { return m_x.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedNumber> yAnimated() { return m_y.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedNumber> zAnimated() { return m_z.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedNumber> pointsAtXAnimated() { return m_pointsAtX.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedNumber> pointsAtYAnimated() { return m_pointsAtY.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedNumber> pointsAtZAnimated() { return m_pointsAtZ.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedNumber> specularExponentAnimated() { return m_specularExponent.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedNumber> limitingConeAngleAnimated() { return m_limitingConeAngle.animatedProperty(attributeOwnerProxy()); }

protected:
    SVGFELightElement(const QualifiedName&, Document&);

    bool rendererIsNeeded(const RenderStyle&) override { return false; }

private:
    using AttributeOwnerProxy = SVGAttributeOwnerProxyImpl<SVGFELightElement, SVGElement>;
    static auto& attributeRegistry() { return AttributeOwnerProxy::attributeRegistry(); }
    static bool isKnownAttribute(const QualifiedName& attributeName) { return AttributeOwnerProxy::isKnownAttribute(attributeName); }
    static void registerAttributes();

    const SVGAttributeOwnerProxy& attributeOwnerProxy() const final { return m_attributeOwnerProxy; }
    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    void svgAttributeChanged(const QualifiedName&) override;
    void childrenChanged(const ChildChange&) override;

    AttributeOwnerProxy m_attributeOwnerProxy { *this };
    SVGAnimatedNumberAttribute m_azimuth;
    SVGAnimatedNumberAttribute m_elevation;
    SVGAnimatedNumberAttribute m_x;
    SVGAnimatedNumberAttribute m_y;
    SVGAnimatedNumberAttribute m_z;
    SVGAnimatedNumberAttribute m_pointsAtX;
    SVGAnimatedNumberAttribute m_pointsAtY;
    SVGAnimatedNumberAttribute m_pointsAtZ;
    SVGAnimatedNumberAttribute m_specularExponent { 1 };
    SVGAnimatedNumberAttribute m_limitingConeAngle;
};

} // namespace WebCore
