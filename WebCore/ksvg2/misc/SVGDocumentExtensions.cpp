/*
    Copyright (C) 2006 Apple Computer, Inc.

    This file is part of the WebKit project

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
#include "SVGDocumentExtensions.h"

#include "DocumentImpl.h"
#include "Frame.h"
#include "KSVGTimeScheduler.h"
#include "kjs_proxy.h"

namespace WebCore {

SVGDocumentExtensions::SVGDocumentExtensions(DocumentImpl *doc)
    : m_doc(doc)
    , m_timeScheduler(new TimeScheduler(doc))
{
}

SVGDocumentExtensions::~SVGDocumentExtensions()
{
    delete m_timeScheduler;
}

EventListener *SVGDocumentExtensions::createSVGEventListener(const DOMString& code, NodeImpl *node)
{
    if (Frame *frame = m_doc->frame()) {
        if (KJSProxyImpl *proxy = frame->jScript())
            return proxy->createSVGEventHandler(code, node);
    }
    return 0;
}

void SVGDocumentExtensions::pauseAnimations()
{
    if (!m_timeScheduler->animationsPaused())
        m_timeScheduler->toggleAnimations();
}

void SVGDocumentExtensions::unpauseAnimations()
{
    if (m_timeScheduler->animationsPaused())
        m_timeScheduler->toggleAnimations();
}

bool SVGDocumentExtensions::animationsPaused() const
{
    return m_timeScheduler->animationsPaused();
}

float SVGDocumentExtensions::getCurrentTime() const
{
    return m_timeScheduler->elapsed();
}

void SVGDocumentExtensions::setCurrentTime(float /* seconds */)
{
    // TODO
}

}

#endif
