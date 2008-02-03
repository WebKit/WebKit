/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2007 Rob Buis <buis@kde.org>

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
#include "SVGFEFloodElement.h"

#include "Attr.h"
#include "RenderStyle.h"
#include "SVGNames.h"
#include "SVGRenderStyle.h"
#include "SVGResourceFilter.h"

namespace WebCore {

SVGFEFloodElement::SVGFEFloodElement(const QualifiedName& tagName, Document* doc)
    : SVGFilterPrimitiveStandardAttributes(tagName, doc)
    , m_filterEffect(0)
{
}

SVGFEFloodElement::~SVGFEFloodElement()
{
    delete m_filterEffect;
}

ANIMATED_PROPERTY_DEFINITIONS(SVGFEFloodElement, String, String, string, In1, in1, SVGNames::inAttr, m_in1)

void SVGFEFloodElement::parseMappedAttribute(MappedAttribute* attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::inAttr)
        setIn1BaseValue(value);
    else
        SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(attr);
}

SVGFEFlood* SVGFEFloodElement::filterEffect(SVGResourceFilter* filter) const
{
    if (!m_filterEffect)
        m_filterEffect = new SVGFEFlood(filter);
    
    m_filterEffect->setIn(in1());
    setStandardAttributes(m_filterEffect);

    SVGFEFloodElement* nonConstThis = const_cast<SVGFEFloodElement*>(this);

    RenderStyle* parentStyle = nonConstThis->styleForRenderer(parent()->renderer());
    RenderStyle* filterStyle = nonConstThis->resolveStyle(parentStyle);
    
    m_filterEffect->setFloodColor(filterStyle->svgStyle()->floodColor());
    m_filterEffect->setFloodOpacity(filterStyle->svgStyle()->floodOpacity());
    
    parentStyle->deref(document()->renderArena());
    filterStyle->deref(document()->renderArena());
    
    return m_filterEffect;
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
