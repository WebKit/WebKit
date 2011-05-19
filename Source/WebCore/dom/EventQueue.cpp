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
#include "Document.h"
#include "Event.h"
#include "EventNames.h"
#include "RuntimeApplicationChecks.h"
#include "ScriptExecutionContext.h"

namespace WebCore {
    
static inline bool shouldDispatchScrollEventSynchronously(Document* document)
{
    ASSERT_ARG(document, document);
    return applicationIsSafari() && (document->url().protocolIs("feed") || document->url().protocolIs("feeds"));
}

PassOwnPtr<EventQueue> EventQueue::create(ScriptExecutionContext* context)
{
    return adoptPtr(new EventQueue(context));
}

EventQueue::EventQueue(ScriptExecutionContext* context)
    : m_scriptExecutionContext(context)
{
}

EventQueue::~EventQueue()
{
    cancelQueuedEvents();
}

class EventQueue::EventDispatcherTask : public ScriptExecutionContext::Task {
public:
    static PassOwnPtr<EventDispatcherTask> create(PassRefPtr<Event> event, EventQueue* eventQueue)
    {
        return adoptPtr(new EventDispatcherTask(event, eventQueue));
    }

    void dispatchEvent(ScriptExecutionContext*, PassRefPtr<Event> event)
    {
        EventTarget* eventTarget = event->target();
        if (eventTarget->toDOMWindow())
            eventTarget->toDOMWindow()->dispatchEvent(event, 0);
        else
            eventTarget->dispatchEvent(event);
    }

    virtual void performTask(ScriptExecutionContext* context)
    {
        if (m_isCancelled)
            return;
        m_eventQueue->removeEvent(m_event.get());
        dispatchEvent(context, m_event);
    }

    void cancel()
    {
        m_isCancelled = true;
        m_event.clear();
    }

private:
    EventDispatcherTask(PassRefPtr<Event> event, EventQueue* eventQueue)
        : m_event(event)
        , m_eventQueue(eventQueue)
        , m_isCancelled(false)
    {
    }

    RefPtr<Event> m_event;
    EventQueue* m_eventQueue;
    bool m_isCancelled;
};

void EventQueue::removeEvent(Event* event)
{
    if (Node* node = event->target()->toNode())
        m_nodesWithQueuedScrollEvents.remove(node);
    m_eventTaskMap.remove(event);
}

void EventQueue::enqueueEvent(PassRefPtr<Event> prpEvent)
{
    RefPtr<Event> event = prpEvent;
    OwnPtr<EventDispatcherTask> task = EventDispatcherTask::create(event, this);
    m_eventTaskMap.add(event.release(), task.get());
    m_scriptExecutionContext->postTask(task.release());
}

void EventQueue::enqueueOrDispatchScrollEvent(PassRefPtr<Node> target, ScrollEventTargetType targetType)
{
    // Per the W3C CSSOM View Module, scroll events fired at the document should bubble, others should not.
    bool canBubble = targetType == ScrollEventDocumentTarget;
    RefPtr<Event> scrollEvent = Event::create(eventNames().scrollEvent, canBubble, false /* non cancelleable */);
     
    if (shouldDispatchScrollEventSynchronously(target->document())) {
        target->dispatchEvent(scrollEvent.release());
        return;
    }

    if (!m_nodesWithQueuedScrollEvents.add(target.get()).second)
        return;

    scrollEvent->setTarget(target);
    enqueueEvent(scrollEvent.release());
}

bool EventQueue::cancelEvent(Event* event)
{
    EventDispatcherTask* task = m_eventTaskMap.get(event);
    if (!task)
        return false;
    task->cancel();
    removeEvent(event);
    return true;
}

void EventQueue::cancelQueuedEvents()
{
    for (EventTaskMap::iterator it = m_eventTaskMap.begin(); it != m_eventTaskMap.end(); ++it) {
        EventDispatcherTask* task = it->second;
        task->cancel();
    }
    m_eventTaskMap.clear();
    m_nodesWithQueuedScrollEvents.clear();
}

}
