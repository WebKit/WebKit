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
#include "MappedAttribute.h"
#include "SVGFilterPrimitiveStandardAttributes.h"
#include "SVGLength.h"
#include "SVGNames.h"
#include "SVGResourceFilter.h"
#include "SVGUnitTypes.h"

namespace WebCore {

char SVGFilterResXIdentifier[] = "SVGFilterResX";
char SVGFilterResYIdentifier[] = "SVGFilterResY";

SVGFilterElement::SVGFilterElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledElement(tagName, doc)
    , SVGURIReference()
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , m_filterUnits(this, SVGNames::filterUnitsAttr, SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
    , m_primitiveUnits(this, SVGNames::primitiveUnitsAttr, SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE)
    , m_x(this, SVGNames::xAttr, LengthModeWidth, "-10%")
    , m_y(this, SVGNames::yAttr, LengthModeHeight, "-10%")
    , m_width(this, SVGNames::widthAttr, LengthModeWidth, "120%")
    , m_height(this, SVGNames::heightAttr, LengthModeHeight, "120%")
    , m_filterResX(this, SVGNames::filterResAttr)
    , m_filterResY(this, SVGNames::filterResAttr)
{
    // Spec: If the x/y attribute is not specified, the effect is as if a value of "-10%" were specified.
    // Spec: If the width/height attribute is not specified, the effect is as if a value of "120%" were specified.
}

SVGFilterElement::~SVGFilterElement()
{
}

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
        setXBaseValue(SVGLength(LengthModeWidth, value));
    else if (attr->name() == SVGNames::yAttr)
        setYBaseValue(SVGLength(LengthModeHeight, value));
    else if (attr->name() == SVGNames::widthAttr)
        setWidthBaseValue(SVGLength(LengthModeWidth, value));
    else if (attr->name() == SVGNames::heightAttr)
        setHeightBaseValue(SVGLength(LengthModeHeight, value));
    else {
        if (SVGURIReference::parseMappedAttribute(attr))
            return;
        if (SVGLangSpace::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;

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

        _x = x().value(this);
        _y = y().value(this);
        _width = width().value(this);
        _height = height().value(this);
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
