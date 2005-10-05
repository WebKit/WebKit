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
#include <kdom/core/AttrImpl.h>
#include <kdom/events/MouseEventImpl.h>
#include <kdom/events/kdomevents.h>
#include <kdom/Helper.h>

#include <kcanvas/KCanvasCreator.h>
#include <kcanvas/KCanvasContainer.h>
#include <kcanvas/device/KRenderingDevice.h>

#include "svgattrs.h"
#include "SVGHelper.h"
#include <ksvg2/KSVGPart.h>
#include "SVGDocumentImpl.h"
#include "SVGAElementImpl.h"
#include "SVGAnimatedStringImpl.h"

using namespace KSVG;

SVGAElementImpl::SVGAElementImpl(KDOM::DocumentPtr *doc, KDOM::NodeImpl::Id id, KDOM::DOMStringImpl *prefix)
: SVGStyledElementImpl(doc, id, prefix), SVGURIReferenceImpl(), SVGTestsImpl(), SVGLangSpaceImpl(), SVGExternalResourcesRequiredImpl(), SVGTransformableImpl()
{
    m_target = 0;
}

SVGAElementImpl::~SVGAElementImpl()
{
    if(m_target)
        m_target->deref();
}

SVGAnimatedStringImpl *SVGAElementImpl::target() const
{
    return lazy_create<SVGAnimatedStringImpl>(m_target, this);
}

void SVGAElementImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
    int id = (attr->id() & NodeImpl_IdLocalMask);
    KDOM::DOMString value(attr->value());
    switch(id)
    {
        case ATTR_TARGET:
        {
            target()->setBaseVal(value.handle());
            break;
        }
        default:
        {
            if(SVGURIReferenceImpl::parseAttribute(attr))
            {
                m_hasAnchor = attr->value() != 0;
                return;
            }
            if(SVGTestsImpl::parseAttribute(attr)) return;
            if(SVGLangSpaceImpl::parseAttribute(attr)) return;
            if(SVGExternalResourcesRequiredImpl::parseAttribute(attr)) return;
            if(SVGTransformableImpl::parseAttribute(attr)) return;
            
            SVGStyledElementImpl::parseAttribute(attr);
        }
    };
}

KCanvasItem *SVGAElementImpl::createCanvasItem(KCanvas *canvas, KRenderingStyle *style) const
{
    return KCanvasCreator::self()->createContainer(canvas, style);
}

void SVGAElementImpl::defaultEventHandler(KDOM::EventImpl *evt)
{
    // TODO : should use CLICK instead
    kdDebug() << k_funcinfo << endl;
    if((evt->id() == KDOM::MOUSEUP_EVENT && m_hasAnchor))
    {
        KDOM::MouseEventImpl *e = 0;
        if(evt->id() == KDOM::MOUSEUP_EVENT)
            e = static_cast<KDOM::MouseEventImpl*>(evt);

        QString url;
        QString utarget;
        if(e && e->button() == 2)
        {
            KDOM::EventTargetImpl::defaultEventHandler(evt);
            return;
        }

        url = KDOM::DOMString(KDOM::Helper::parseURL(href()->baseVal())).string();
        kdDebug() << "url : " << url << endl;
        utarget = KDOM::DOMString(getAttribute(ATTR_TARGET)).string();
        kdDebug() << "utarget : " << utarget << endl;

        if(e && e->button() == 1)
            utarget = "_blank";

        if(!evt->defaultPrevented())
        {
            int state = 0;
            int button = 0;
            if(e)
            {
                if(e->ctrlKey())
                    state |= Qt::ControlButton;
                if(e->shiftKey())
                    state |= Qt::ShiftButton;
                if(e->altKey())
                    state |= Qt::AltButton;
                if(e->metaKey())
                    state |= Qt::MetaButton;
                if(e->button() == 0)
                    button = Qt::LeftButton;
                else if(e->button() == 1)
                    button = Qt::MidButton;
                else if(e->button() == 2)
                    button = Qt::RightButton;
            }
            if(ownerDocument() && ownerDocument()->view() && ownerDocument()->part())
            {
                //getDocument()->view()->resetCursor();
                getDocument()->part()->urlSelected(url, button, state, utarget);
            }
        }

        evt->setDefaultHandled();
    }

    KDOM::EventTargetImpl::defaultEventHandler(evt);
}

// vim:ts=4:noet
