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
#include <QStringList.h>

#include <kdom/core/AttrImpl.h>

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasFilters.h>
#include <kcanvas/device/KRenderingDevice.h>

#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGFETileElementImpl.h"
#include "SVGAnimatedStringImpl.h"
#include "SVGDOMImplementationImpl.h"

using namespace WebCore;

SVGFETileElementImpl::SVGFETileElementImpl(const QualifiedName& tagName, DocumentImpl *doc) : 
SVGFilterPrimitiveStandardAttributesImpl(tagName, doc)
{
    m_filterEffect = 0;
}

SVGFETileElementImpl::~SVGFETileElementImpl()
{
    delete m_filterEffect;
}

SVGAnimatedStringImpl *SVGFETileElementImpl::in1() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedStringImpl>(m_in1, dummy);
}

void SVGFETileElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    DOMString value(attr->value());
    if (attr->name() == SVGNames::inAttr)
        in1()->setBaseVal(value.impl());
    else
        SVGFilterPrimitiveStandardAttributesImpl::parseMappedAttribute(attr);
}

KCanvasFETile *SVGFETileElementImpl::filterEffect() const
{
    if (!m_filterEffect)
        m_filterEffect = static_cast<KCanvasFETile *>(QPainter::renderingDevice()->createFilterEffect(FE_TILE));
    if (!m_filterEffect)
        return 0;
    m_filterEffect->setIn(DOMString(in1()->baseVal()).qstring());
    setStandardAttributes(m_filterEffect);
    return m_filterEffect;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

