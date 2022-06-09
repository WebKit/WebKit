/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Oliver Hunt <oliver@nerget.com>
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

#include "LightSource.h"
#include "SVGElement.h"

namespace WebCore {

class SVGFilterBuilder;

class SVGFELightElement : public SVGElement {
    WTF_MAKE_ISO_ALLOCATED(SVGFELightElement);
public:
    virtual Ref<LightSource> lightSource(SVGFilterBuilder&) const = 0;
    static SVGFELightElement* findLightElement(const SVGElement*);

    float azimuth() const { return m_azimuth->currentValue(); }
    float elevation() const { return m_elevation->currentValue(); }
    float x() const { return m_x->currentValue(); }
    float y() const { return m_y->currentValue(); }
    float z() const { return m_z->currentValue(); }
    float pointsAtX() const { return m_pointsAtX->currentValue(); }
    float pointsAtY() const { return m_pointsAtY->currentValue(); }
    float pointsAtZ() const { return m_pointsAtZ->currentValue(); }
    float specularExponent() const { return m_specularExponent->currentValue(); }
    float limitingConeAngle() const { return m_limitingConeAngle->currentValue(); }

    SVGAnimatedNumber& azimuthAnimated() { return m_azimuth; }
    SVGAnimatedNumber& elevationAnimated() { return m_elevation; }
    SVGAnimatedNumber& xAnimated() { return m_x; }
    SVGAnimatedNumber& yAnimated() { return m_y; }
    SVGAnimatedNumber& zAnimated() { return m_z; }
    SVGAnimatedNumber& pointsAtXAnimated() { return m_pointsAtX; }
    SVGAnimatedNumber& pointsAtYAnimated() { return m_pointsAtY; }
    SVGAnimatedNumber& pointsAtZAnimated() { return m_pointsAtZ; }
    SVGAnimatedNumber& specularExponentAnimated() { return m_specularExponent; }
    SVGAnimatedNumber& limitingConeAngleAnimated() { return m_limitingConeAngle; }

protected:
    SVGFELightElement(const QualifiedName&, Document&);

    bool rendererIsNeeded(const RenderStyle&) override { return false; }

private:
    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGFELightElement, SVGElement>;
    const SVGPropertyRegistry& propertyRegistry() const final { return m_propertyRegistry; }

    void parseAttribute(const QualifiedName&, const AtomString&) override;
    void svgAttributeChanged(const QualifiedName&) override;
    void childrenChanged(const ChildChange&) override;

    PropertyRegistry m_propertyRegistry { *this };
    Ref<SVGAnimatedNumber> m_azimuth { SVGAnimatedNumber::create(this) };
    Ref<SVGAnimatedNumber> m_elevation { SVGAnimatedNumber::create(this) };
    Ref<SVGAnimatedNumber> m_x { SVGAnimatedNumber::create(this) };
    Ref<SVGAnimatedNumber> m_y { SVGAnimatedNumber::create(this) };
    Ref<SVGAnimatedNumber> m_z { SVGAnimatedNumber::create(this) };
    Ref<SVGAnimatedNumber> m_pointsAtX { SVGAnimatedNumber::create(this) };
    Ref<SVGAnimatedNumber> m_pointsAtY { SVGAnimatedNumber::create(this) };
    Ref<SVGAnimatedNumber> m_pointsAtZ { SVGAnimatedNumber::create(this) };
    Ref<SVGAnimatedNumber> m_specularExponent { SVGAnimatedNumber::create(this, 1) };
    Ref<SVGAnimatedNumber> m_limitingConeAngle { SVGAnimatedNumber::create(this) };
};

} // namespace WebCore
