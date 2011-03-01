/*
 * Copyright (C) 2010 Google Inc. All Rights Reserved.
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
#include "EventQueue.h"

#include "DOMWindow.h"
#include "Event.h"
#include "EventNames.h"
#include "ScriptExecutionContext.h"
#include "SuspendableTimer.h"

namespace WebCore {

class EventQueueTimer : public SuspendableTimer {
    WTF_MAKE_NONCOPYABLE(EventQueueTimer);
public:
    EventQueueTimer(EventQueue* eventQueue, ScriptExecutionContext* context)
        : SuspendableTimer(context)
        , m_eventQueue(eventQueue) { }

private:
    virtual void fired() { m_eventQueue->pendingEventTimerFired(); }
    EventQueue* m_eventQueue;    
};

EventQueue::EventQueue(ScriptExecutionContext* context)
    : m_pendingEventTimer(adoptPtr(new EventQueueTimer(this, context)))
{
}

EventQueue::~EventQueue()
{
}

void EventQueue::enqueueEvent(PassRefPtr<Event> event)
{
    ASSERT(event->target());
    bool wasAdded = m_queuedEvents.add(event).second;
    ASSERT_UNUSED(wasAdded, wasAdded); // It should not have already been in the list.
    
    if (!m_pendingEventTimer->isActive())
        m_pendingEventTimer->startOneShot(0);
}

void EventQueue::enqueueScrollEvent(PassRefPtr<Node> target, ScrollEventTargetType targetType)
{
    if (!m_nodesWithQueuedScrollEvents.add(target.get()).second)
        return;

    // Per the W3C CSSOM View Module, scroll events fired at the document should bubble, others should not.
    bool canBubble = targetType == ScrollEventDocumentTarget;
    RefPtr<Event> scrollEvent = Event::create(eventNames().scrollEvent, canBubble, false /* non cancelleable */);
    scrollEvent->setTarget(target);
    enqueueEvent(scrollEvent.release());
}

bool EventQueue::cancelEvent(Event* event)
{
    bool found = m_queuedEvents.contains(event);
    m_queuedEvents.remove(event);
    if (m_queuedEvents.isEmpty())
        m_pendingEventTimer->stop();
    return found;
}

void EventQueue::pendingEventTimerFired()
{
    ASSERT(!m_pendingEventTimer->isActive());
    ASSERT(!m_queuedEvents.isEmpty());

    m_nodesWithQueuedScrollEvents.clear();

    // Insert a marker for where we should stop.
    ASSERT(!m_queuedEvents.contains(0));
    bool wasAdded = m_queuedEvents.add(0).second;
    ASSERT_UNUSED(wasAdded, wasAdded); // It should not have already been in the list.

    while (!m_queuedEvents.isEmpty()) {
        ListHashSet<RefPtr<Event> >::iterator iter = m_queuedEvents.begin();
        RefPtr<Event> event = *iter;
        m_queuedEvents.remove(iter);
        if (!event)
            break;
        dispatchEvent(event.get());
    }
}

void EventQueue::dispatchEvent(PassRefPtr<Event> event)
{
    EventTarget* eventTarget = event->target();
    if (eventTarget->toDOMWindow())
        eventTarget->toDOMWindow()->dispatchEvent(event, 0);
    else
        eventTarget->dispatchEvent(event);
}

}
