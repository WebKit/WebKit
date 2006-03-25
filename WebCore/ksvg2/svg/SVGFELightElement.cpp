/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
 */

#include "config.h"
#if SVG_SUPPORT
#include <DeprecatedStringList.h>

#include "Attr.h"

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasResources.h>
#include <kcanvas/KCanvasFilters.h>
#include <kcanvas/device/KRenderingDevice.h>

#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGFELightElement.h"
#include "SVGAnimatedString.h"
#include "SVGAnimatedNumber.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGDOMImplementation.h"

using namespace WebCore;

SVGFELightElement::SVGFELightElement(const QualifiedName& tagName, Document *doc) : 
SVGElement(tagName, doc)
{
}

SVGFELightElement::~SVGFELightElement()
{
}

SVGAnimatedNumber *SVGFELightElement::azimuth() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_azimuth, dummy);
}

SVGAnimatedNumber *SVGFELightElement::elevation() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_elevation, dummy);
}

SVGAnimatedNumber *SVGFELightElement::x() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_x, dummy);
}

SVGAnimatedNumber *SVGFELightElement::y() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_y, dummy);
}


SVGAnimatedNumber *SVGFELightElement::z() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_z, dummy);
}

SVGAnimatedNumber *SVGFELightElement::pointsAtX() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_pointsAtX, dummy);
}

SVGAnimatedNumber *SVGFELightElement::pointsAtY() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_pointsAtY, dummy);
}

SVGAnimatedNumber *SVGFELightElement::pointsAtZ() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_pointsAtZ, dummy);
}

SVGAnimatedNumber *SVGFELightElement::specularExponent() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_specularExponent, dummy);
}

SVGAnimatedNumber *SVGFELightElement::limitingConeAngle() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_limitingConeAngle, dummy);
}

void SVGFELightElement::parseMappedAttribute(MappedAttribute *attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::azimuthAttr)
        azimuth()->setBaseVal(value.deprecatedString().toDouble());
    else if (attr->name() == SVGNames::elevationAttr)
        elevation()->setBaseVal(value.deprecatedString().toDouble());
    else if (attr->name() == SVGNames::xAttr)
        x()->setBaseVal(value.deprecatedString().toDouble());
    else if (attr->name() == SVGNames::yAttr)
        y()->setBaseVal(value.deprecatedString().toDouble());
    else if (attr->name() == SVGNames::zAttr)
        z()->setBaseVal(value.deprecatedString().toDouble());
    else if (attr->name() == SVGNames::pointsAtXAttr)
        pointsAtX()->setBaseVal(value.deprecatedString().toDouble());
    else if (attr->name() == SVGNames::pointsAtYAttr)
        pointsAtY()->setBaseVal(value.deprecatedString().toDouble());
    else if (attr->name() == SVGNames::pointsAtZAttr)
        pointsAtZ()->setBaseVal(value.deprecatedString().toDouble());
    else if (attr->name() == SVGNames::specularExponentAttr)
        specularExponent()->setBaseVal(value.deprecatedString().toDouble());
    else if (attr->name() == SVGNames::limitingConeAngleAttr)
        limitingConeAngle()->setBaseVal(value.deprecatedString().toDouble());
    else
        SVGElement::parseMappedAttribute(attr);
}
#endif // SVG_SUPPORT

