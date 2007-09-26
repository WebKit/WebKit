/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
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

#if ENABLE(SVG) && ENABLE(SVG_EXPERIMENTAL_FEATURES)
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

ANIMATED_PROPERTY_DEFINITIONS(SVGFEFloodElement, String, String, string, In1, in1, SVGNames::inAttr.localName(), m_in1)

void SVGFEFloodElement::parseMappedAttribute(MappedAttribute* attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::inAttr)
        setIn1BaseValue(value);
    else
        SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(attr);
}

SVGFEFlood* SVGFEFloodElement::filterEffect() const
{
    if (!m_filterEffect)
        m_filterEffect = static_cast<SVGFEFlood*>(SVGResourceFilter::createFilterEffect(FE_FLOOD));
    if (!m_filterEffect)
        return 0;
    m_filterEffect->setIn(in1());
    setStandardAttributes(m_filterEffect);
    RenderStyle* filterStyle = const_cast<SVGFEFloodElement *>(this)->styleForRenderer(parentNode()->renderer());
    const SVGRenderStyle* svgStyle = filterStyle->svgStyle();
    m_filterEffect->setFloodColor(svgStyle->floodColor());
    m_filterEffect->setFloodOpacity(svgStyle->floodOpacity());
    filterStyle->deref(document()->renderArena());

    return m_filterEffect;
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
