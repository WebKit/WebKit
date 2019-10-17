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

template<typename T> struct TaskQueueConstructor {
    static UniqueRef<GenericTaskQueue<T>> construct(ScriptExecutionContext* context) { return makeUniqueRef<GenericTaskQueue<T>>(context); }
};

template<> struct TaskQueueConstructor<Timer> {
    static UniqueRef<GenericTaskQueue<Timer>> construct(ScriptExecutionContext*) { return makeUniqueRef<GenericTaskQueue<Timer>>(); }
};

template<typename T>
GenericEventQueueBase<T>::GenericEventQueueBase(EventTarget& owner)
    : ActiveDOMObject(owner.scriptExecutionContext())
    , m_owner(owner)
    , m_taskQueue(TaskQueueConstructor<T>::construct(owner.scriptExecutionContext()))
{
}

template<typename T>
GenericEventQueueBase<T>::~GenericEventQueueBase() = default;

template<typename T>
void GenericEventQueueBase<T>::enqueueEvent(RefPtr<Event>&& event)
{
    if (m_isClosed)
        return;

    if (event->target() == &m_owner)
        event->setTarget(nullptr);

    m_pendingEvents.append(WTFMove(event));

    if (isSuspendedOrPausedByClient())
        return;

    m_taskQueue->enqueueTask(std::bind(&GenericEventQueueBase::dispatchOneEvent, this));
}

template<typename T>
void GenericEventQueueBase<T>::dispatchOneEvent()
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

template<typename T>
void GenericEventQueueBase<T>::close()
{
    m_isClosed = true;

    m_taskQueue->close();
    m_pendingEvents.clear();
}

template<typename T>
void GenericEventQueueBase<T>::cancelAllEvents()
{
    m_taskQueue->cancelAllTasks();
    m_pendingEvents.clear();
}

template<typename T>
bool GenericEventQueueBase<T>::hasPendingEvents() const
{
    return !m_pendingEvents.isEmpty();
}

template<typename T>
bool GenericEventQueueBase<T>::hasPendingEventsOfType(const AtomString& type) const
{
    for (auto& event : m_pendingEvents) {
        if (event->type() == type)
            return true;
    }

    return false;
}

template<typename T>
void GenericEventQueueBase<T>::setPaused(bool shouldPause)
{
    if (m_isPausedByClient == shouldPause)
        return;

    m_isPausedByClient = shouldPause;
    if (shouldPause)
        m_taskQueue->cancelAllTasks();
    else
        rescheduleAllEventsIfNeeded();
}

template<typename T>
void GenericEventQueueBase<T>::suspend(ReasonForSuspension)
{
    if (m_isSuspended)
        return;

    m_isSuspended = true;
    m_taskQueue->cancelAllTasks();
}

template<typename T>
void GenericEventQueueBase<T>::resume()
{
    if (!m_isSuspended)
        return;

    m_isSuspended = false;
    rescheduleAllEventsIfNeeded();
}

template<typename T>
void GenericEventQueueBase<T>::rescheduleAllEventsIfNeeded()
{
    if (isSuspendedOrPausedByClient())
        return;

    for (unsigned i = 0; i < m_pendingEvents.size(); ++i)
        m_taskQueue->enqueueTask(std::bind(&GenericEventQueueBase::dispatchOneEvent, this));
}

template<typename T>
void GenericEventQueueBase<T>::stop()
{
    close();
}

template<typename T>
const char* GenericEventQueueBase<T>::activeDOMObjectName() const
{
    return "GenericEventQueueBase";
}

template class GenericEventQueueBase<Timer>;
template class GenericEventQueueBase<ScriptExecutionContext>;

UniqueRef<GenericEventQueue> GenericEventQueue::create(EventTarget& eventTarget)
{
    auto eventQueue = makeUniqueRef<GenericEventQueue>(eventTarget);
    eventQueue->suspendIfNeeded();
    return eventQueue;
}

UniqueRef<MainThreadGenericEventQueue> MainThreadGenericEventQueue::create(EventTarget& eventTarget)
{
    auto eventQueue = makeUniqueRef<MainThreadGenericEventQueue>(eventTarget);
    eventQueue->suspendIfNeeded();
    return eventQueue;
}

}
