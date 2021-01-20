/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "IdleCallbackController.h"

#include "Document.h"
#include "IdleDeadline.h"
#include "WindowEventLoop.h"

namespace WebCore {

IdleCallbackController::IdleCallbackController(Document& document)
    : m_document(makeWeakPtr(document))
{

}
int IdleCallbackController::queueIdleCallback(Ref<IdleRequestCallback>&& callback, Seconds)
{
    bool startIdlePeriod = m_idleRequestCallbacks.isEmpty() && m_runnableIdleCallbacks.isEmpty();

    ++m_idleCallbackIdentifier;
    auto handle = m_idleCallbackIdentifier;

    m_idleRequestCallbacks.append({ handle, WTFMove(callback) });

    if (startIdlePeriod)
        queueTaskToStartIdlePeriod();

    // FIXME: Queue a task if timeout is positive.

    return handle;
}

void IdleCallbackController::removeIdleCallback(int signedIdentifier)
{
    if (signedIdentifier <= 0)
        return;
    unsigned identifier = signedIdentifier;

    m_idleRequestCallbacks.removeAllMatching([identifier](auto& request) {
        return request.identifier == identifier;
    });

    m_runnableIdleCallbacks.removeAllMatching([identifier](auto& request) {
        return request.identifier == identifier;
    });
}

void IdleCallbackController::queueTaskToStartIdlePeriod()
{
    m_document->eventLoop().queueTask(TaskSource::IdleTask, [protectedDocument = makeRef(*m_document), this] {
        RELEASE_ASSERT(protectedDocument->idleCallbackController() == this);
        startIdlePeriod();
    });
}

// https://w3c.github.io/requestidlecallback/#start-an-idle-period-algorithm
static const auto deadlineCapToEnsureResponsiveness = 50_ms;
void IdleCallbackController::startIdlePeriod()
{
    auto now = MonotonicTime::now();
    if (m_lastDeadline > now)
        return;

    // FIXME: Take other tasks in the WindowEventLoop into account.
    auto deadline = now + deadlineCapToEnsureResponsiveness;

    for (auto& request : m_idleRequestCallbacks)
        m_runnableIdleCallbacks.append({ request.identifier, WTFMove(request.callback) });
    m_idleRequestCallbacks.clear();

    ASSERT(!m_runnableIdleCallbacks.isEmpty());
    queueTaskToInvokeIdleCallbacks(deadline);

    m_lastDeadline = deadline;
}

void IdleCallbackController::queueTaskToInvokeIdleCallbacks(MonotonicTime deadline)
{
    m_document->eventLoop().queueTask(TaskSource::IdleTask, [protectedDocument = makeRef(*m_document), deadline, this] {
        RELEASE_ASSERT(protectedDocument->idleCallbackController() == this);
        invokeIdleCallbacks(deadline);
    });
}

// https://w3c.github.io/requestidlecallback/#invoke-idle-callbacks-algorithm
void IdleCallbackController::invokeIdleCallbacks(MonotonicTime deadline)
{
    if (!m_document)
        return;

    auto now = MonotonicTime::now();
    if (now < deadline) {
        // FIXME: Don't do this if there is a higher priority task in the event loop.
        // https://github.com/w3c/requestidlecallback/issues/83
        if (m_runnableIdleCallbacks.isEmpty())
            return;

        auto request = m_runnableIdleCallbacks.takeFirst();
        auto idleDeadline = IdleDeadline::create(deadline);
        request.callback->handleEvent(idleDeadline.get());
        if (!m_runnableIdleCallbacks.isEmpty())
            queueTaskToInvokeIdleCallbacks(deadline);
        return;
    }

    if (!m_idleRequestCallbacks.isEmpty() || !m_runnableIdleCallbacks.isEmpty())
        queueTaskToStartIdlePeriod();
}

} // namespace WebCore
