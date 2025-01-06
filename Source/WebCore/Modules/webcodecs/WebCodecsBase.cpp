/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "Event.h"
#include "EventNames.h"
#include "WebCodecsControlMessage.h"

#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(WebCodecsBase);

WebCodecsBase::WebCodecsBase(ScriptExecutionContext& context)
    : ActiveDOMObject(&context)
{
}

WebCodecsBase::~WebCodecsBase() = default;

void WebCodecsBase::queueControlMessageAndProcess(WebCodecsControlMessage&& message)
{
    if (m_isMessageQueueBlocked) {
        m_controlMessageQueue.append(WTFMove(message));
        return;
    }
    m_controlMessageQueue.append(WTFMove(message));
    processControlMessageQueue();
}

void WebCodecsBase::queueCodecControlMessageAndProcess(WebCodecsControlMessage&& message)
{
    incrementCodecQueueSize();
    // message holds a strong ref to ourselves already.
    queueControlMessageAndProcess({ *this, [this, message = WTFMove(message)]() mutable {
        if (isCodecSaturated())
            return WebCodecsControlMessageOutcome::NotProcessed;
        decrementCodecQueueSizeAndScheduleDequeueEvent();
        return message();
    } });
}

void WebCodecsBase::scheduleDequeueEvent()
{
    if (m_dequeueEventScheduled)
        return;

    m_dequeueEventScheduled = true;
    queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, [this]() mutable {
        dispatchEvent(Event::create(eventNames().dequeueEvent, Event::CanBubble::No, Event::IsCancelable::No));
        m_dequeueEventScheduled = false;
    });
}

void WebCodecsBase::processControlMessageQueue()
{
    while (!m_isMessageQueueBlocked && !m_controlMessageQueue.isEmpty()) {
        auto& frontMessage = m_controlMessageQueue.first();
        auto outcome = frontMessage();
        if (outcome == WebCodecsControlMessageOutcome::NotProcessed)
            break;
        m_controlMessageQueue.removeFirst();
    }
}

void WebCodecsBase::incrementCodecQueueSize()
{
    m_codecControlMessagesPending++;
}

// Equivalent to spec's "Decrement [[encodeQueueSize]] or "Decrement [[decodeQueueSize]]" and run the Schedule Dequeue Event algorithm"
void WebCodecsBase::decrementCodecQueueSizeAndScheduleDequeueEvent()
{
    m_codecControlMessagesPending--;
    scheduleDequeueEvent();
}

void WebCodecsBase::decrementCodecOperationCountAndMaybeProcessControlMessageQueue()
{
    ASSERT(m_codecOperationsPending > 0);
    m_codecOperationsPending--;
    if (!isCodecSaturated())
        processControlMessageQueue();
}

void WebCodecsBase::clearControlMessageQueue()
{
    m_controlMessageQueue.clear();
}

void WebCodecsBase::clearControlMessageQueueAndMaybeScheduleDequeueEvent()
{
    clearControlMessageQueue();
    if (m_codecControlMessagesPending) {
        m_codecControlMessagesPending = 0;
        scheduleDequeueEvent();
    }
}

void WebCodecsBase::blockControlMessageQueue()
{
    m_isMessageQueueBlocked = true;
}

void WebCodecsBase::unblockControlMessageQueue()
{
    m_isMessageQueueBlocked = false;
    processControlMessageQueue();
}

bool WebCodecsBase::virtualHasPendingActivity() const
{
    return m_state == WebCodecsCodecState::Configured && (m_codecControlMessagesPending || m_isMessageQueueBlocked);
}

} // namespace WebCore

#endif // ENABLE(WEB_CODECS)
