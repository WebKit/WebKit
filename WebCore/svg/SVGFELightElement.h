/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>
                  2005 Oliver Hunt <oliver@nerget.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
 */

#ifndef SVGFELightElement_h
#define SVGFELightElement_h

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "SVGElement.h"
#include "SVGLightSource.h"
#include "SVGNames.h"

namespace WebCore {

    class SVGFELightElement : public SVGElement {
    public:
        SVGFELightElement(const QualifiedName&, Document*);
        virtual ~SVGFELightElement();
        
        virtual PassRefPtr<LightSource> lightSource() const = 0;
        virtual void parseMappedAttribute(MappedAttribute*);
        virtual void synchronizeProperty(const QualifiedName&);

    private:
        DECLARE_ANIMATED_PROPERTY(SVGFELightElement, SVGNames::azimuthAttr, float, Azimuth, azimuth)
        DECLARE_ANIMATED_PROPERTY(SVGFELightElement, SVGNames::elevationAttr, float, Elevation, elevation)
        DECLARE_ANIMATED_PROPERTY(SVGFELightElement, SVGNames::xAttr, float, X, x)
        DECLARE_ANIMATED_PROPERTY(SVGFELightElement, SVGNames::yAttr, float, Y, y)
        DECLARE_ANIMATED_PROPERTY(SVGFELightElement, SVGNames::zAttr, float, Z, z)
        DECLARE_ANIMATED_PROPERTY(SVGFELightElement, SVGNames::pointsAtXAttr, float, PointsAtX, pointsAtX)
        DECLARE_ANIMATED_PROPERTY(SVGFELightElement, SVGNames::pointsAtYAttr, float, PointsAtY, pointsAtY)
        DECLARE_ANIMATED_PROPERTY(SVGFELightElement, SVGNames::pointsAtZAttr, float, PointsAtZ, pointsAtZ)
        DECLARE_ANIMATED_PROPERTY(SVGFELightElement, SVGNames::specularExponentAttr, float, SpecularExponent, specularExponent)
        DECLARE_ANIMATED_PROPERTY(SVGFELightElement, SVGNames::limitingConeAngleAttr, float, LimitingConeAngle, limitingConeAngle)
    };

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(FILTERS)
#endif
