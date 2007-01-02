/*
    Copyright (C) 2006 Apple Computer, Inc.
                  2006 Nikolas Zimmermann <zimmermann@kde.org>

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

#ifdef SVG_SUPPORT
#include "SVGDocumentExtensions.h"

#include "Document.h"
#include "EventListener.h"
#include "Frame.h"
#include "TimeScheduler.h"
#include "AtomicString.h"
#include "kjs_proxy.h"
#include "SVGSVGElement.h"

namespace WebCore {

SVGDocumentExtensions::SVGDocumentExtensions(Document* doc)
    : m_doc(doc)
{
}

SVGDocumentExtensions::~SVGDocumentExtensions()
{
}

PassRefPtr<EventListener> SVGDocumentExtensions::createSVGEventListener(const String& functionName, const String& code, Node *node)
{
    if (Frame* frame = m_doc->frame())
        if (KJSProxy* proxy = frame->scriptProxy())
            return proxy->createSVGEventHandler(functionName, code, node);
    return 0;
}

void SVGDocumentExtensions::addTimeContainer(SVGSVGElement* element)
{
    m_timeContainers.add(element);
}

void SVGDocumentExtensions::removeTimeContainer(SVGSVGElement* element)
{
    ASSERT(m_timeContainers.contains(element));
    m_timeContainers.remove(element);
}

void SVGDocumentExtensions::startAnimations()
{
    // FIXME: Eventually every "Time Container" will need a way to latch on to some global timer
    // starting animations for a document will do this "latching"
    
    HashSet<SVGSVGElement*>::iterator end = m_timeContainers.end();
    for (HashSet<SVGSVGElement*>::iterator itr = m_timeContainers.begin(); itr != end; ++itr)
        (*itr)->timeScheduler()->startAnimations();
}
    
void SVGDocumentExtensions::pauseAnimations()
{
    HashSet<SVGSVGElement*>::iterator end = m_timeContainers.end();
    for (HashSet<SVGSVGElement*>::iterator itr = m_timeContainers.begin(); itr != end; ++itr)
        (*itr)->pauseAnimations();
}

void SVGDocumentExtensions::unpauseAnimations()
{
    HashSet<SVGSVGElement*>::iterator end = m_timeContainers.end();
    for (HashSet<SVGSVGElement*>::iterator itr = m_timeContainers.begin(); itr != end; ++itr)
        (*itr)->unpauseAnimations();
}

void SVGDocumentExtensions::addPendingResource(const AtomicString& id, SVGStyledElement* obj)
{
    if (m_pendingResources.contains(id))
        m_pendingResources.get(id).add(obj);
    else {
        HashSet<SVGStyledElement*> set;
        set.add(obj);

        m_pendingResources.add(id, set);
    }
}

bool SVGDocumentExtensions::isPendingResource(const AtomicString& id) const
{
    return m_pendingResources.contains(id);
}

HashSet<SVGStyledElement*> SVGDocumentExtensions::removePendingResource(const AtomicString& id)
{
    ASSERT(m_pendingResources.contains(id));

    HashSet<SVGStyledElement*> set = m_pendingResources.get(id);
    m_pendingResources.remove(id);
    return set;
}

}

#endif
