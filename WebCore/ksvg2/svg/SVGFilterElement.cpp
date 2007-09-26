/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <wildfox@kde.org>
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

#if ENABLE(SVG) && ENABLE(SVG_EXPERIMENTAL_FEATURES)
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

ANIMATED_PROPERTY_DEFINITIONS(SVGFilterElement, int, Enumeration, enumeration, FilterUnits, filterUnits, SVGNames::filterUnitsAttr.localName(), m_filterUnits)
ANIMATED_PROPERTY_DEFINITIONS(SVGFilterElement, int, Enumeration, enumeration, PrimitiveUnits, primitiveUnits, SVGNames::primitiveUnitsAttr.localName(), m_primitiveUnits)
ANIMATED_PROPERTY_DEFINITIONS(SVGFilterElement, SVGLength, Length, length, X, x, SVGNames::xAttr.localName(), m_x)
ANIMATED_PROPERTY_DEFINITIONS(SVGFilterElement, SVGLength, Length, length, Y, y, SVGNames::yAttr.localName(), m_y)
ANIMATED_PROPERTY_DEFINITIONS(SVGFilterElement, SVGLength, Length, length, Width, width, SVGNames::widthAttr.localName(), m_width)
ANIMATED_PROPERTY_DEFINITIONS(SVGFilterElement, SVGLength, Length, length, Height, height, SVGNames::heightAttr.localName(), m_height)
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

    float _x, _y, w, h;

    if (filterBBoxMode && x().unitType() == LengthTypePercentage)
        _x = x().valueInSpecifiedUnits() / 100.0;
    else
        _x = x().value();

    if (filterBBoxMode && y().unitType() == LengthTypePercentage)
        _y = y().valueInSpecifiedUnits() / 100.0;
    else
        _y = y().value();

    if (filterBBoxMode && width().unitType() == LengthTypePercentage)
        w = width().valueInSpecifiedUnits() / 100.0;
    else
        w = width().value();

    if (filterBBoxMode && height().unitType() == LengthTypePercentage)
        h = height().valueInSpecifiedUnits() / 100.0;
    else
        h = height().value();

    m_filter->setFilterRect(FloatRect(_x, _y, w, h));

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

#endif // ENABLE(SVG)

// vim:ts=4:noet
