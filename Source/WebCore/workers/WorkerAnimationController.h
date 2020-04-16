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

#if ENABLE(OFFSCREEN_CANVAS)

#include "ActiveDOMObject.h"
#include "DOMHighResTimeStamp.h"
#include "PlatformScreen.h"
#include "Timer.h"
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class RequestAnimationFrameCallback;
class WorkerGlobalScope;

class WorkerAnimationController final : public ThreadSafeRefCounted<WorkerAnimationController>, public ActiveDOMObject {
public:
    static Ref<WorkerAnimationController> create(WorkerGlobalScope&);
    ~WorkerAnimationController();

    int requestAnimationFrame(Ref<RequestAnimationFrameCallback>&&);
    void cancelAnimationFrame(int);

    using ThreadSafeRefCounted::ref;
    using ThreadSafeRefCounted::deref;

private:
    WorkerAnimationController(WorkerGlobalScope&);

    const char* activeDOMObjectName() const final;

    bool virtualHasPendingActivity() const final;
    void stop() final;
    void suspend(ReasonForSuspension) final;
    void resume() final;

    void scheduleAnimation();
    void animationTimerFired();
    void serviceRequestAnimationFrameCallbacks(DOMHighResTimeStamp timestamp);

    WorkerGlobalScope& m_workerGlobalScope;

    typedef Vector<RefPtr<RequestAnimationFrameCallback>> CallbackList;
    CallbackList m_animationCallbacks;
    typedef int CallbackId;
    CallbackId m_nextAnimationCallbackId { 0 };

    Timer m_animationTimer;
    DOMHighResTimeStamp m_lastAnimationFrameTimestamp { 0 };

    bool m_savedIsActive { false };
};

} // namespace WebCore

#endif
