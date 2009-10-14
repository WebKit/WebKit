/*
    Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
    Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
    Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
    Copyright (C) 2009 Dirk Schulze <krit@webkit.org>

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

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "SVGFilterElement.h"

#include "Attr.h"
#include "SVGFilterBuilder.h"
#include "MappedAttribute.h"
#include "PlatformString.h"
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
    , m_href(this, XLinkNames::hrefAttr)
    , m_externalResourcesRequired(this, SVGNames::externalResourcesRequiredAttr, false)
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

void SVGFilterElement::buildFilter(const FloatRect& targetRect) const
{
    bool filterBBoxMode = filterUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX;
    bool primitiveBBoxMode = primitiveUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX;

    FloatRect filterBBox;
    if (filterBBoxMode)
        filterBBox = FloatRect(x().valueAsPercentage(),
                               y().valueAsPercentage(),
                               width().valueAsPercentage(),
                               height().valueAsPercentage());
    else
        filterBBox = FloatRect(x().value(this),
                               y().value(this),
                               width().value(this),
                               height().value(this));

    FloatRect filterRect = filterBBox;
    if (filterBBoxMode)
        filterRect = FloatRect(targetRect.x() + filterRect.x() * targetRect.width(),
                               targetRect.y() + filterRect.y() * targetRect.height(),
                               filterRect.width() * targetRect.width(),
                               filterRect.height() * targetRect.height());

    m_filter->setFilterBoundingBox(filterRect);
    m_filter->setFilterRect(filterBBox);
    m_filter->setEffectBoundingBoxMode(primitiveBBoxMode);
    m_filter->setFilterBoundingBoxMode(filterBBoxMode);

    // Add effects to the filter
    m_filter->builder()->clearEffects();
    for (Node* n = firstChild(); n != 0; n = n->nextSibling()) {
        SVGElement* element = 0;
        if (n->isSVGElement()) {
            element = static_cast<SVGElement*>(n);
            if (element->isFilterEffect()) {
                SVGFilterPrimitiveStandardAttributes* effectElement = static_cast<SVGFilterPrimitiveStandardAttributes*>(element);
                if (!effectElement->build(m_filter.get())) {
                    m_filter->builder()->clearEffects();
                    break;
                }
            }
        }
    }
}

SVGResource* SVGFilterElement::canvasResource()
{
    if (!attached())
        return 0;

    if (!m_filter)
        m_filter = SVGResourceFilter::create(this);
    return m_filter.get();
}

}

#endif // ENABLE(SVG) && ENABLE(FILTERS)
