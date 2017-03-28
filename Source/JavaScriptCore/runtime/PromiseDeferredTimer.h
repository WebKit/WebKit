/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "JSRunLoopTimer.h"
#include "Strong.h"
#include "WriteBarrier.h"

#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/SharedTask.h>
#include <wtf/Vector.h>

namespace JSC {

class JSPromiseDeferred;
class VM;
class JSCell;

class PromiseDeferredTimer : public JSRunLoopTimer {
public:
    using Base = JSRunLoopTimer;

    PromiseDeferredTimer(VM&);

    void doWork() override;

    void addPendingPromise(JSPromiseDeferred*, Vector<Strong<JSCell>>&& dependencies);
    // JSPromiseDeferred should handle canceling when the promise is resolved or rejected.
    bool cancelPendingPromise(JSPromiseDeferred*);

    typedef std::function<void()> Task;
    void scheduleWorkSoon(JSPromiseDeferred*, Task&&);

    // Blocked tasks should only be registered while holding the JS API lock. If we didn't require holding the
    // JS API lock then there might be a race where the promise you are waiting on is run before your task is
    // registered.
    void scheduleBlockedTask(JSPromiseDeferred*, Task&&);

    void stopRunningTasks() { m_runTasks = false; }
#if USE(CF)
    JS_EXPORT_PRIVATE void runRunLoop();
#endif

private:
    HashMap<JSPromiseDeferred*, Vector<Strong<JSCell>>> m_pendingPromises;
    Lock m_taskLock;
    bool m_runTasks { true };
#if USE(CF)
    bool m_shouldStopRunLoopWhenAllPromisesFinish { false };
#endif
    Vector<std::tuple<JSPromiseDeferred*, Task>> m_tasks;
    HashMap<JSPromiseDeferred*, Vector<Task>> m_blockedTasks;
};

} // namespace JSC
