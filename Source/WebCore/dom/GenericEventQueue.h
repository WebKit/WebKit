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

#include "GenericTaskQueue.h"
#include <wtf/Deque.h>
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Event;
class EventTarget;
class Timer;

class GenericEventQueue {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit GenericEventQueue(EventTarget&);
    ~GenericEventQueue();

    void enqueueEvent(RefPtr<Event>&&);
    void close();

    void cancelAllEvents();
    bool hasPendingEvents() const;
    bool hasPendingEventsOfType(const AtomicString&) const;

    void suspend();
    void resume();

private:
    void dispatchOneEvent();

    EventTarget& m_owner;
    GenericTaskQueue<Timer> m_taskQueue;
    Deque<RefPtr<Event>> m_pendingEvents;
    bool m_isClosed;
    bool m_isSuspended { false };
};

} // namespace WebCore
