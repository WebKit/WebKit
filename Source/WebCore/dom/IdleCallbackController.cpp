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
    auto request = IdleRequest::create(handle, WTF::move(callback), hasTimeout ? std::optional { MonotonicTime::now() + timeout } : std::nullopt);
    m_idleRequestCallbacks.append(request.copyRef());
    m_pendingIdleRequestsByIdentifier.add(handle, WTF::move(request));

    if (hasTimeout) {
        Timer::schedule(timeout, [weakThis = WeakPtr { *this }, handle]() mutable {
            CheckedPtr checkedThis = weakThis.get();
            if (!checkedThis)
                return;
            RefPtr document = checkedThis->m_document.get();
            if (!document)
                return;
            document->eventLoop().queueTask(TaskSource::IdleTask, [weakThis = WTF::move(weakThis), handle]() {
                if (CheckedPtr checkedThis = weakThis.get())
                    checkedThis->invokeIdleCallbackTimeout(handle);
            });
        });
    }

    if (RefPtr document = m_document.get())
        protect(document->windowEventLoop())->scheduleIdlePeriod();

    return handle;
}

void IdleCallbackController::removeIdleCallback(int signedIdentifier)
{
    if (signedIdentifier <= 0)
        return;
    unsigned identifier = signedIdentifier;

    if (auto request = m_pendingIdleRequestsByIdentifier.take(identifier))
        request->isPending = false;
}

// https://w3c.github.io/requestidlecallback/#start-an-idle-period-algorithm
void IdleCallbackController::startIdlePeriod()
{
    for (auto& request : m_idleRequestCallbacks) {
        if (request->isPending)
            m_runnableIdleCallbacks.append(WTF::move(request));
    }
    m_idleRequestCallbacks.clear();

    if (m_runnableIdleCallbacks.isEmpty())
        return;

    while (invokeIdleCallbacks()) { }
}

void IdleCallbackController::queueTaskToInvokeIdleCallbacks()
{
    Ref document = *m_document;
    document->eventLoop().queueTask(TaskSource::IdleTask, [weakThis = WeakPtr { *this }, document] {
        CheckedPtr checkedThis = weakThis.get();
        if (!checkedThis)
            return;
        RELEASE_ASSERT(document->idleCallbackController() == checkedThis.get());
        while (checkedThis->invokeIdleCallbacks()) { }
    });
}

// https://w3c.github.io/requestidlecallback/#invoke-idle-callbacks-algorithm
bool IdleCallbackController::invokeIdleCallbacks()
{
    RefPtr document = m_document.get();
    if (!document || !document->frame())
        return false;

    // Drop cancelled/already-timed-out entries left at the front by removeIdleCallback()
    // or invokeIdleCallbackTimeout() before spending any idle-time budget on them; this
    // keeps isEmpty() accurate even if the deadline check below causes an early return.
    while (!m_runnableIdleCallbacks.isEmpty() && !m_runnableIdleCallbacks.first()->isPending)
        m_runnableIdleCallbacks.removeFirst();

    if (m_runnableIdleCallbacks.isEmpty())
        return false;

    Ref windowEventLoop = document->windowEventLoop();
    // FIXME: Implement "if the user-agent believes it should end the idle period early due to newly scheduled high-priority work, return from the algorithm."

    auto now = MonotonicTime::now();
    auto deadline = windowEventLoop->computeIdleDeadline();
    if (now >= deadline)
        return false;

    auto request = m_runnableIdleCallbacks.takeFirst();
    m_pendingIdleRequestsByIdentifier.remove(request->identifier);
    auto idleDeadline = IdleDeadline::create(request->timeout && *request->timeout < now ? IdleDeadline::DidTimeout::Yes : IdleDeadline::DidTimeout::No);
    protect(request->callback)->invoke(idleDeadline.get());

    return !m_runnableIdleCallbacks.isEmpty();
}

// https://w3c.github.io/requestidlecallback/#dfn-invoke-idle-callback-timeout-algorithm
void IdleCallbackController::invokeIdleCallbackTimeout(unsigned identifier)
{
    if (!m_document)
        return;

    auto request = m_pendingIdleRequestsByIdentifier.take(identifier);
    if (!request)
        return;

    request->isPending = false;

    auto idleDeadline = IdleDeadline::create(IdleDeadline::DidTimeout::Yes);
    protect(request->callback)->invoke(idleDeadline.get());
}

} // namespace WebCore
