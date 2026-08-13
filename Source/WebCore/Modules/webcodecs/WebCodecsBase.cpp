/*
 * Copyright (C) 2024-2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebCodecsBase.h"

#if ENABLE(WEB_CODECS)

#include "ContextDestructionObserverInlines.h"
#include "Event.h"
#include "EventNames.h"
#include "WebCodecsControlMessage.h"

#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebCodecsBase);

ScriptExecutionContext* WebCodecsBase::scriptExecutionContext() const
{
    return WebCodecsControlMessageQueue::scriptExecutionContext();
}

void WebCodecsBase::queueCodecControlMessageAndProcess(WebCodecsControlMessage&& message)
{
    incrementCodecQueueSize();
    // message holds a strong ref to ourselves already.
    queueControlMessageAndProcess({ *this, [this, message = WTF::move(message)]() mutable {
        if (isCodecSaturated())
            return WebCodecsControlMessageOutcome::NotProcessed;
        decrementCodecQueueSizeAndScheduleDequeueEvent();
        return message();
    } });
}

void WebCodecsBase::clearControlMessageQueueAndMaybeScheduleDequeueEvent()
{
    clearControlMessageQueue();
    if (codecQueueSize()) {
        resetCodecQueueSize();
        scheduleDequeueEvent();
    }
}

// Equivalent to spec's "Decrement [[encodeQueueSize]] or "Decrement [[decodeQueueSize]]" and run the Schedule Dequeue Event algorithm"
void WebCodecsBase::decrementCodecQueueSizeAndScheduleDequeueEvent()
{
    decrementCodecQueueSize();
    scheduleDequeueEvent();
}

void WebCodecsBase::scheduleDequeueEvent()
{
    if (std::exchange(m_dequeueEventScheduled, true))
        return;

    queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, [](auto& codec) mutable {
        codec.dispatchEvent(Event::create(eventNames().dequeueEvent, Event::CanBubble::No, Event::IsCancelable::No));
        codec.m_dequeueEventScheduled = false;
    });
}

} // namespace WebCore

#endif // ENABLE(WEB_CODECS)
