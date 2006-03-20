/*
 Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 
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

#include <kdom/core/Attr.h>

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasFilters.h>
#include <kcanvas/device/KRenderingDevice.h>

#include "ksvg.h"
#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGFEDisplacementMapElement.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedString.h"
#include "SVGAnimatedNumber.h"
#include "SVGDOMImplementation.h"

using namespace WebCore;

SVGFEDisplacementMapElement::SVGFEDisplacementMapElement(const WebCore::QualifiedName& tagName, WebCore::Document* doc) : 
SVGFilterPrimitiveStandardAttributes(tagName, doc)
{
    m_filterEffect = 0;
}

SVGFEDisplacementMapElement::~SVGFEDisplacementMapElement()
{
    delete m_filterEffect;
}

SVGAnimatedString* SVGFEDisplacementMapElement::in1() const
{
    SVGStyledElement* dummy = 0;
    return lazy_create<SVGAnimatedString>(m_in1, dummy);
}

SVGAnimatedString* SVGFEDisplacementMapElement::in2() const
{
    SVGStyledElement* dummy = 0;
    return lazy_create<SVGAnimatedString>(m_in2, dummy);
}

SVGAnimatedEnumeration* SVGFEDisplacementMapElement::xChannelSelector() const
{
    SVGStyledElement* dummy = 0;
    return lazy_create<SVGAnimatedEnumeration>(m_xChannelSelector, dummy);
}

SVGAnimatedEnumeration* SVGFEDisplacementMapElement::yChannelSelector() const
{
    SVGStyledElement* dummy = 0;
    return lazy_create<SVGAnimatedEnumeration>(m_yChannelSelector, dummy);
}

SVGAnimatedNumber* SVGFEDisplacementMapElement::scale() const
{
    SVGStyledElement* dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_scale, dummy);
}

KCChannelSelectorType SVGFEDisplacementMapElement::stringToChannel(WebCore::String& key)
{
    if(key == "R")
        return CS_RED;
    else if(key == "G")
        return CS_GREEN;
    else if(key == "B")
        return CS_BLUE;
    else if(key == "A")
        return CS_ALPHA;
    //error
    return (KCChannelSelectorType)-1;
}

void SVGFEDisplacementMapElement::parseMappedAttribute(WebCore::MappedAttribute* attr)
{
    WebCore::String value(attr->value());
    if (attr->name() == SVGNames::xChannelSelectorAttr)
        xChannelSelector()->setBaseVal(stringToChannel(value));
    else if (attr->name() == SVGNames::yChannelSelectorAttr)
        yChannelSelector()->setBaseVal(stringToChannel(value));
    else if (attr->name() == SVGNames::inAttr)
        in1()->setBaseVal(value.impl());
    else if (attr->name() == SVGNames::in2Attr)
        in2()->setBaseVal(value.impl());
    else if (attr->name() == SVGNames::scaleAttr)
        scale()->setBaseVal(value.deprecatedString().toDouble());
    else
        SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(attr);
}

KCanvasFEDisplacementMap* SVGFEDisplacementMapElement::filterEffect() const
{
    if (!m_filterEffect)
        m_filterEffect = static_cast<KCanvasFEDisplacementMap *>(renderingDevice()->createFilterEffect(FE_DISPLACEMENT_MAP));
    if (!m_filterEffect)
        return 0;
    m_filterEffect->setXChannelSelector((KCChannelSelectorType)(xChannelSelector()->baseVal()));
    m_filterEffect->setYChannelSelector((KCChannelSelectorType)(yChannelSelector()->baseVal()));
    m_filterEffect->setIn(WebCore::String(in1()->baseVal()).deprecatedString());
    m_filterEffect->setIn2(WebCore::String(in2()->baseVal()).deprecatedString());
    m_filterEffect->setScale(scale()->baseVal());
    setStandardAttributes(m_filterEffect);
    return m_filterEffect;
}
#endif // SVG_SUPPORT
