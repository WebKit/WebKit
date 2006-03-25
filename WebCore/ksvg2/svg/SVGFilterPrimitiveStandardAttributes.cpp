/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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
#if SVG_SUPPORT
#include "Attr.h"
#include <kxmlcore/Assertions.h>

#include "ksvg.h"
#include "SVGNames.h"
#include "SVGFilterPrimitiveStandardAttributes.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedString.h"
#include "SVGAnimatedLength.h"
#include "SVGStyledElement.h"
#include "SVGFilterElement.h"

#include <kcanvas/KCanvasFilters.h>

using namespace WebCore;

SVGFilterPrimitiveStandardAttributes::SVGFilterPrimitiveStandardAttributes(const QualifiedName& tagName, Document *doc)
: SVGStyledElement(tagName, doc)
{
}

SVGFilterPrimitiveStandardAttributes::~SVGFilterPrimitiveStandardAttributes()
{
}

SVGAnimatedLength *SVGFilterPrimitiveStandardAttributes::x() const
{
    // Spec : If the attribute is not specified, the effect is as if a value of "0%" were specified.
    return lazy_create<SVGAnimatedLength>(m_x, this, LM_WIDTH);
}

SVGAnimatedLength *SVGFilterPrimitiveStandardAttributes::y() const
{
    // Spec : If the attribute is not specified, the effect is as if a value of "0%" were specified.
    return lazy_create<SVGAnimatedLength>(m_y, this, LM_HEIGHT);
}

SVGAnimatedLength *SVGFilterPrimitiveStandardAttributes::width() const
{
    // Spec : If the attribute is not specified, the effect is as if a value of "100%" were specified.
    if (!m_width) {
        lazy_create<SVGAnimatedLength>(m_width, this, LM_WIDTH);
        m_width->baseVal()->setValueAsString(String("100%").impl());
    }

    return m_width.get();
}

SVGAnimatedLength *SVGFilterPrimitiveStandardAttributes::height() const
{
    // Spec : If the attribute is not specified, the effect is as if a value of "100%" were specified.
    if (!m_height) {
        lazy_create<SVGAnimatedLength>(m_height, this, LM_HEIGHT);
        m_height->baseVal()->setValueAsString(String("100%").impl());
    }

    return m_height.get();
}

SVGAnimatedString *SVGFilterPrimitiveStandardAttributes::result() const
{
    return lazy_create<SVGAnimatedString>(m_result, this);
}

void SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(MappedAttribute *attr)
{
    const AtomicString& value = attr->value();
    if (attr->name() == SVGNames::xAttr)
        x()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::yAttr)
        y()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::widthAttr)
        width()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::heightAttr)
        height()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::resultAttr)
        result()->setBaseVal(value.impl());
    else
        return SVGStyledElement::parseMappedAttribute(attr);
}

void SVGFilterPrimitiveStandardAttributes::setStandardAttributes(KCanvasFilterEffect *filterEffect) const
{
    ASSERT(filterEffect);
    if (!filterEffect)
        return;
    bool bbox = false;
    if(parentNode() && parentNode()->hasTagName(SVGNames::filterTag))
        bbox = static_cast<SVGFilterElement *>(parentNode())->primitiveUnits()->baseVal() == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX;

    x()->baseVal()->setBboxRelative(bbox);
    y()->baseVal()->setBboxRelative(bbox);
    width()->baseVal()->setBboxRelative(bbox);
    height()->baseVal()->setBboxRelative(bbox);
    float _x = x()->baseVal()->value(), _y = y()->baseVal()->value();
    float _width = width()->baseVal()->value(), _height = height()->baseVal()->value();
    if(bbox)
        filterEffect->setSubRegion(FloatRect(_x * 100.f, _y * 100.f, _width * 100.f, _height * 100.f));
    else
        filterEffect->setSubRegion(FloatRect(_x, _y, _width, _height));

    filterEffect->setResult(String(result()->baseVal()).deprecatedString());
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

