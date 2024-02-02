/*
 * Copyright (C) 2010 Julien Chaffraix <jchaffraix@webkit.org>  All right reserved.
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
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

#include "config.h"
#include "XMLHttpRequestProgressEventThrottle.h"

#include "ContextDestructionObserverInlines.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "EventTarget.h"
#include "ScriptExecutionContext.h"
#include "XMLHttpRequest.h"
#include "XMLHttpRequestProgressEvent.h"

namespace WebCore {

const Seconds XMLHttpRequestProgressEventThrottle::minimumProgressEventDispatchingInterval { 50_ms }; // 50 ms per specification.

XMLHttpRequestProgressEventThrottle::XMLHttpRequestProgressEventThrottle(XMLHttpRequest& target)
    : m_target(target)
{
}

XMLHttpRequestProgressEventThrottle::~XMLHttpRequestProgressEventThrottle() = default;

void XMLHttpRequestProgressEventThrottle::updateProgress(bool isAsync, bool lengthComputable, unsigned long long loaded, unsigned long long total)
{
    m_lengthComputable = lengthComputable;
    m_loaded = loaded;
    m_total = total;

    if (!isAsync || !m_target.hasEventListeners(eventNames().progressEvent))
        return;

    if (!m_shouldDeferEventsDueToSuspension && !m_dispatchThrottledProgressEventTimer) {
        // The timer is not active so the least frequent event for now is every byte. Just dispatch the event.

        // We should not have any throttled progress event.
        ASSERT(!m_hasPendingThrottledProgressEvent);

        dispatchEventWhenPossible(XMLHttpRequestProgressEvent::create(eventNames().progressEvent, lengthComputable, loaded, total));
        m_dispatchThrottledProgressEventTimer = m_target.scriptExecutionContext()->eventLoop().scheduleRepeatingTask(
            minimumProgressEventDispatchingInterval, minimumProgressEventDispatchingInterval, TaskSource::Networking, [weakThis = WeakPtr { *this }] {
                if (weakThis)
                    weakThis->dispatchThrottledProgressEventTimerFired();
            });
        m_hasPendingThrottledProgressEvent = false;
        return;
    }

    // The timer is already active so minimumProgressEventDispatchingInterval is the least frequent event.
    m_hasPendingThrottledProgressEvent = true;
}

void XMLHttpRequestProgressEventThrottle::dispatchReadyStateChangeEvent(Event& event, ProgressEventAction progressEventAction)
{
    if (progressEventAction == FlushProgressEvent)
        flushProgressEvent();

    dispatchEventWhenPossible(event);
}

void XMLHttpRequestProgressEventThrottle::dispatchEventWhenPossible(Event& event)
{
    if (m_shouldDeferEventsDueToSuspension)
        m_target.queueTaskToDispatchEvent(m_target, TaskSource::Networking, event);
    else
        m_target.dispatchEvent(event);
}

void XMLHttpRequestProgressEventThrottle::dispatchProgressEvent(const AtomString& type)
{
    ASSERT(type == eventNames().loadstartEvent || type == eventNames().progressEvent || type == eventNames().loadEvent || type == eventNames().loadendEvent);

    if (type == eventNames().loadstartEvent) {
        m_lengthComputable = false;
        m_loaded = 0;
        m_total = 0;
    }

    if (m_target.hasEventListeners(type))
        dispatchEventWhenPossible(XMLHttpRequestProgressEvent::create(type, m_lengthComputable, m_loaded, m_total));
}

void XMLHttpRequestProgressEventThrottle::dispatchErrorProgressEvent(const AtomString& type)
{
    ASSERT(type == eventNames().loadendEvent || type == eventNames().abortEvent || type == eventNames().errorEvent || type == eventNames().timeoutEvent);

    if (m_target.hasEventListeners(type))
        dispatchEventWhenPossible(XMLHttpRequestProgressEvent::create(type, false, 0, 0));
}

void XMLHttpRequestProgressEventThrottle::flushProgressEvent()
{
    if (!m_hasPendingThrottledProgressEvent)
        return;

    m_hasPendingThrottledProgressEvent = false;
    // We stop the timer as this is called when no more events are supposed to occur.
    m_dispatchThrottledProgressEventTimer = nullptr;

    dispatchEventWhenPossible(XMLHttpRequestProgressEvent::create(eventNames().progressEvent, m_lengthComputable, m_loaded, m_total));
}

void XMLHttpRequestProgressEventThrottle::dispatchThrottledProgressEventTimerFired()
{
    ASSERT(m_dispatchThrottledProgressEventTimer);
    if (!m_hasPendingThrottledProgressEvent) {
        // No progress event was queued since the previous dispatch, we can safely stop the timer.
        m_dispatchThrottledProgressEventTimer = nullptr;
        return;
    }

    dispatchEventWhenPossible(XMLHttpRequestProgressEvent::create(eventNames().progressEvent, m_lengthComputable, m_loaded, m_total));
    m_hasPendingThrottledProgressEvent = false;
}

void XMLHttpRequestProgressEventThrottle::suspend()
{
    m_shouldDeferEventsDueToSuspension = true;

    if (m_hasPendingThrottledProgressEvent) {
        m_target.queueTaskKeepingObjectAlive(m_target, TaskSource::Networking, [this] {
            flushProgressEvent();
        });
    }
}

void XMLHttpRequestProgressEventThrottle::resume()
{
    m_target.queueTaskKeepingObjectAlive(m_target, TaskSource::Networking, [this] {
        m_shouldDeferEventsDueToSuspension = false;
    });
}

} // namespace WebCore
