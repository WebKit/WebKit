/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

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

#ifdef SVG_SUPPORT
#include "SVGFEOffsetElement.h"

#include "Attr.h"
#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGResourceFilter.h"

namespace WebCore {

SVGFEOffsetElement::SVGFEOffsetElement(const QualifiedName& tagName, Document* doc)
    : SVGFilterPrimitiveStandardAttributes(tagName, doc)
    , m_dx(0.0)
    , m_dy(0.0)
    , m_filterEffect(0)
{
}

SVGFEOffsetElement::~SVGFEOffsetElement()
{
    delete m_filterEffect;
}

ANIMATED_PROPERTY_DEFINITIONS(SVGFEOffsetElement, String, String, string, In1, in1, SVGNames::inAttr.localName(), m_in1)
ANIMATED_PROPERTY_DEFINITIONS(SVGFEOffsetElement, double, Number, number, Dx, dx, SVGNames::dxAttr.localName(), m_dx)
ANIMATED_PROPERTY_DEFINITIONS(SVGFEOffsetElement, double, Number, number, Dy, dy, SVGNames::dyAttr.localName(), m_dy)

void SVGFEOffsetElement::parseMappedAttribute(MappedAttribute* attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::dxAttr)
        setDxBaseValue(value.toDouble());
    else if (attr->name() == SVGNames::dyAttr)
        setDyBaseValue(value.toDouble());
    else if (attr->name() == SVGNames::inAttr)
        setIn1BaseValue(value);
    else
        SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(attr);
}

SVGFEOffset* SVGFEOffsetElement::filterEffect() const
{
    if (!m_filterEffect)
        m_filterEffect = static_cast<SVGFEOffset*>(SVGResourceFilter::createFilterEffect(FE_OFFSET));
    if (!m_filterEffect)
        return 0;
    m_filterEffect->setIn(in1());
    setStandardAttributes(m_filterEffect);
    m_filterEffect->setDx(dx());
    m_filterEffect->setDy(dy());
    return m_filterEffect;
}

}

#endif // SVG_SUPPORT

// vim:ts=4:noet
