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

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "SVGElement.h"
#include "SVGLightSource.h"

namespace WebCore
{
    class SVGNumberList;
    
    class SVGFELightElement : public SVGElement
    {
    public:
        SVGFELightElement(const QualifiedName&, Document*);
        virtual ~SVGFELightElement();
        
        virtual SVGLightSource* lightSource() const = 0;
        virtual void parseMappedAttribute(MappedAttribute*);

    private:
        ANIMATED_PROPERTY_DECLARATIONS(SVGFELightElement, float, float, Azimuth, azimuth)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFELightElement, float, float, Elevation, elevation)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFELightElement, float, float, X, x)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFELightElement, float, float, Y, y)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFELightElement, float, float, Z, z)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFELightElement, float, float, PointsAtX, pointsAtX)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFELightElement, float, float, PointsAtY, pointsAtY)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFELightElement, float, float, PointsAtZ, pointsAtZ)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFELightElement, float, float, SpecularExponent, specularExponent)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFELightElement, float, float, LimitingConeAngle, limitingConeAngle)
    };

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
#endif
