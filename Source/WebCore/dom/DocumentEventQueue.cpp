/*
 * Copyright (C) 2010 Google Inc. All Rights Reserved.
 * Copyright (C) 2013 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 *
 */

#include "config.h"
#include "DocumentEventQueue.h"

#include "DOMWindow.h"
#include "Document.h"
#include "Event.h"
#include "SuspendableTimer.h"
#include <wtf/Ref.h>

namespace WebCore {
    
class DocumentEventQueue::Timer final : public SuspendableTimer {
public:
    Timer(DocumentEventQueue& eventQueue)
        : SuspendableTimer(&eventQueue.m_document)
        , m_eventQueue(eventQueue)
    {
    }

private:
    virtual void fired() override
    {
        ASSERT(!isSuspended());
        m_eventQueue.pendingEventTimerFired();
    }

    DocumentEventQueue& m_eventQueue;
};

DocumentEventQueue::DocumentEventQueue(Document& document)
    : m_document(document)
    , m_pendingEventTimer(std::make_unique<Timer>(*this))
    , m_isClosed(false)
{
    m_pendingEventTimer->suspendIfNeeded();
}

DocumentEventQueue::~DocumentEventQueue()
{
}

bool DocumentEventQueue::enqueueEvent(PassRefPtr<Event> event)
{
    ASSERT(event->target());
    ASSERT(!m_queuedEvents.contains(event.get()));

    if (m_isClosed)
        return false;

    m_queuedEvents.add(event);
    if (!m_pendingEventTimer->isActive())
        m_pendingEventTimer->startOneShot(0);
    return true;
}

void DocumentEventQueue::enqueueOrDispatchScrollEvent(Node& target)
{
    ASSERT(&target.document() == &m_document);

    if (m_isClosed)
        return;

    if (!m_document.hasListenerType(Document::SCROLL_LISTENER))
        return;

    if (!m_nodesWithQueuedScrollEvents.add(&target).isNewEntry)
        return;

    // Per the W3C CSSOM View Module, scroll events fired at the document should bubble, others should not.
    bool bubbles = target.isDocumentNode();
    bool cancelable = false;

    RefPtr<Event> scrollEvent = Event::create(eventNames().scrollEvent, bubbles, cancelable);
    scrollEvent->setTarget(&target);
    enqueueEvent(scrollEvent.release());
}

bool DocumentEventQueue::cancelEvent(Event& event)
{
    bool found = m_queuedEvents.remove(&event);
    if (m_queuedEvents.isEmpty())
        m_pendingEventTimer->cancel();
    return found;
}

void DocumentEventQueue::close()
{
    m_isClosed = true;
    m_pendingEventTimer->cancel();
    m_queuedEvents.clear();
}

void DocumentEventQueue::pendingEventTimerFired()
{
    ASSERT(!m_pendingEventTimer->isActive());
    ASSERT(!m_queuedEvents.isEmpty());

    m_nodesWithQueuedScrollEvents.clear();

    // Insert a marker for where we should stop.
    ASSERT(!m_queuedEvents.contains(nullptr));
    m_queuedEvents.add(nullptr);

    Ref<Document> protect(m_document);

    while (!m_queuedEvents.isEmpty()) {
        RefPtr<Event> event = m_queuedEvents.takeFirst();
        if (!event)
            break;
        dispatchEvent(*event);
    }
}

void DocumentEventQueue::dispatchEvent(Event& event)
{
    // FIXME: Where did this special case for the DOM window come from?
    // Why do we have this special case here instead of a virtual function on EventTarget?
    EventTarget& eventTarget = *event.target();
    if (DOMWindow* window = eventTarget.toDOMWindow())
        window->dispatchEvent(&event, 0);
    else
        eventTarget.dispatchEvent(&event);
}

}
