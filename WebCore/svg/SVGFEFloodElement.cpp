/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2007, 2008 Rob Buis <buis@kde.org>

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
#include "SVGFEFloodElement.h"

#include "MappedAttribute.h"
#include "RenderStyle.h"
#include "SVGRenderStyle.h"
#include "SVGResourceFilter.h"

namespace WebCore {

SVGFEFloodElement::SVGFEFloodElement(const QualifiedName& tagName, Document* doc)
    : SVGFilterPrimitiveStandardAttributes(tagName, doc)
    , m_in1(this, SVGNames::inAttr)
{
}

SVGFEFloodElement::~SVGFEFloodElement()
{
}

void SVGFEFloodElement::parseMappedAttribute(MappedAttribute* attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::inAttr)
        setIn1BaseValue(value);
    else
        SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(attr);
}

bool SVGFEFloodElement::build(SVGResourceFilter* filterResource)
{
    FilterEffect* input = filterResource->builder()->getEffectById(in1());

    if(!input)
        return false;

    RefPtr<RenderStyle> filterStyle = styleForRenderer();

    Color color = filterStyle->svgStyle()->floodColor();
    float opacity = filterStyle->svgStyle()->floodOpacity();

    RefPtr<FilterEffect> effect = FEFlood::create(input, color, opacity);
    filterResource->addFilterEffect(this, effect.release());
    
    return true;
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
