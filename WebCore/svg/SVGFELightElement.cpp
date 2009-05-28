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

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "SVGFELightElement.h"

#include "MappedAttribute.h"
#include "SVGNames.h"

namespace WebCore {

char SVGFELightElementIdentifier[] = "SVGFELightElement";

SVGFELightElement::SVGFELightElement(const QualifiedName& tagName, Document* doc)
    : SVGElement(tagName, doc)
    , m_azimuth(this, SVGNames::azimuthAttr)
    , m_elevation(this, SVGNames::elevationAttr)
    , m_x(this, SVGNames::xAttr)
    , m_y(this, SVGNames::yAttr)
    , m_z(this, SVGNames::zAttr)
    , m_pointsAtX(this, SVGNames::pointsAtXAttr)
    , m_pointsAtY(this, SVGNames::pointsAtYAttr)
    , m_pointsAtZ(this, SVGNames::pointsAtZAttr)
    , m_specularExponent(this, SVGNames::specularExponentAttr, 1.0f)
    , m_limitingConeAngle(this, SVGNames::limitingConeAngleAttr)
{
}

SVGFELightElement::~SVGFELightElement()
{
}

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
