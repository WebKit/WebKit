/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>
                  2005 Oliver Hunt <ojh16@student.canterbury.ac.nz>

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

#if ENABLE(SVG) && ENABLE(SVG_EXPERIMENTAL_FEATURES)
#include "SVGFELightElement.h"

#include "SVGNames.h"

namespace WebCore {

SVGFELightElement::SVGFELightElement(const QualifiedName& tagName, Document* doc)
    : SVGElement(tagName, doc)
    , m_azimuth(0.0)
    , m_elevation(0.0)
    , m_x(0.0)
    , m_y(0.0)
    , m_z(0.0)
    , m_pointsAtX(0.0)
    , m_pointsAtY(0.0)
    , m_pointsAtZ(0.0)
    , m_specularExponent(0.0)
    , m_limitingConeAngle(0.0)
{
}

SVGFELightElement::~SVGFELightElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGFELightElement, double, Number, number, Azimuth, azimuth, SVGNames::azimuthAttr.localName(), m_azimuth)
ANIMATED_PROPERTY_DEFINITIONS(SVGFELightElement, double, Number, number, Elevation, elevation, SVGNames::elevationAttr.localName(), m_elevation)
ANIMATED_PROPERTY_DEFINITIONS(SVGFELightElement, double, Number, number, X, x, SVGNames::xAttr.localName(), m_x)
ANIMATED_PROPERTY_DEFINITIONS(SVGFELightElement, double, Number, number, Y, y, SVGNames::yAttr.localName(), m_y)

ANIMATED_PROPERTY_DEFINITIONS(SVGFELightElement, double, Number, number, Z, z, SVGNames::zAttr.localName(), m_z)
ANIMATED_PROPERTY_DEFINITIONS(SVGFELightElement, double, Number, number, PointsAtX, pointsAtX, SVGNames::pointsAtXAttr.localName(), m_pointsAtX)
ANIMATED_PROPERTY_DEFINITIONS(SVGFELightElement, double, Number, number, PointsAtY, pointsAtY, SVGNames::pointsAtYAttr.localName(), m_pointsAtY)
ANIMATED_PROPERTY_DEFINITIONS(SVGFELightElement, double, Number, number, PointsAtZ, pointsAtZ, SVGNames::pointsAtZAttr.localName(), m_pointsAtZ)
ANIMATED_PROPERTY_DEFINITIONS(SVGFELightElement, double, Number, number, SpecularExponent, specularExponent, SVGNames::specularExponentAttr.localName(), m_specularExponent)
ANIMATED_PROPERTY_DEFINITIONS(SVGFELightElement, double, Number, number, LimitingConeAngle, limitingConeAngle, SVGNames::limitingConeAngleAttr.localName(), m_limitingConeAngle)

void SVGFELightElement::parseMappedAttribute(MappedAttribute* attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::azimuthAttr)
        setAzimuthBaseValue(value.toDouble());
    else if (attr->name() == SVGNames::elevationAttr)
        setElevationBaseValue(value.toDouble());
    else if (attr->name() == SVGNames::xAttr)
        setXBaseValue(value.toDouble());
    else if (attr->name() == SVGNames::yAttr)
        setYBaseValue(value.toDouble());
    else if (attr->name() == SVGNames::zAttr)
        setZBaseValue(value.toDouble());
    else if (attr->name() == SVGNames::pointsAtXAttr)
        setPointsAtXBaseValue(value.toDouble());
    else if (attr->name() == SVGNames::pointsAtYAttr)
        setPointsAtYBaseValue(value.toDouble());
    else if (attr->name() == SVGNames::pointsAtZAttr)
        setPointsAtZBaseValue(value.toDouble());
    else if (attr->name() == SVGNames::specularExponentAttr)
        setSpecularExponentBaseValue(value.toDouble());
    else if (attr->name() == SVGNames::limitingConeAngleAttr)
        setLimitingConeAngleBaseValue(value.toDouble());
    else
        SVGElement::parseMappedAttribute(attr);
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
