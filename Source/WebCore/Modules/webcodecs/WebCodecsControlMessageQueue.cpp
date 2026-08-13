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
#include "WebCodecsControlMessageQueue.h"

#if ENABLE(WEB_CODECS)

#include "WebCodecsControlMessage.h"

#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebCodecsControlMessageQueue);

WebCodecsControlMessageQueue::WebCodecsControlMessageQueue(ScriptExecutionContext& context)
    : ActiveDOMObject(&context)
{
}

WebCodecsControlMessageQueue::~WebCodecsControlMessageQueue() = default;

void WebCodecsControlMessageQueue::queueControlMessageAndProcess(WebCodecsControlMessage&& message)
{
    m_controlMessageQueue.append(WTF::move(message));
    if (!m_isMessageQueueBlocked)
        processControlMessageQueue();
}

void WebCodecsControlMessageQueue::queueCodecControlMessageAndProcess(WebCodecsControlMessage&& message)
{
    incrementCodecQueueSize();
    // message holds a strong ref to ourselves already.
    queueControlMessageAndProcess({ *this, [this, message = WTF::move(message)]() mutable {
        if (isCodecSaturated())
            return WebCodecsControlMessageOutcome::NotProcessed;
        decrementCodecQueueSize();
        return message();
    } });
}

void WebCodecsControlMessageQueue::processControlMessageQueue()
{
    while (!m_isMessageQueueBlocked && !m_controlMessageQueue.isEmpty()) {
        auto& frontMessage = m_controlMessageQueue.first();
        auto outcome = frontMessage();
        if (outcome == WebCodecsControlMessageOutcome::NotProcessed)
            break;
        m_controlMessageQueue.removeFirst();
    }
}

void WebCodecsControlMessageQueue::incrementCodecQueueSize()
{
    m_codecControlMessagesPending++;
}

// Equivalent to spec's "Decrement [[encodeQueueSize]] or "Decrement [[decodeQueueSize]]".
void WebCodecsControlMessageQueue::decrementCodecQueueSize()
{
    m_codecControlMessagesPending--;
}

void WebCodecsControlMessageQueue::resetCodecQueueSize()
{
    m_codecControlMessagesPending = 0;
}

void WebCodecsControlMessageQueue::decrementCodecOperationCountAndMaybeProcessControlMessageQueue()
{
    ASSERT(m_codecOperationsPending > 0);
    m_codecOperationsPending--;
    if (!isCodecSaturated())
        processControlMessageQueue();
}

void WebCodecsControlMessageQueue::clearControlMessageQueue()
{
    m_controlMessageQueue.clear();
}

void WebCodecsControlMessageQueue::blockControlMessageQueue()
{
    m_isMessageQueueBlocked = true;
}

void WebCodecsControlMessageQueue::unblockControlMessageQueue()
{
    m_isMessageQueueBlocked = false;
    processControlMessageQueue();
}

bool WebCodecsControlMessageQueue::virtualHasPendingActivity() const
{
    return m_codecControlMessagesPending || m_isMessageQueueBlocked;
}

} // namespace WebCore

#endif // ENABLE(WEB_CODECS)
