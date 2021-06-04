/*
 * Copyright (C) 2012 Victor Carbune (victor@rosedu.org)
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "EventLoopEventQueue.h"

#include "Document.h"
#include "Event.h"
#include "EventLoop.h"
#include "EventTarget.h"
#include "Node.h"
#include "ScriptExecutionContext.h"
#include "Timer.h"
#include <wtf/Algorithms.h>
#include <wtf/MainThread.h>
#include <wtf/SetForScope.h>

namespace WebCore {

EventLoopEventQueue::EventLoopEventQueue(EventTarget& owner)
    : ActiveDOMObject(owner.scriptExecutionContext())
    , m_owner(owner)
{
}

void EventLoopEventQueue::enqueueEvent(RefPtr<Event>&& event)
{
    if (m_isClosed || !scriptExecutionContext())
        return;

    if (event->target() == &m_owner)
        event->setTarget(nullptr);

    m_pendingEvents.append(WTFMove(event));

    scriptExecutionContext()->eventLoop().queueTask(TaskSource::MediaElement, [weakThis = makeWeakPtr(*this)] {
        if (weakThis)
            weakThis->dispatchOneEvent();
    });
}

void EventLoopEventQueue::dispatchOneEvent()
{
    ASSERT(!m_pendingEvents.isEmpty());

    Ref<EventTarget> protectedOwner(m_owner);
    SetForScope<bool> eventFiringScope(m_isFiringEvent, true);

    RefPtr<Event> event = m_pendingEvents.takeFirst();
    Ref<EventTarget> target = event->target() ? *event->target() : m_owner;
    ASSERT_WITH_MESSAGE(!target->scriptExecutionContext()->activeDOMObjectsAreStopped(),
        "An attempt to dispatch an event on a stopped target by EventTargetInterface=%d (nodeName=%s target=%p owner=%p)",
        m_owner.eventTargetInterface(), m_owner.isNode() ? static_cast<Node&>(m_owner).nodeName().ascii().data() : "", target.ptr(), &m_owner);
    target->dispatchEvent(*event);
}

void EventLoopEventQueue::close()
{
    m_isClosed = true;
    cancelAllEvents();
}

void EventLoopEventQueue::cancelAllEvents()
{
    weakPtrFactory().revokeAll();
    m_pendingEvents.clear();
}

bool EventLoopEventQueue::hasPendingActivity() const
{
    return !m_pendingEvents.isEmpty() || m_isFiringEvent;
}

bool EventLoopEventQueue::hasPendingEventsOfType(const AtomString& type) const
{
    return WTF::anyOf(m_pendingEvents, [&](auto& event) { return event->type() == type; });
}

void EventLoopEventQueue::stop()
{
    close();
}

const char* EventLoopEventQueue::activeDOMObjectName() const
{
    return "EventLoopEventQueue";
}

UniqueRef<EventLoopEventQueue> EventLoopEventQueue::create(EventTarget& eventTarget)
{
    auto eventQueue = makeUniqueRef<EventLoopEventQueue>(eventTarget);
    eventQueue->suspendIfNeeded();
    return eventQueue;
}

}
