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
#include <QStringList.h>

#include <kdom/core/AttrImpl.h>

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasResources.h>
#include <kcanvas/KCanvasFilters.h>
#include <kcanvas/device/KRenderingDevice.h>

#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGFELightElementImpl.h"
#include "SVGAnimatedStringImpl.h"
#include "SVGAnimatedNumberImpl.h"
#include "SVGAnimatedEnumerationImpl.h"
#include "SVGDOMImplementationImpl.h"

using namespace WebCore;

SVGFELightElementImpl::SVGFELightElementImpl(const QualifiedName& tagName, DocumentImpl *doc) : 
SVGElementImpl(tagName, doc)
{
}

SVGFELightElementImpl::~SVGFELightElementImpl()
{
}

SVGAnimatedNumberImpl *SVGFELightElementImpl::azimuth() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedNumberImpl>(m_azimuth, dummy);
}

SVGAnimatedNumberImpl *SVGFELightElementImpl::elevation() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedNumberImpl>(m_elevation, dummy);
}

SVGAnimatedNumberImpl *SVGFELightElementImpl::x() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedNumberImpl>(m_x, dummy);
}

SVGAnimatedNumberImpl *SVGFELightElementImpl::y() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedNumberImpl>(m_y, dummy);
}


SVGAnimatedNumberImpl *SVGFELightElementImpl::z() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedNumberImpl>(m_z, dummy);
}

SVGAnimatedNumberImpl *SVGFELightElementImpl::pointsAtX() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedNumberImpl>(m_pointsAtX, dummy);
}

SVGAnimatedNumberImpl *SVGFELightElementImpl::pointsAtY() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedNumberImpl>(m_pointsAtY, dummy);
}

SVGAnimatedNumberImpl *SVGFELightElementImpl::pointsAtZ() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedNumberImpl>(m_pointsAtZ, dummy);
}

SVGAnimatedNumberImpl *SVGFELightElementImpl::specularExponent() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedNumberImpl>(m_specularExponent, dummy);
}

SVGAnimatedNumberImpl *SVGFELightElementImpl::limitingConeAngle() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedNumberImpl>(m_limitingConeAngle, dummy);
}

void SVGFELightElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    DOMString value(attr->value());
    if (attr->name() == SVGNames::azimuthAttr)
        azimuth()->setBaseVal(value.qstring().toDouble());
    else if (attr->name() == SVGNames::elevationAttr)
        elevation()->setBaseVal(value.qstring().toDouble());
    else if (attr->name() == SVGNames::xAttr)
        x()->setBaseVal(value.qstring().toDouble());
    else if (attr->name() == SVGNames::yAttr)
        y()->setBaseVal(value.qstring().toDouble());
    else if (attr->name() == SVGNames::zAttr)
        z()->setBaseVal(value.qstring().toDouble());
    else if (attr->name() == SVGNames::pointsAtXAttr)
        pointsAtX()->setBaseVal(value.qstring().toDouble());
    else if (attr->name() == SVGNames::pointsAtYAttr)
        pointsAtY()->setBaseVal(value.qstring().toDouble());
    else if (attr->name() == SVGNames::pointsAtZAttr)
        pointsAtZ()->setBaseVal(value.qstring().toDouble());
    else if (attr->name() == SVGNames::specularExponentAttr)
        specularExponent()->setBaseVal(value.qstring().toDouble());
    else if (attr->name() == SVGNames::limitingConeAngleAttr)
        limitingConeAngle()->setBaseVal(value.qstring().toDouble());
    else
        SVGElementImpl::parseMappedAttribute(attr);
}
#endif // SVG_SUPPORT

