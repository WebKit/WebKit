/*
    Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "SVGFilterPrimitiveStandardAttributes.h"

#include "MappedAttribute.h"
#include "SVGFilterEffect.h"
#include "SVGFilterElement.h"
#include "SVGLength.h"
#include "SVGNames.h"
#include "SVGStyledElement.h"
#include "SVGUnitTypes.h"

namespace WebCore {

char SVGFilterPrimitiveStandardAttributesIdentifierIdentifier[] = "SVGFilterPrimitiveStandardAttributesIdentifier";

SVGFilterPrimitiveStandardAttributes::SVGFilterPrimitiveStandardAttributes(const QualifiedName& tagName, Document* doc)
    : SVGStyledElement(tagName, doc)
    , m_x(this, SVGNames::xAttr, LengthModeWidth, "0%")
    , m_y(this, SVGNames::yAttr, LengthModeHeight, "0%")
    , m_width(this, SVGNames::widthAttr, LengthModeWidth, "100%")
    , m_height(this, SVGNames::heightAttr, LengthModeHeight, "100%")
    , m_result(this, SVGNames::resultAttr)
{
    // Spec: If the x/y attribute is not specified, the effect is as if a value of "0%" were specified.
    // Spec: If the width/height attribute is not specified, the effect is as if a value of "100%" were specified.
}

SVGFilterPrimitiveStandardAttributes::~SVGFilterPrimitiveStandardAttributes()
{
}

void SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(MappedAttribute* attr)
{
    const AtomicString& value = attr->value();
    if (attr->name() == SVGNames::xAttr)
        setXBaseValue(SVGLength(LengthModeWidth, value));
    else if (attr->name() == SVGNames::yAttr)
        setYBaseValue(SVGLength(LengthModeHeight, value));
    else if (attr->name() == SVGNames::widthAttr)
        setWidthBaseValue(SVGLength(LengthModeWidth, value));
    else if (attr->name() == SVGNames::heightAttr)
        setHeightBaseValue(SVGLength(LengthModeHeight, value));
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

    ASSERT(filterEffect->filter());

    float _x, _y, _width, _height;

    if (filterEffect->filter()->effectBoundingBoxMode()) {
        _x = x().valueAsPercentage();
        _y = y().valueAsPercentage();
        _width = width().valueAsPercentage();
        _height = height().valueAsPercentage();
    } else {
        // We need to resolve any percentages in filter rect space.
        if (x().unitType() == LengthTypePercentage) {
            filterEffect->setXBoundingBoxMode(true);
            _x = x().valueAsPercentage();
        } else {
            filterEffect->setXBoundingBoxMode(false);
            _x = x().value(this);
        }

        if (y().unitType() == LengthTypePercentage) {
            filterEffect->setYBoundingBoxMode(true);
            _y = y().valueAsPercentage();
        } else {
            filterEffect->setYBoundingBoxMode(false);
            _y = y().value(this);
        }

        if (width().unitType() == LengthTypePercentage) {
            filterEffect->setWidthBoundingBoxMode(true);
            _width = width().valueAsPercentage();
        } else {
            filterEffect->setWidthBoundingBoxMode(false);
            _width = width().value(this);
        }

        if (height().unitType() == LengthTypePercentage) {
            filterEffect->setHeightBoundingBoxMode(true);
            _height = height().valueAsPercentage();
        } else {
            filterEffect->setHeightBoundingBoxMode(false);
            _height = height().value(this);
        }
    }

    filterEffect->setSubRegion(FloatRect(_x, _y, _width, _height));
    filterEffect->setResult(result());
}

}

#endif // ENABLE(SVG)
