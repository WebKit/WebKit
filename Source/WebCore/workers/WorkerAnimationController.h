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

#pragma once

#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)

#include "ActiveDOMObject.h"
#include "DOMHighResTimeStamp.h"
#include "PlatformScreen.h"
#include "Timer.h"
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class RequestAnimationFrameCallback;
class WorkerGlobalScope;

class WorkerAnimationController : public ThreadSafeRefCounted<WorkerAnimationController>, public ActiveDOMObject {
public:
    WEBCORE_EXPORT ~WorkerAnimationController();

    int requestAnimationFrame(Ref<RequestAnimationFrameCallback>&&);
    void cancelAnimationFrame(int);

    using ThreadSafeRefCounted::ref;
    using ThreadSafeRefCounted::deref;

protected:
    virtual void scheduleAnimation() = 0;
    virtual void stopAnimation() = 0;
    virtual bool isActive() const = 0;
    WEBCORE_EXPORT void animationTimerFired();

    WEBCORE_EXPORT WorkerAnimationController(WorkerGlobalScope&);

    WorkerGlobalScope& m_workerGlobalScope;
    DOMHighResTimeStamp m_lastAnimationFrameTimestamp { 0 };

private:
    WEBCORE_EXPORT const char* activeDOMObjectName() const final;

    WEBCORE_EXPORT bool virtualHasPendingActivity() const final;
    WEBCORE_EXPORT void stop() final;
    WEBCORE_EXPORT void suspend(ReasonForSuspension) final;
    WEBCORE_EXPORT void resume() final;

    void serviceRequestAnimationFrameCallbacks(DOMHighResTimeStamp timestamp);

    typedef Vector<RefPtr<RequestAnimationFrameCallback>> CallbackList;
    CallbackList m_animationCallbacks;
    typedef int CallbackId;
    CallbackId m_nextAnimationCallbackId { 0 };

    bool m_savedIsActive { false };
};

class TimerWorkerAnimationController final : public WorkerAnimationController {
public:
    static Ref<WorkerAnimationController> create(WorkerGlobalScope&);

private:
    TimerWorkerAnimationController(WorkerGlobalScope&);
    ~TimerWorkerAnimationController();

    void scheduleAnimation() final;
    void stopAnimation() final;
    bool isActive() const final;

    Timer m_animationTimer;
};

} // namespace WebCore

#endif
