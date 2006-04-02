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
#include <DeprecatedStringList.h>

#include "Attr.h"

#include <kcanvas/KCanvasFilters.h>
#include <kcanvas/device/KRenderingDevice.h>

#include "ksvg.h"
#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGFEBlendElement.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedString.h"
#include "SVGDOMImplementation.h"

using namespace WebCore;

SVGFEBlendElement::SVGFEBlendElement(const QualifiedName& tagName, Document *doc) : 
SVGFilterPrimitiveStandardAttributes(tagName, doc)
{
    m_filterEffect = 0;
}

SVGFEBlendElement::~SVGFEBlendElement()
{
    delete m_filterEffect;
}

SVGAnimatedString *SVGFEBlendElement::in1() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedString>(m_in1, dummy);
}

SVGAnimatedString *SVGFEBlendElement::in2() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedString>(m_in2, dummy);
}

SVGAnimatedEnumeration *SVGFEBlendElement::mode() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedEnumeration>(m_mode, dummy);
}

void SVGFEBlendElement::parseMappedAttribute(MappedAttribute *attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::modeAttr)
    {
        if(value == "normal")
            mode()->setBaseVal(SVG_FEBLEND_MODE_NORMAL);
        else if(value == "multiply")
            mode()->setBaseVal(SVG_FEBLEND_MODE_MULTIPLY);
        else if(value == "screen")
            mode()->setBaseVal(SVG_FEBLEND_MODE_SCREEN);
        else if(value == "darken")
            mode()->setBaseVal(SVG_FEBLEND_MODE_DARKEN);
        else if(value == "lighten")
            mode()->setBaseVal(SVG_FEBLEND_MODE_LIGHTEN);
    }
    else if (attr->name() == SVGNames::inAttr)
        in1()->setBaseVal(value.impl());
    else if (attr->name() == SVGNames::in2Attr)
        in2()->setBaseVal(value.impl());
    else
        SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(attr);
}

KCanvasFEBlend *SVGFEBlendElement::filterEffect() const
{
    if (!m_filterEffect)
        m_filterEffect = static_cast<KCanvasFEBlend *>(renderingDevice()->createFilterEffect(FE_BLEND));
    if (!m_filterEffect)
        return 0;
    m_filterEffect->setBlendMode((KCBlendModeType)(mode()->baseVal()-1));
    m_filterEffect->setIn(String(in1()->baseVal()).deprecatedString());
    m_filterEffect->setIn2(String(in2()->baseVal()).deprecatedString());
    setStandardAttributes(m_filterEffect);
    return m_filterEffect;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

