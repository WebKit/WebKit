/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "SVGFEOffsetElement.h"

#include "Attr.h"
#include "MappedAttribute.h"
#include "SVGResourceFilter.h"

namespace WebCore {

SVGFEOffsetElement::SVGFEOffsetElement(const QualifiedName& tagName, Document* doc)
    : SVGFilterPrimitiveStandardAttributes(tagName, doc)
    , m_in1(this, SVGNames::inAttr)
    , m_dx(this, SVGNames::dxAttr)
    , m_dy(this, SVGNames::dyAttr)
    , m_filterEffect(0)
{
}

SVGFEOffsetElement::~SVGFEOffsetElement()
{
}

void SVGFEOffsetElement::parseMappedAttribute(MappedAttribute* attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::dxAttr)
        setDxBaseValue(value.toFloat());
    else if (attr->name() == SVGNames::dyAttr)
        setDyBaseValue(value.toFloat());
    else if (attr->name() == SVGNames::inAttr)
        setIn1BaseValue(value);
    else
        SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(attr);
}

SVGFilterEffect* SVGFEOffsetElement::filterEffect(SVGResourceFilter* filter) const
{
    ASSERT_NOT_REACHED();
    return 0;
}

bool SVGFEOffsetElement::build(FilterBuilder* builder)
{
    FilterEffect* input1 = builder->getEffectById(in1());

    if(!input1)
        return false;

    builder->add(result(), FEOffset::create(input1, dx(), dy()));

    return true;
}

}

#endif // ENABLE(SVG)
