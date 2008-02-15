/*
    Copyright (C) 2006 Apple Computer, Inc.
                  2006 Nikolas Zimmermann <zimmermann@kde.org>
                  2007 Rob Buis <buis@kde.org>

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG)
#include "SVGDocumentExtensions.h"

#include "AtomicString.h"
#include "Chrome.h"
#include "Document.h"
#include "EventListener.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "Page.h"
#include "SVGSVGElement.h"
#include "TimeScheduler.h"
#include "XMLTokenizer.h"
#include "kjs_proxy.h"

namespace WebCore {

SVGDocumentExtensions::SVGDocumentExtensions(Document* doc)
    : m_doc(doc)
{
}

SVGDocumentExtensions::~SVGDocumentExtensions()
{
    deleteAllValues(m_pendingResources);
    deleteAllValues(m_elementInstances);
}

PassRefPtr<EventListener> SVGDocumentExtensions::createSVGEventListener(const String& functionName, const String& code, Node *node)
{
    if (Frame* frame = m_doc->frame())
        if (frame->scriptProxy()->isEnabled())
            return frame->scriptProxy()->createSVGEventHandler(functionName, code, node);
    return 0;
}

void SVGDocumentExtensions::addTimeContainer(SVGSVGElement* element)
{
    m_timeContainers.add(element);
}

void SVGDocumentExtensions::removeTimeContainer(SVGSVGElement* element)
{
    m_timeContainers.remove(element);
}

void SVGDocumentExtensions::startAnimations()
{
    // FIXME: Eventually every "Time Container" will need a way to latch on to some global timer
    // starting animations for a document will do this "latching"
#if ENABLE(SVG_ANIMATION)    
    HashSet<SVGSVGElement*>::iterator end = m_timeContainers.end();
    for (HashSet<SVGSVGElement*>::iterator itr = m_timeContainers.begin(); itr != end; ++itr)
        (*itr)->timeScheduler()->startAnimations();
#endif
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

void SVGDocumentExtensions::reportWarning(const String& message)
{
    if (Frame* frame = m_doc->frame())
        if (Page* page = frame->page())
            page->chrome()->addMessageToConsole(JSMessageSource, ErrorMessageLevel, "Warning: " + message, m_doc->tokenizer() ? m_doc->tokenizer()->lineNumber() : 1, String());
}

void SVGDocumentExtensions::reportError(const String& message)
{
    if (Frame* frame = m_doc->frame())
        if (Page* page = frame->page())
            page->chrome()->addMessageToConsole(JSMessageSource, ErrorMessageLevel, "Error: " + message, m_doc->tokenizer() ? m_doc->tokenizer()->lineNumber() : 1, String());
}

void SVGDocumentExtensions::addPendingResource(const AtomicString& id, SVGStyledElement* obj)
{
    ASSERT(obj);

    if (id.isEmpty())
        return;

    if (m_pendingResources.contains(id))
        m_pendingResources.get(id)->add(obj);
    else {
        HashSet<SVGStyledElement*>* set = new HashSet<SVGStyledElement*>();
        set->add(obj);

        m_pendingResources.add(id, set);
    }
}

bool SVGDocumentExtensions::isPendingResource(const AtomicString& id) const
{
    if (id.isEmpty())
        return false;

    return m_pendingResources.contains(id);
}

std::auto_ptr<HashSet<SVGStyledElement*> > SVGDocumentExtensions::removePendingResource(const AtomicString& id)
{
    ASSERT(m_pendingResources.contains(id));

    std::auto_ptr<HashSet<SVGStyledElement*> > set(m_pendingResources.get(id));
    m_pendingResources.remove(id);
    return set;
}

void SVGDocumentExtensions::mapInstanceToElement(SVGElementInstance* instance, SVGElement* element)
{
    ASSERT(instance);
    ASSERT(element);

    if (m_elementInstances.contains(element))
        m_elementInstances.get(element)->add(instance);
    else {
        HashSet<SVGElementInstance*>* set = new HashSet<SVGElementInstance*>();
        set->add(instance);

        m_elementInstances.add(element, set);
    }
}

void SVGDocumentExtensions::removeInstanceMapping(SVGElementInstance* instance, SVGElement* element)
{
    ASSERT(instance);

    if (!m_elementInstances.contains(element))
        return;

    m_elementInstances.get(element)->remove(instance);
}

HashSet<SVGElementInstance*>* SVGDocumentExtensions::instancesForElement(SVGElement* element) const
{
    ASSERT(element);
    return m_elementInstances.get(element);
}

}

#endif
