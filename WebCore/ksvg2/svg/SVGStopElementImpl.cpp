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

#include <kdom/impl/AttrImpl.h>

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasRegistry.h>
#include <kcanvas/device/KRenderingPaintServerGradient.h>

#include "svgattrs.h"
#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGDocumentImpl.h"
#include "SVGStopElementImpl.h"
#include "SVGAnimatedNumberImpl.h"
#include "SVGDOMImplementationImpl.h"

using namespace KSVG;

SVGStopElementImpl::SVGStopElementImpl(KDOM::DocumentPtr *doc, KDOM::NodeImpl::Id id, KDOM::DOMStringImpl *prefix) : SVGStyledElementImpl(doc, id, prefix)
{
    m_offset = 0;
}

SVGStopElementImpl::~SVGStopElementImpl()
{
    if(m_offset)
        m_offset->deref();
}

SVGAnimatedNumberImpl *SVGStopElementImpl::offset() const
{
    return lazy_create<SVGAnimatedNumberImpl>(m_offset, this);
}

void SVGStopElementImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
    int id = (attr->id() & NodeImpl_IdLocalMask);
    KDOM::DOMString value(attr->value());
    switch(id)
    {
        case ATTR_OFFSET:
        {
            if(value.string().endsWith(QString::fromLatin1("%")))
                offset()->setBaseVal(value.string().left(value.length() - 1).toFloat() / 100.);
            else
                offset()->setBaseVal(value.string().toFloat());
            break;
        }
        default:
            SVGStyledElementImpl::parseAttribute(attr);
    };

    if(!ownerDocument()->parsing())
    {
        SVGDocumentImpl *document = static_cast<SVGDocumentImpl *>(ownerDocument());
        KCanvas *canvas = (document ? document->canvas() : 0);
        if(canvas)
        {
            recalcStyle(Force);
            createCanvasItem(canvas, 0);
            
            SVGStyledElementImpl *parentStyled = static_cast<SVGStyledElementImpl *>(parentNode());
            if(parentStyled)
                parentStyled->notifyAttributeChange();
        }
    }
}

KCanvasItem *SVGStopElementImpl::createCanvasItem(KCanvas *canvas, KRenderingStyle *) const
{
    if(renderStyle())
    {
        QString gradientId = KDOM::DOMString(static_cast<SVGElementImpl *>(parentNode())->getId()).string();
        KRenderingPaintServer *paintServer = canvas->registry()->getPaintServerById(gradientId);
        KRenderingPaintServerGradient *paintServerGradient  = static_cast<KRenderingPaintServerGradient *>(paintServer);

        float _offset = offset()->baseVal();

        QColor c = static_cast<SVGRenderStyle *>(renderStyle())->stopColor();
        float opacity = static_cast<SVGRenderStyle *>(renderStyle())->stopOpacity();

        KCSortedGradientStopList &stops = paintServerGradient->gradientStops();

        if(!ownerDocument()->parsing())
        {
            KCSortedGradientStopList::Iterator it(stops);
            for(; it.current(); ++it)
            {
                KCGradientOffsetPair *pair = it.current();
                if(pair->offset == _offset)
                    stops.remove(pair);
            }
        }
        
        stops.addStop(_offset, qRgba(c.red(), c.green(), c.blue(), int(opacity * 255.)));
    }

    return 0;
}

// vim:ts=4:noet
