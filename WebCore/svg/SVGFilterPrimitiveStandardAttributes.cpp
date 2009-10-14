/*
    Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>
                  2009 Dirk Schulze <krit@webkit.org>

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
#include "SVGFilterPrimitiveStandardAttributes.h"

#include "FilterEffect.h"
#include "MappedAttribute.h"
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

void SVGFilterPrimitiveStandardAttributes::setStandardAttributes(SVGResourceFilter* resourceFilter, FilterEffect* filterEffect) const
{
    ASSERT(filterEffect);
    if (!filterEffect)
        return;

    ASSERT(resourceFilter);

    if (this->hasAttribute(SVGNames::xAttr))
        filterEffect->setHasX(true);
    if (this->hasAttribute(SVGNames::yAttr))
        filterEffect->setHasY(true);
    if (this->hasAttribute(SVGNames::widthAttr))
        filterEffect->setHasWidth(true);
    if (this->hasAttribute(SVGNames::heightAttr))
        filterEffect->setHasHeight(true);

    FloatRect effectBBox;
    if (resourceFilter->effectBoundingBoxMode())
        effectBBox = FloatRect(x().valueAsPercentage(),
                               y().valueAsPercentage(),
                               width().valueAsPercentage(),
                               height().valueAsPercentage());
    else
        effectBBox = FloatRect(x().value(this),
                               y().value(this),
                               width().value(this),
                               height().value(this));

    filterEffect->setSubRegion(effectBBox);
}

}

#endif // ENABLE(SVG)
