/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

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
#include "SVGFilterPrimitiveStandardAttributes.h"

#include "SVGFilterElement.h"
#include "SVGFilterEffect.h"
#include "SVGLength.h"
#include "SVGNames.h"
#include "SVGStyledElement.h"
#include "SVGUnitTypes.h"

namespace WebCore {

SVGFilterPrimitiveStandardAttributes::SVGFilterPrimitiveStandardAttributes(const QualifiedName& tagName, Document* doc)
    : SVGStyledElement(tagName, doc)
    , m_x(new SVGLength(this, LM_WIDTH, viewportElement()))
    , m_y(new SVGLength(this, LM_HEIGHT, viewportElement()))
    , m_width(new SVGLength(this, LM_WIDTH, viewportElement()))
    , m_height(new SVGLength(this, LM_HEIGHT, viewportElement()))
{
    // Spec : If the attribute is not specified, the effect is as if a value of "100%" were specified.
    m_width->setValueAsString("100%");
    m_height->setValueAsString("100%");
}

SVGFilterPrimitiveStandardAttributes::~SVGFilterPrimitiveStandardAttributes()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGFilterPrimitiveStandardAttributes, SVGLength*, Length, length, X, x, SVGNames::xAttr.localName(), m_x.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGFilterPrimitiveStandardAttributes, SVGLength*, Length, length, Y, y, SVGNames::yAttr.localName(), m_y.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGFilterPrimitiveStandardAttributes, SVGLength*, Length, length, Width, width, SVGNames::widthAttr.localName(), m_width.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGFilterPrimitiveStandardAttributes, SVGLength*, Length, length, Height, height, SVGNames::heightAttr.localName(), m_height.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGFilterPrimitiveStandardAttributes, String, String, string, Result, result, SVGNames::resultAttr.localName(), m_result)

void SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(MappedAttribute* attr)
{
    const AtomicString& value = attr->value();
    if (attr->name() == SVGNames::xAttr)
        xBaseValue()->setValueAsString(value);
    else if (attr->name() == SVGNames::yAttr)
        yBaseValue()->setValueAsString(value);
    else if (attr->name() == SVGNames::widthAttr)
        widthBaseValue()->setValueAsString(value);
    else if (attr->name() == SVGNames::heightAttr)
        heightBaseValue()->setValueAsString(value);
    else if (attr->name() == SVGNames::resultAttr)
        setResultBaseValue(value);
    else
        return SVGStyledElement::parseMappedAttribute(attr);
}

void SVGFilterPrimitiveStandardAttributes::setStandardAttributes(SVGFilterEffect* filterEffect) const
{
    ASSERT(filterEffect);
    if (!filterEffect)
        return;
    bool bbox = false;
    if (parentNode() && parentNode()->hasTagName(SVGNames::filterTag))
        bbox = static_cast<SVGFilterElement*>(parentNode())->primitiveUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX;

    x()->setBboxRelative(bbox);
    y()->setBboxRelative(bbox);
    width()->setBboxRelative(bbox);
    height()->setBboxRelative(bbox);
    float _x = x()->value(), _y = y()->value();
    float _width = width()->value(), _height = height()->value();
    if (bbox)
        filterEffect->setSubRegion(FloatRect(_x * 100.f, _y * 100.f, _width * 100.f, _height * 100.f));
    else
        filterEffect->setSubRegion(FloatRect(_x, _y, _width, _height));

    filterEffect->setResult(result());
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

