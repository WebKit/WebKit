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
#include "Console.h"
#include "DOMWindow.h"
#include "Document.h"
#include "EventListener.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "Page.h"
#include "SVGSMILElement.h"
#include "SVGSVGElement.h"
#include "SMILTimeContainer.h"
#include "XMLTokenizer.h"
#include "ScriptController.h"

namespace WebCore {

SVGDocumentExtensions::SVGDocumentExtensions(Document* doc)
    : m_doc(doc)
{
}

SVGDocumentExtensions::~SVGDocumentExtensions()
{
    deleteAllValues(m_pendingResources);
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
        (*itr)->timeContainer()->begin();
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

bool SVGDocumentExtensions::sampleAnimationAtTime(const String& elementId, SVGSMILElement* element, double time)
{
    ASSERT(element);
    SMILTimeContainer* container = element->timeContainer();
    if (!container || container->isPaused())
        return false;

    container->sampleAnimationAtTime(elementId, time);
    return true;
}

void SVGDocumentExtensions::reportWarning(const String& message)
{
    if (Frame* frame = m_doc->frame())
        frame->domWindow()->console()->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "Warning: " + message, m_doc->tokenizer() ? m_doc->tokenizer()->lineNumber() : 1, String());
}

void SVGDocumentExtensions::reportError(const String& message)
{
    if (Frame* frame = m_doc->frame())
        frame->domWindow()->console()->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "Error: " + message, m_doc->tokenizer() ? m_doc->tokenizer()->lineNumber() : 1, String());
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

PassOwnPtr<HashSet<SVGStyledElement*> > SVGDocumentExtensions::removePendingResource(const AtomicString& id)
{
    ASSERT(m_pendingResources.contains(id));

    OwnPtr<HashSet<SVGStyledElement*> > set(m_pendingResources.get(id));
    m_pendingResources.remove(id);
    return set.release();
}

}

#endif
