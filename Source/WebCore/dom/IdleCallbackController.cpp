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

unsigned IdleCallbackController::queueIdleCallback(Ref<IdleRequestCallback>&& callback, Seconds timeout)
{
    ++m_idleCallbackIdentifier;
    auto handle = m_idleCallbackIdentifier;

    bool hasTimeout = timeout > 0_s;
    m_idleRequestCallbacks.append({ handle, WTF::move(callback), hasTimeout ? std::optional { MonotonicTime::now() + timeout } : std::nullopt });

    if (hasTimeout) {
        Timer::schedule(timeout, [weakThis = WeakPtr { *this }, handle]() mutable {
            CheckedPtr checkedThis = weakThis.get();
            if (!checkedThis)
                return;
            RefPtr document = checkedThis->m_document.get();
            if (!document)
                return;
            protect(document->eventLoop())->queueTask(TaskSource::IdleTask, [weakThis = WTF::move(weakThis), handle]() {
                if (CheckedPtr checkedThis = weakThis.get())
                    checkedThis->invokeIdleCallbackTimeout(handle);
            });
        });
    }

    if (RefPtr document = m_document.get())
        protect(document->windowEventLoop())->scheduleIdlePeriod();

    return handle;
}

void IdleCallbackController::removeIdleCallback(unsigned identifier)
{
    auto it = m_idleRequestCallbacks.findIf([identifier](auto& request) {
        return request.identifier == identifier;
    });

    if (it == m_idleRequestCallbacks.end())
        return;

    if (it->callback) {
        it->callback = nullptr;
        ++m_staleRequestCount;
    }
}

// https://w3c.github.io/requestidlecallback/#start-an-idle-period-algorithm
void IdleCallbackController::startIdlePeriod()
{
    m_runnableRequestCount = m_idleRequestCallbacks.size();
    if (!m_runnableRequestCount)
        return;

    while (invokeIdleCallbacks()) { }
}

// https://w3c.github.io/requestidlecallback/#invoke-idle-callbacks-algorithm
bool IdleCallbackController::invokeIdleCallbacks()
{
    ASSERT(!m_idleRequestCallbacks.isEmpty());
    if (!m_runnableRequestCount)
        return false;

    RefPtr document = m_document.get();
    if (!document || !document->frame())
        return false;

    Ref windowEventLoop = document->windowEventLoop();
    auto now = MonotonicTime::now();
    auto deadline = windowEventLoop->computeIdleDeadline();
    if (now >= deadline)
        return false;
    auto request = takeFirstRunnableNonStaleRequest();
    if (!request)
        return false;

    auto idleDeadline = IdleDeadline::create(request->timeout && *request->timeout < now ? IdleDeadline::DidTimeout::Yes : IdleDeadline::DidTimeout::No);
    protect(request->callback)->invoke(idleDeadline.get());

    return !m_idleRequestCallbacks.isEmpty();
}

std::optional<IdleCallbackController::IdleRequest> IdleCallbackController::takeFirstRunnableNonStaleRequest()
{
    while (m_runnableRequestCount) {
        --m_runnableRequestCount;
        auto request = m_idleRequestCallbacks.takeFirst();
        if (!request.callback) {
            --m_staleRequestCount;
            continue;
        }
        return request;
    }
    return std::nullopt;
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
    auto callback = std::exchange(it->callback, nullptr);
    ++m_staleRequestCount;

    callback->invoke(idleDeadline.get());
}

} // namespace WebCore
