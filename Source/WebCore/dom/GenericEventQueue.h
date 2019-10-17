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

#pragma once

#include "ActiveDOMObject.h"
#include "GenericTaskQueue.h"
#include <wtf/Deque.h>
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

class Event;
class EventTarget;
class Timer;
class ScriptExecutionContext;

template<typename T>
class GenericEventQueueBase : public ActiveDOMObject {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit GenericEventQueueBase(EventTarget&);
    ~GenericEventQueueBase();

    void enqueueEvent(RefPtr<Event>&&);
    void close();

    void cancelAllEvents();
    bool hasPendingEvents() const;
    bool hasPendingEventsOfType(const AtomString&) const;

    void setPaused(bool);

    bool isSuspended() const { return m_isSuspended; }

private:
    void dispatchOneEvent();

    const char* activeDOMObjectName() const final;
    void stop() final;
    void suspend(ReasonForSuspension) final;
    void resume() final;

    void rescheduleAllEventsIfNeeded();
    bool isSuspendedOrPausedByClient() const { return m_isSuspended || m_isPausedByClient; }

    EventTarget& m_owner;
    UniqueRef<GenericTaskQueue<T>> m_taskQueue;
    Deque<RefPtr<Event>> m_pendingEvents;
    bool m_isClosed { false };
    bool m_isPausedByClient { false };
    bool m_isSuspended { false };
};

// All instances of MainThreadGenericEventQueue use a shared Timer for dispatching events.
class MainThreadGenericEventQueue : public GenericEventQueueBase<Timer> {
public:
    static UniqueRef<MainThreadGenericEventQueue> create(EventTarget&);

private:
    friend UniqueRef<MainThreadGenericEventQueue> WTF::makeUniqueRefWithoutFastMallocCheck<MainThreadGenericEventQueue, WebCore::EventTarget&>(WebCore::EventTarget&);
    explicit MainThreadGenericEventQueue(EventTarget& eventTarget)
        : GenericEventQueueBase<Timer>(eventTarget)
    {
    }
};

class GenericEventQueue : public GenericEventQueueBase<ScriptExecutionContext> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static UniqueRef<GenericEventQueue> create(EventTarget&);

private:
    friend UniqueRef<GenericEventQueue> WTF::makeUniqueRefWithoutFastMallocCheck<GenericEventQueue, WebCore::EventTarget&>(WebCore::EventTarget&);
    explicit GenericEventQueue(EventTarget& eventTarget)
        : GenericEventQueueBase<ScriptExecutionContext>(eventTarget)
    {
    }
};

} // namespace WebCore
