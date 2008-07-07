/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "config.h"

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "SVGFELightElement.h"

#include "SVGNames.h"

namespace WebCore {

SVGFELightElement::SVGFELightElement(const QualifiedName& tagName, Document* doc)
    : SVGElement(tagName, doc)
    , m_azimuth(0.0f)
    , m_elevation(0.0f)
    , m_x(0.0f)
    , m_y(0.0f)
    , m_z(0.0f)
    , m_pointsAtX(0.0f)
    , m_pointsAtY(0.0f)
    , m_pointsAtZ(0.0f)
    , m_specularExponent(1.0f)
    , m_limitingConeAngle(0.0f)
{
}

SVGFELightElement::~SVGFELightElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGFELightElement, float, Azimuth, azimuth, SVGNames::azimuthAttr)
ANIMATED_PROPERTY_DEFINITIONS(SVGFELightElement, float, Elevation, elevation, SVGNames::elevationAttr)
ANIMATED_PROPERTY_DEFINITIONS(SVGFELightElement, float, X, x, SVGNames::xAttr)
ANIMATED_PROPERTY_DEFINITIONS(SVGFELightElement, float, Y, y, SVGNames::yAttr)

ANIMATED_PROPERTY_DEFINITIONS(SVGFELightElement, float, Z, z, SVGNames::zAttr)
ANIMATED_PROPERTY_DEFINITIONS(SVGFELightElement, float, PointsAtX, pointsAtX, SVGNames::pointsAtXAttr)
ANIMATED_PROPERTY_DEFINITIONS(SVGFELightElement, float, PointsAtY, pointsAtY, SVGNames::pointsAtYAttr)
ANIMATED_PROPERTY_DEFINITIONS(SVGFELightElement, float, PointsAtZ, pointsAtZ, SVGNames::pointsAtZAttr)
ANIMATED_PROPERTY_DEFINITIONS(SVGFELightElement, float, SpecularExponent, specularExponent, SVGNames::specularExponentAttr)
ANIMATED_PROPERTY_DEFINITIONS(SVGFELightElement, float, LimitingConeAngle, limitingConeAngle, SVGNames::limitingConeAngleAttr)

void SVGFELightElement::parseMappedAttribute(MappedAttribute* attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::azimuthAttr)
        setAzimuthBaseValue(value.toFloat());
    else if (attr->name() == SVGNames::elevationAttr)
        setElevationBaseValue(value.toFloat());
    else if (attr->name() == SVGNames::xAttr)
        setXBaseValue(value.toFloat());
    else if (attr->name() == SVGNames::yAttr)
        setYBaseValue(value.toFloat());
    else if (attr->name() == SVGNames::zAttr)
        setZBaseValue(value.toFloat());
    else if (attr->name() == SVGNames::pointsAtXAttr)
        setPointsAtXBaseValue(value.toFloat());
    else if (attr->name() == SVGNames::pointsAtYAttr)
        setPointsAtYBaseValue(value.toFloat());
    else if (attr->name() == SVGNames::pointsAtZAttr)
        setPointsAtZBaseValue(value.toFloat());
    else if (attr->name() == SVGNames::specularExponentAttr)
        setSpecularExponentBaseValue(value.toFloat());
    else if (attr->name() == SVGNames::limitingConeAngleAttr)
        setLimitingConeAngleBaseValue(value.toFloat());
    else
        SVGElement::parseMappedAttribute(attr);
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
