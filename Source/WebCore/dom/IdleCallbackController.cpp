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
#include "Page.h"
#include "Timer.h"
#include "WindowEventLoop.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(IdleCallbackController);

IdleCallbackController::IdleCallbackController(Document& document)
    : m_document(document)
{

}

int IdleCallbackController::queueIdleCallback(Ref<IdleRequestCallback>&& callback, Seconds timeout)
{
    ++m_idleCallbackIdentifier;
    auto handle = m_idleCallbackIdentifier;

    bool hasTimeout = timeout > 0_s;
    m_idleRequestCallbacks.append({ handle, WTFMove(callback), hasTimeout ? std::optional { MonotonicTime::now() + timeout } : std::nullopt });

    if (hasTimeout) {
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

    if (RefPtr document = m_document.get())
        document->protectedWindowEventLoop()->scheduleIdlePeriod();

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

// https://w3c.github.io/requestidlecallback/#start-an-idle-period-algorithm
void IdleCallbackController::startIdlePeriod()
{
    for (auto& request : m_idleRequestCallbacks)
        m_runnableIdleCallbacks.append(WTFMove(request));
    m_idleRequestCallbacks.clear();

    if (m_runnableIdleCallbacks.isEmpty())
        return;

    while (invokeIdleCallbacks()) { }
}

void IdleCallbackController::queueTaskToInvokeIdleCallbacks()
{
    Ref document = *m_document;
    document->eventLoop().queueTask(TaskSource::IdleTask, [this, document] {
        RELEASE_ASSERT(document->idleCallbackController() == this);
        while (invokeIdleCallbacks()) { }
    });
}

// https://w3c.github.io/requestidlecallback/#invoke-idle-callbacks-algorithm
bool IdleCallbackController::invokeIdleCallbacks()
{
    RefPtr document = m_document.get();
    if (!document || !document->frame())
        return false;

    Ref windowEventLoop = document->windowEventLoop();
    // FIXME: Implement "if the user-agent believes it should end the idle period early due to newly scheduled high-priority work, return from the algorithm."

    auto now = MonotonicTime::now();
    auto deadline = windowEventLoop->computeIdleDeadline();
    if (now >= deadline || m_runnableIdleCallbacks.isEmpty())
        return false;

    auto request = m_runnableIdleCallbacks.takeFirst();
    auto idleDeadline = IdleDeadline::create(request.timeout && *request.timeout < now ? IdleDeadline::DidTimeout::Yes : IdleDeadline::DidTimeout::No);
    request.callback->handleEvent(idleDeadline.get());

    return !m_runnableIdleCallbacks.isEmpty();
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
