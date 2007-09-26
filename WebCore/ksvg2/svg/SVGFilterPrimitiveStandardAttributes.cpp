/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <wildfox@kde.org>
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

#if ENABLE(SVG) && ENABLE(SVG_EXPERIMENTAL_FEATURES)
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

    bool bbox = false;
    if (parentNode() && parentNode()->hasTagName(SVGNames::filterTag))
        bbox = static_cast<SVGFilterElement*>(parentNode())->primitiveUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX;

    float _x, _y, _width, _height;
  
    if (bbox) {
        _x = x().valueInSpecifiedUnits();
        _y = y().valueInSpecifiedUnits();
        _width = width().valueInSpecifiedUnits();
        _height = height().valueInSpecifiedUnits();
    } else {
        _x = x().value();
        _y = y().value();
        _width = width().value();
        _height = height().value();
    } 
    
    filterEffect->setSubRegion(FloatRect(_x, _y, _width, _height));
    filterEffect->setResult(result());
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
