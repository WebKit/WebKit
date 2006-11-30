/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
    Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
    Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>

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
#include "SVGFilterElement.h"

#include "Attr.h"
#include "SVGResourceFilter.h"
#include "SVGFilterPrimitiveStandardAttributes.h"
#include "SVGHelper.h"
#include "SVGLength.h"
#include "SVGNames.h"
#include "SVGUnitTypes.h"

namespace WebCore {

SVGFilterElement::SVGFilterElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledElement(tagName, doc)
    , SVGURIReference()
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , m_filterUnits(SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
    , m_primitiveUnits(SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE)
    , m_x(new SVGLength(this, LM_WIDTH, viewportElement()))
    , m_y(new SVGLength(this, LM_HEIGHT, viewportElement()))
    , m_width(new SVGLength(this, LM_WIDTH, viewportElement()))
    , m_height(new SVGLength(this, LM_HEIGHT, viewportElement()))
    , m_filterResX(0)
    , m_filterResY(0)

{
    // Spec: If the attribute is not specified, the effect is as if a value of "-10%" were specified.
    m_x->setValueAsString("-10%");
    m_y->setValueAsString("-10%");

    // Spec: If the attribute is not specified, the effect is as if a value of "120%" were specified.
    m_width->setValueAsString("120%");
    m_height->setValueAsString("120%");
}

SVGFilterElement::~SVGFilterElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGFilterElement, int, Enumeration, enumeration, FilterUnits, filterUnits, SVGNames::filterUnitsAttr.localName(), m_filterUnits)
ANIMATED_PROPERTY_DEFINITIONS(SVGFilterElement, int, Enumeration, enumeration, PrimitiveUnits, primitiveUnits, SVGNames::primitiveUnitsAttr.localName(), m_primitiveUnits)
ANIMATED_PROPERTY_DEFINITIONS(SVGFilterElement, SVGLength*, Length, length, X, x, SVGNames::xAttr.localName(), m_x.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGFilterElement, SVGLength*, Length, length, Y, y, SVGNames::yAttr.localName(), m_y.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGFilterElement, SVGLength*, Length, length, Width, width, SVGNames::widthAttr.localName(), m_width.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGFilterElement, SVGLength*, Length, length, Height, height, SVGNames::heightAttr.localName(), m_height.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGFilterElement, long, Integer, integer, FilterResX, filterResX, "filterResX", m_filterResX)
ANIMATED_PROPERTY_DEFINITIONS(SVGFilterElement, long, Integer, integer, FilterResY, filterResY, "filterResY", m_filterResY)

void SVGFilterElement::setFilterRes(unsigned long, unsigned long) const
{
}

void SVGFilterElement::parseMappedAttribute(MappedAttribute* attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::filterUnitsAttr) {
        if (value == "userSpaceOnUse")
            setFilterUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE);
        else if (value == "objectBoundingBox")
            setFilterUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    } else if (attr->name() == SVGNames::primitiveUnitsAttr) {
        if (value == "userSpaceOnUse")
            setPrimitiveUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE);
        else if (value == "objectBoundingBox")
            setPrimitiveUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    } else if (attr->name() == SVGNames::xAttr)
        xBaseValue()->setValueAsString(value);
    else if (attr->name() == SVGNames::yAttr)
        yBaseValue()->setValueAsString(value);
    else if (attr->name() == SVGNames::widthAttr)
        widthBaseValue()->setValueAsString(value);
    else if (attr->name() == SVGNames::heightAttr)
        heightBaseValue()->setValueAsString(value);
    else {
        if (SVGURIReference::parseMappedAttribute(attr)) return;
        if (SVGLangSpace::parseMappedAttribute(attr)) return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr)) return;

        SVGStyledElement::parseMappedAttribute(attr);
    }
}

SVGResource* SVGFilterElement::canvasResource()
{
    if (!attached())
        return 0;

    if (!m_filter)
        m_filter = new SVGResourceFilter();

    bool filterBBoxMode = filterUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX;
    m_filter->setFilterBoundingBoxMode(filterBBoxMode);
    
    x()->setBboxRelative(filterBBoxMode);
    y()->setBboxRelative(filterBBoxMode);
    width()->setBboxRelative(filterBBoxMode);
    height()->setBboxRelative(filterBBoxMode);
    m_filter->setFilterRect(FloatRect(x()->value(), y()->value(), width()->value(), height()->value()));
    
    bool primitiveBBoxMode = primitiveUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX;
    m_filter->setEffectBoundingBoxMode(primitiveBBoxMode);
    // FIXME: When does this info get passed to the filters elements?

    // TODO : use switch/case instead?
    m_filter->clearEffects();
    for (Node* n = firstChild(); n != 0; n = n->nextSibling()) {
        SVGElement* element = svg_dynamic_cast(n);
        if (element && element->isFilterEffect()) {
            SVGFilterPrimitiveStandardAttributes* fe = static_cast<SVGFilterPrimitiveStandardAttributes*>(element);
            if (fe->filterEffect())
                m_filter->addFilterEffect(fe->filterEffect());
        }
    }
    return m_filter.get();
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

