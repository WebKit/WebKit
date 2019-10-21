/*
 * Copyright (C) 2010 Julien Chaffraix <jchaffraix@webkit.org>
 * All right reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "SuspendableTimer.h"
#include <wtf/Forward.h>
#include <wtf/Vector.h>

namespace WebCore {

class Event;
class EventTarget;

enum ProgressEventAction {
    DoNotFlushProgressEvent,
    FlushProgressEvent
};

// This implements the XHR2 progress event dispatching: "dispatch a progress event called progress
// about every 50ms or for every byte received, whichever is least frequent".
class XMLHttpRequestProgressEventThrottle {
public:
    explicit XMLHttpRequestProgressEventThrottle(EventTarget&);
    virtual ~XMLHttpRequestProgressEventThrottle();

    void dispatchThrottledProgressEvent(bool lengthComputable, unsigned long long loaded, unsigned long long total);
    void dispatchReadyStateChangeEvent(Event&, ProgressEventAction = DoNotFlushProgressEvent);
    void dispatchProgressEvent(const AtomString&);

    void suspend();
    void resume();

private:
    static const Seconds minimumProgressEventDispatchingInterval;

    void dispatchThrottledProgressEventTimerFired();
    void dispatchDeferredEventsAfterResuming();
    void flushProgressEvent();
    void dispatchEventWhenPossible(Event&);

    // Weak pointer to our XMLHttpRequest object as it is the one holding us.
    EventTarget& m_target;

    unsigned long long m_loaded { 0 };
    unsigned long long m_total { 0 };

    RefPtr<Event> m_deferredProgressEvent;
    Vector<Ref<Event>> m_eventsDeferredDueToSuspension;
    SuspendableTimer m_dispatchThrottledProgressEventTimer;
    SuspendableTimer m_dispatchDeferredEventsAfterResumingTimer;

    bool m_hasPendingThrottledProgressEvent { false };
    bool m_lengthComputable { false };
    bool m_shouldDeferEventsDueToSuspension { false };
};

} // namespace WebCore
