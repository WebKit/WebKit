/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
 * Copyright (C) 2020 Metrological Group B.V.
 * Copyright (C) 2020 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WorkerAnimationController.h"

#if ENABLE(OFFSCREEN_CANVAS)

#include "Performance.h"
#include "RequestAnimationFrameCallback.h"
#include "WorkerGlobalScope.h"
#include <wtf/Ref.h>

namespace WebCore {

Ref<WorkerAnimationController> WorkerAnimationController::create(WorkerGlobalScope& workerGlobalScope)
{
    return adoptRef(*new WorkerAnimationController(workerGlobalScope));
}

WorkerAnimationController::WorkerAnimationController(WorkerGlobalScope& workerGlobalScope)
    : ActiveDOMObject(&workerGlobalScope)
    , m_workerGlobalScope(workerGlobalScope)
    , m_animationTimer(*this, &WorkerAnimationController::animationTimerFired)
{
    suspendIfNeeded();
}

WorkerAnimationController::~WorkerAnimationController()
{
    ASSERT(!hasPendingActivity());
}

const char* WorkerAnimationController::activeDOMObjectName() const
{
    return "WorkerAnimationController";
}

bool WorkerAnimationController::virtualHasPendingActivity() const
{
    return m_animationTimer.isActive();
}

void WorkerAnimationController::stop()
{
    m_animationTimer.stop();
}

void WorkerAnimationController::suspend(ReasonForSuspension)
{
    m_savedIsActive = hasPendingActivity();
    stop();
}

void WorkerAnimationController::resume()
{
    if (m_savedIsActive) {
        m_savedIsActive = false;
        scheduleAnimation();
    }
}

WorkerAnimationController::CallbackId WorkerAnimationController::requestAnimationFrame(Ref<RequestAnimationFrameCallback>&& callback)
{
    // FIXME: There's a lot of missing throttling behaviour that's present on DOMDocument
    WorkerAnimationController::CallbackId callbackId = ++m_nextAnimationCallbackId;
    callback->m_firedOrCancelled = false;
    callback->m_id = callbackId;
    m_animationCallbacks.append(WTFMove(callback));

    scheduleAnimation();

    return callbackId;
}

void WorkerAnimationController::cancelAnimationFrame(CallbackId callbackId)
{
    for (size_t i = 0; i < m_animationCallbacks.size(); ++i) {
        auto& callback = m_animationCallbacks[i];
        if (callback->m_id == callbackId) {
            callback->m_firedOrCancelled = true;
            m_animationCallbacks.remove(i);
            return;
        }
    }
}

void WorkerAnimationController::scheduleAnimation()
{
    if (!m_workerGlobalScope.requestAnimationFrameEnabled())
        return;

    if (m_animationTimer.isActive())
        return;

    Seconds animationInterval = RequestAnimationFrameCallback::fullSpeedAnimationInterval;
    Seconds scheduleDelay = std::max(animationInterval - Seconds::fromMilliseconds(m_workerGlobalScope.performance().now() - m_lastAnimationFrameTimestamp), 0_s);

    m_animationTimer.startOneShot(scheduleDelay);
}

void WorkerAnimationController::animationTimerFired()
{
    m_lastAnimationFrameTimestamp = m_workerGlobalScope.performance().now();
    serviceRequestAnimationFrameCallbacks(m_lastAnimationFrameTimestamp);
}

void WorkerAnimationController::serviceRequestAnimationFrameCallbacks(DOMHighResTimeStamp timestamp)
{
    if (!m_animationCallbacks.size() || !m_workerGlobalScope.requestAnimationFrameEnabled())
        return;

    // First, generate a list of callbacks to consider. Callbacks registered from this point
    // on are considered only for the "next" frame, not this one.
    CallbackList callbacks(m_animationCallbacks);

    for (auto& callback : callbacks) {
        if (callback->m_firedOrCancelled)
            continue;
        callback->m_firedOrCancelled = true;
        callback->handleEvent(timestamp);
    }

    // Remove any callbacks we fired from the list of pending callbacks.
    m_animationCallbacks.removeAllMatching([](auto& callback) {
        return callback->m_firedOrCancelled;
    });

    if (m_animationCallbacks.size())
        scheduleAnimation();
}

} // namespace WebCore

#endif
