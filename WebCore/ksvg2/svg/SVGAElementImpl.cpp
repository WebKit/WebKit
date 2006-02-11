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
#include <kdom/core/AttrImpl.h>
#include <kdom/events/MouseEventImpl.h>
#include <kdom/events/kdomevents.h>
#include <kdom/Helper.h>
#include "dom2_eventsimpl.h"
#include "csshelper.h"
#include "DocumentImpl.h"

#include <kcanvas/KCanvasCreator.h>
#include <kcanvas/KCanvasContainer.h>
#include <kcanvas/device/KRenderingDevice.h>

#include "EventNames.h"
#include "SVGNames.h"
#include "SVGHelper.h"
#include <ksvg2/KSVGPart.h>
#include "SVGAElementImpl.h"
#include "SVGAnimatedStringImpl.h"

using namespace WebCore;

SVGAElementImpl::SVGAElementImpl(const QualifiedName& tagName, DocumentImpl *doc)
: SVGStyledTransformableElementImpl(tagName, doc), SVGURIReferenceImpl(), SVGTestsImpl(), SVGLangSpaceImpl(), SVGExternalResourcesRequiredImpl()
{
}

SVGAElementImpl::~SVGAElementImpl()
{
}

SVGAnimatedStringImpl *SVGAElementImpl::target() const
{
    return lazy_create<SVGAnimatedStringImpl>(m_target, this);
}

void SVGAElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    DOMString value(attr->value());
    if (attr->name() == SVGNames::targetAttr) {
        target()->setBaseVal(value.impl());
    } else {
        if(SVGURIReferenceImpl::parseMappedAttribute(attr))
        {
            m_isLink = attr->value() != 0;
            return;
        }
        if(SVGTestsImpl::parseMappedAttribute(attr)) return;
        if(SVGLangSpaceImpl::parseMappedAttribute(attr)) return;
        if(SVGExternalResourcesRequiredImpl::parseMappedAttribute(attr)) return;        
        SVGStyledTransformableElementImpl::parseMappedAttribute(attr);
    }
}

RenderObject *SVGAElementImpl::createRenderer(RenderArena *arena, RenderStyle *style)
{
    return QPainter::renderingDevice()->createContainer(arena, style, this);
}

void SVGAElementImpl::defaultEventHandler(EventImpl *evt)
{
    // TODO : should use CLICK instead
    if((evt->type() == EventNames::mouseupEvent && m_isLink))
    {
        MouseEventImpl *e = static_cast<MouseEventImpl*>(evt);

        QString url;
        QString utarget;
        if(e && e->button() == 2)
        {
            EventTargetImpl::defaultEventHandler(evt);
            return;
        }
        url = parseURL(href()->baseVal()).qstring();
        utarget = DOMString(getAttribute(SVGNames::targetAttr)).qstring();

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
            if(ownerDocument() && ownerDocument()->view() && ownerDocument()->frame())
            {
                //getDocument()->view()->resetCursor();
                getDocument()->frame()->urlSelected(url, button, state, utarget);
            }
        }

        evt->setDefaultHandled();
    }

    EventTargetImpl::defaultEventHandler(evt);
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

