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

#include "SVGFilterElement.h"
#include "SVGFilterEffect.h"
#include "SVGLength.h"
#include "SVGNames.h"
#include "SVGStyledElement.h"
#include "SVGUnitTypes.h"

namespace WebCore {

SVGFilterPrimitiveStandardAttributes::SVGFilterPrimitiveStandardAttributes(const QualifiedName& tagName, Document* doc)
    : SVGStyledElement(tagName, doc)
    , m_x(this, LengthModeWidth)
    , m_y(this, LengthModeHeight)
    , m_width(this, LengthModeWidth)
    , m_height(this, LengthModeHeight)
{
    // Spec: If the attribute is not specified, the effect is as if a value of "0%" were specified.
    setXBaseValue(SVGLength(this, LengthModeWidth, "0%"));
    setYBaseValue(SVGLength(this, LengthModeHeight, "0%"));

    // Spec: If the attribute is not specified, the effect is as if a value of "100%" were specified.
    setWidthBaseValue(SVGLength(this, LengthModeWidth, "100%"));
    setHeightBaseValue(SVGLength(this, LengthModeHeight, "100%"));
}

SVGFilterPrimitiveStandardAttributes::~SVGFilterPrimitiveStandardAttributes()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGFilterPrimitiveStandardAttributes, SVGLength, Length, length, X, x, SVGNames::xAttr.localName(), m_x)
ANIMATED_PROPERTY_DEFINITIONS(SVGFilterPrimitiveStandardAttributes, SVGLength, Length, length, Y, y, SVGNames::yAttr.localName(), m_y)
ANIMATED_PROPERTY_DEFINITIONS(SVGFilterPrimitiveStandardAttributes, SVGLength, Length, length, Width, width, SVGNames::widthAttr.localName(), m_width)
ANIMATED_PROPERTY_DEFINITIONS(SVGFilterPrimitiveStandardAttributes, SVGLength, Length, length, Height, height, SVGNames::heightAttr.localName(), m_height)
ANIMATED_PROPERTY_DEFINITIONS(SVGFilterPrimitiveStandardAttributes, String, String, string, Result, result, SVGNames::resultAttr.localName(), m_result)

void SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(MappedAttribute* attr)
{
    const AtomicString& value = attr->value();
    if (attr->name() == SVGNames::xAttr)
        setXBaseValue(SVGLength(this, LengthModeWidth, value));
    else if (attr->name() == SVGNames::yAttr)
        setYBaseValue(SVGLength(this, LengthModeHeight, value));
    else if (attr->name() == SVGNames::widthAttr)
        setWidthBaseValue(SVGLength(this, LengthModeWidth, value));
    else if (attr->name() == SVGNames::heightAttr)
        setHeightBaseValue(SVGLength(this, LengthModeHeight, value));
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
            _x = x().value();
        }

        if (y().unitType() == LengthTypePercentage) {
            filterEffect->setYBoundingBoxMode(true);
            _y = y().valueAsPercentage();
        } else {
            filterEffect->setYBoundingBoxMode(false);
            _y = y().value();
        }

        if (width().unitType() == LengthTypePercentage) {
            filterEffect->setWidthBoundingBoxMode(true);
            _width = width().valueAsPercentage();
        } else {
            filterEffect->setWidthBoundingBoxMode(false);
            _width = width().value();
        }

        if (height().unitType() == LengthTypePercentage) {
            filterEffect->setHeightBoundingBoxMode(true);
            _height = height().valueAsPercentage();
        } else {
            filterEffect->setHeightBoundingBoxMode(false);
            _height = height().value();
        }
    }

    filterEffect->setSubRegion(FloatRect(_x, _y, _width, _height));
    filterEffect->setResult(result());
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
