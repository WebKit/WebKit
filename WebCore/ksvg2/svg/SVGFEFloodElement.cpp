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
#include "SVGFEFloodElement.h"

#include "Attr.h"
#include "DeprecatedStringList.h"
#include "RenderCanvas.h"
#include "SVGAnimatedString.h"
#include "SVGDOMImplementation.h"
#include "SVGHelper.h"
#include "SVGNames.h"
#include "SVGRenderStyle.h"
#include <kcanvas/KCanvasFilters.h>
#include <kcanvas/device/KRenderingDevice.h>

using namespace WebCore;

SVGFEFloodElement::SVGFEFloodElement(const QualifiedName& tagName, Document *doc) : 
SVGFilterPrimitiveStandardAttributes(tagName, doc)
{
    m_filterEffect = 0;
}

SVGFEFloodElement::~SVGFEFloodElement()
{
    delete m_filterEffect;
}

SVGAnimatedString *SVGFEFloodElement::in1() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedString>(m_in1, dummy);
}

void SVGFEFloodElement::parseMappedAttribute(MappedAttribute *attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::inAttr)
        in1()->setBaseVal(value.impl());
    else
        SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(attr);
}

KCanvasFEFlood *SVGFEFloodElement::filterEffect() const
{
    if (!m_filterEffect)
        m_filterEffect = static_cast<KCanvasFEFlood *>(renderingDevice()->createFilterEffect(FE_FLOOD));
    if (!m_filterEffect)
        return 0;
    m_filterEffect->setIn(String(in1()->baseVal()).deprecatedString());
    setStandardAttributes(m_filterEffect);
    RenderStyle *filterStyle = const_cast<SVGFEFloodElement *>(this)->createStyleForRenderer(parentNode()->renderer());
    const SVGRenderStyle *svgStyle = filterStyle->svgStyle();
    m_filterEffect->setFloodColor(svgStyle->floodColor());
    m_filterEffect->setFloodOpacity(svgStyle->floodOpacity());
    filterStyle->deref(canvas()->renderArena());

    return m_filterEffect;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

