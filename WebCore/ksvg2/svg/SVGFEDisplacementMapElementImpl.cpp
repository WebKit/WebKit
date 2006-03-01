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
#include <QStringList.h>

#include <kdom/core/AttrImpl.h>

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasFilters.h>
#include <kcanvas/device/KRenderingDevice.h>

#include "ksvg.h"
#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGFEDisplacementMapElementImpl.h"
#include "SVGAnimatedEnumerationImpl.h"
#include "SVGAnimatedStringImpl.h"
#include "SVGAnimatedNumberImpl.h"
#include "SVGDOMImplementationImpl.h"

using namespace KSVG;

SVGFEDisplacementMapElementImpl::SVGFEDisplacementMapElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl* doc) : 
SVGFilterPrimitiveStandardAttributesImpl(tagName, doc)
{
    m_filterEffect = 0;
}

SVGFEDisplacementMapElementImpl::~SVGFEDisplacementMapElementImpl()
{
    delete m_filterEffect;
}

SVGAnimatedStringImpl* SVGFEDisplacementMapElementImpl::in1() const
{
    SVGStyledElementImpl* dummy = 0;
    return lazy_create<SVGAnimatedStringImpl>(m_in1, dummy);
}

SVGAnimatedStringImpl* SVGFEDisplacementMapElementImpl::in2() const
{
    SVGStyledElementImpl* dummy = 0;
    return lazy_create<SVGAnimatedStringImpl>(m_in2, dummy);
}

SVGAnimatedEnumerationImpl* SVGFEDisplacementMapElementImpl::xChannelSelector() const
{
    SVGStyledElementImpl* dummy = 0;
    return lazy_create<SVGAnimatedEnumerationImpl>(m_xChannelSelector, dummy);
}

SVGAnimatedEnumerationImpl* SVGFEDisplacementMapElementImpl::yChannelSelector() const
{
    SVGStyledElementImpl* dummy = 0;
    return lazy_create<SVGAnimatedEnumerationImpl>(m_yChannelSelector, dummy);
}

SVGAnimatedNumberImpl* SVGFEDisplacementMapElementImpl::scale() const
{
    SVGStyledElementImpl* dummy = 0;
    return lazy_create<SVGAnimatedNumberImpl>(m_scale, dummy);
}

KCChannelSelectorType SVGFEDisplacementMapElementImpl::stringToChannel(KDOM::DOMString& key)
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

void SVGFEDisplacementMapElementImpl::parseMappedAttribute(KDOM::MappedAttributeImpl* attr)
{
    KDOM::DOMString value(attr->value());
    if (attr->name() == SVGNames::xChannelSelectorAttr)
        xChannelSelector()->setBaseVal(stringToChannel(value));
    else if (attr->name() == SVGNames::yChannelSelectorAttr)
        yChannelSelector()->setBaseVal(stringToChannel(value));
    else if (attr->name() == SVGNames::inAttr)
        in1()->setBaseVal(value.impl());
    else if (attr->name() == SVGNames::in2Attr)
        in2()->setBaseVal(value.impl());
    else if (attr->name() == SVGNames::scaleAttr)
        scale()->setBaseVal(value.qstring().toDouble());
    else
        SVGFilterPrimitiveStandardAttributesImpl::parseMappedAttribute(attr);
}

KCanvasFEDisplacementMap* SVGFEDisplacementMapElementImpl::filterEffect() const
{
    if (!m_filterEffect)
        m_filterEffect = static_cast<KCanvasFEDisplacementMap *>(renderingDevice()->createFilterEffect(FE_DISPLACEMENT_MAP));
    if (!m_filterEffect)
        return 0;
    m_filterEffect->setXChannelSelector((KCChannelSelectorType)(xChannelSelector()->baseVal()));
    m_filterEffect->setYChannelSelector((KCChannelSelectorType)(yChannelSelector()->baseVal()));
    m_filterEffect->setIn(KDOM::DOMString(in1()->baseVal()).qstring());
    m_filterEffect->setIn2(KDOM::DOMString(in2()->baseVal()).qstring());
    m_filterEffect->setScale(scale()->baseVal());
    setStandardAttributes(m_filterEffect);
    return m_filterEffect;
}
#endif // SVG_SUPPORT
