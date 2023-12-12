/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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

#include "JSCast.h"
#include "JSRunLoopTimer.h"
#include "Strong.h"

#include <wtf/Deque.h>
#include <wtf/FixedVector.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace JSC {

class JSPromise;
class VM;
class JSCell;

class DeferredWorkTimer final : public JSRunLoopTimer {
public:
    using Base = JSRunLoopTimer;

    struct TicketData {
    private:
        WTF_MAKE_TZONE_ALLOCATED(TicketData);
    public:
        inline TicketData(VM&, JSObject* scriptExecutionOwner, Vector<Strong<JSCell>>&& dependencies);

        inline VM& vm();
        JSObject* target();

        inline void cancel();
        bool isCancelled() const { return !scriptExecutionOwner.get(); }

        FixedVector<Strong<JSCell>> dependencies;
        Strong<JSObject> scriptExecutionOwner;
    };

    using Ticket = TicketData*;

    void doWork(VM&) final;

    JS_EXPORT_PRIVATE Ticket addPendingWork(VM&, JSObject* target, Vector<Strong<JSCell>>&& dependencies);
    bool hasAnyPendingWork() const;
    bool hasPendingWork(Ticket);
    bool hasDependancyInPendingWork(Ticket, JSCell* dependency);
    bool cancelPendingWork(Ticket);

    // If the script execution owner your ticket is associated with gets canceled
    // the Task will not be called and will be deallocated. So it's important
    // to make sure your memory ownership model won't leak memory when
    // this occurs. The easiest way is to make sure everything is either owned
    // by a GC'd value in dependencies or by the Task lambda.
    using Task = Function<void(Ticket)>;
    JS_EXPORT_PRIVATE void scheduleWorkSoon(Ticket, Task&&);
    JS_EXPORT_PRIVATE void didResumeScriptExecutionOwner();

    void stopRunningTasks() { m_runTasks = false; }
    JS_EXPORT_PRIVATE void runRunLoop();

    static Ref<DeferredWorkTimer> create(VM& vm) { return adoptRef(*new DeferredWorkTimer(vm)); }
private:
    DeferredWorkTimer(VM&);

    Lock m_taskLock;
    bool m_runTasks { true };
    bool m_shouldStopRunLoopWhenAllTicketsFinish { false };
    bool m_currentlyRunningTask { false };
    Deque<std::tuple<Ticket, Task>> m_tasks WTF_GUARDED_BY_LOCK(m_taskLock);
    HashSet<std::unique_ptr<TicketData>> m_pendingTickets;
};

inline JSObject* DeferredWorkTimer::TicketData::target()
{
    ASSERT(!isCancelled());
    return jsCast<JSObject*>(dependencies.last().get());
}

} // namespace JSC
