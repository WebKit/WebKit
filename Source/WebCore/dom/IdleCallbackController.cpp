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
#include "FrameDestructionObserverInlines.h"
#include "IdleDeadline.h"
#include "Timer.h"
#include "WindowEventLoop.h"

namespace WebCore {

IdleCallbackController::IdleCallbackController(Document& document)
    : m_document(document)
{

}

int IdleCallbackController::queueIdleCallback(Ref<IdleRequestCallback>&& callback, Seconds timeout)
{
    ++m_idleCallbackIdentifier;
    auto handle = m_idleCallbackIdentifier;

    m_idleRequestCallbacks.append({ handle, WTFMove(callback) });

    if (timeout > 0_s) {
        Timer::schedule(timeout, [weakThis = WeakPtr { *this }, handle]() {
            if (!weakThis)
                return;
            RefPtr document = weakThis->m_document.get();
            if (!document)
                return;
            document->eventLoop().queueTask(TaskSource::IdleTask, [weakThis, handle]() {
                if (!weakThis)
                    return;
                weakThis->invokeIdleCallbackTimeout(handle);
            });
        });
    }

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
    Ref document = *m_document;
    document->eventLoop().queueTask(TaskSource::IdleTask, [this, document] {
        RELEASE_ASSERT(document->idleCallbackController() == this);
        startIdlePeriod();
    });
}

// https://w3c.github.io/requestidlecallback/#start-an-idle-period-algorithm
void IdleCallbackController::startIdlePeriod()
{
    for (auto& request : m_idleRequestCallbacks)
        m_runnableIdleCallbacks.append({ request.identifier, WTFMove(request.callback) });
    m_idleRequestCallbacks.clear();

    if (m_runnableIdleCallbacks.isEmpty())
        return;

    queueTaskToInvokeIdleCallbacks();
}

void IdleCallbackController::queueTaskToInvokeIdleCallbacks()
{
    Ref document = *m_document;
    document->eventLoop().queueTask(TaskSource::IdleTask, [this, document] {
        RELEASE_ASSERT(document->idleCallbackController() == this);
        invokeIdleCallbacks();
    });
}

// https://w3c.github.io/requestidlecallback/#invoke-idle-callbacks-algorithm
void IdleCallbackController::invokeIdleCallbacks()
{
    RefPtr document = m_document.get();
    if (!document || !document->frame())
        return;

    Ref windowEventLoop = document->windowEventLoop();
    // FIXME: Implement "if the user-agent believes it should end the idle period early due to newly scheduled high-priority work, return from the algorithm."

    auto now = MonotonicTime::now();
    if (now >= windowEventLoop->computeIdleDeadline() || m_runnableIdleCallbacks.isEmpty())
        return;

    auto request = m_runnableIdleCallbacks.takeFirst();
    auto idleDeadline = IdleDeadline::create(IdleDeadline::DidTimeout::No);
    request.callback->handleEvent(idleDeadline.get());

    if (!m_runnableIdleCallbacks.isEmpty())
        queueTaskToInvokeIdleCallbacks();
}

// https://w3c.github.io/requestidlecallback/#dfn-invoke-idle-callback-timeout-algorithm
void IdleCallbackController::invokeIdleCallbackTimeout(unsigned identifier)
{
    if (!m_document)
        return;

    auto it = m_idleRequestCallbacks.findIf([identifier](auto& request) {
        return request.identifier == identifier;
    });

    if (it == m_idleRequestCallbacks.end())
        return;

    auto idleDeadline = IdleDeadline::create(IdleDeadline::DidTimeout::Yes);
    auto callback = WTFMove(it->callback);
    m_idleRequestCallbacks.remove(it);
    callback->handleEvent(idleDeadline.get());
}

} // namespace WebCore
