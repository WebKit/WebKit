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
#include "GenericEventQueue.h"

#include "Document.h"
#include "Event.h"
#include "EventTarget.h"
#include "Node.h"
#include "ScriptExecutionContext.h"
#include "Timer.h"
#include <wtf/MainThread.h>

namespace WebCore {

GenericEventQueue::GenericEventQueue(EventTarget& owner)
    : m_owner(owner)
    , m_isClosed(false)
{
}

GenericEventQueue::~GenericEventQueue() = default;

void GenericEventQueue::enqueueEvent(RefPtr<Event>&& event)
{
    if (m_isClosed)
        return;

    if (event->target() == &m_owner)
        event->setTarget(nullptr);

    m_pendingEvents.append(WTFMove(event));

    if (m_isSuspended)
        return;

    m_taskQueue.enqueueTask(std::bind(&GenericEventQueue::dispatchOneEvent, this));
}

void GenericEventQueue::dispatchOneEvent()
{
    ASSERT(!m_pendingEvents.isEmpty());

    Ref<EventTarget> protect(m_owner);
    RefPtr<Event> event = m_pendingEvents.takeFirst();
    EventTarget& target = event->target() ? *event->target() : m_owner;
    ASSERT_WITH_MESSAGE(!target.scriptExecutionContext()->activeDOMObjectsAreStopped(),
        "An attempt to dispatch an event on a stopped target by EventTargetInterface=%d (nodeName=%s target=%p owner=%p)",
        m_owner.eventTargetInterface(), m_owner.isNode() ? static_cast<Node&>(m_owner).nodeName().ascii().data() : "", &target, &m_owner);
    target.dispatchEvent(*event);
}

void GenericEventQueue::close()
{
    m_isClosed = true;

    m_taskQueue.close();
    m_pendingEvents.clear();
}

void GenericEventQueue::cancelAllEvents()
{
    m_taskQueue.cancelAllTasks();
    m_pendingEvents.clear();
}

bool GenericEventQueue::hasPendingEvents() const
{
    return !m_pendingEvents.isEmpty();
}

bool GenericEventQueue::hasPendingEventsOfType(const AtomString& type) const
{
    for (auto& event : m_pendingEvents) {
        if (event->type() == type)
            return true;
    }

    return false;
}

void GenericEventQueue::suspend()
{
    ASSERT(!m_isSuspended);
    m_isSuspended = true;
    m_taskQueue.cancelAllTasks();
}

void GenericEventQueue::resume()
{
    if (!m_isSuspended)
        return;

    m_isSuspended = false;

    for (unsigned i = 0; i < m_pendingEvents.size(); ++i)
        m_taskQueue.enqueueTask(std::bind(&GenericEventQueue::dispatchOneEvent, this));
}

}
