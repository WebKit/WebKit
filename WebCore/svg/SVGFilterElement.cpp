/*
    Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "SVGFilterElement.h"

#include "Attr.h"
#include "SVGResourceFilter.h"
#include "SVGFilterPrimitiveStandardAttributes.h"
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
    , m_x(this, LengthModeWidth)
    , m_y(this, LengthModeHeight)
    , m_width(this, LengthModeWidth)
    , m_height(this, LengthModeHeight)
    , m_filterResX(0)
    , m_filterResY(0)
{
    // Spec: If the attribute is not specified, the effect is as if a value of "-10%" were specified.
    setXBaseValue(SVGLength(this, LengthModeWidth, "-10%"));
    setYBaseValue(SVGLength(this, LengthModeHeight, "-10%"));
 
    // Spec: If the attribute is not specified, the effect is as if a value of "120%" were specified.
    setWidthBaseValue(SVGLength(this, LengthModeWidth, "120%"));
    setHeightBaseValue(SVGLength(this, LengthModeHeight, "120%"));
}

SVGFilterElement::~SVGFilterElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGFilterElement, int, Enumeration, enumeration, FilterUnits, filterUnits, SVGNames::filterUnitsAttr, m_filterUnits)
ANIMATED_PROPERTY_DEFINITIONS(SVGFilterElement, int, Enumeration, enumeration, PrimitiveUnits, primitiveUnits, SVGNames::primitiveUnitsAttr, m_primitiveUnits)
ANIMATED_PROPERTY_DEFINITIONS(SVGFilterElement, SVGLength, Length, length, X, x, SVGNames::xAttr, m_x)
ANIMATED_PROPERTY_DEFINITIONS(SVGFilterElement, SVGLength, Length, length, Y, y, SVGNames::yAttr, m_y)
ANIMATED_PROPERTY_DEFINITIONS(SVGFilterElement, SVGLength, Length, length, Width, width, SVGNames::widthAttr, m_width)
ANIMATED_PROPERTY_DEFINITIONS(SVGFilterElement, SVGLength, Length, length, Height, height, SVGNames::heightAttr, m_height)
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
        setXBaseValue(SVGLength(this, LengthModeWidth, value));
    else if (attr->name() == SVGNames::yAttr)
        setYBaseValue(SVGLength(this, LengthModeHeight, value));
    else if (attr->name() == SVGNames::widthAttr)
        setWidthBaseValue(SVGLength(this, LengthModeWidth, value));
    else if (attr->name() == SVGNames::heightAttr)
        setHeightBaseValue(SVGLength(this, LengthModeHeight, value));
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

    float _x, _y, _width, _height;

    if (filterBBoxMode) {
        _x = x().valueAsPercentage();
        _y = y().valueAsPercentage();
        _width = width().valueAsPercentage();
        _height = height().valueAsPercentage();
    } else {
        m_filter->setXBoundingBoxMode(x().unitType() == LengthTypePercentage);
        m_filter->setYBoundingBoxMode(y().unitType() == LengthTypePercentage);

        _x = x().value();
        _y = y().value();
        _width = width().value();
        _height = height().value();
    } 

    m_filter->setFilterRect(FloatRect(_x, _y, _width, _height));

    bool primitiveBBoxMode = primitiveUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX;
    m_filter->setEffectBoundingBoxMode(primitiveBBoxMode);

    // TODO : use switch/case instead?
    m_filter->clearEffects();
    for (Node* n = firstChild(); n != 0; n = n->nextSibling()) {
        SVGElement* element = 0;
        if (n->isSVGElement())
            element = static_cast<SVGElement*>(n);
        if (element && element->isFilterEffect()) {
            SVGFilterPrimitiveStandardAttributes* filterAttributes = static_cast<SVGFilterPrimitiveStandardAttributes*>(element);
            SVGFilterEffect* filterEffect = filterAttributes->filterEffect(m_filter.get());
            if (!filterEffect)
                continue;

            m_filter->addFilterEffect(filterEffect);
        }
    }

    return m_filter.get();
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
