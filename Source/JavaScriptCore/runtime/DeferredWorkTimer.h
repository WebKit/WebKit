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

#include "JSRunLoopTimer.h"
#include "WeakInlines.h"

#include <wtf/Deque.h>
#include <wtf/FixedVector.h>
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/Vector.h>

namespace JSC {

class VM;
class JSCell;
class JSObject;
class JSGlobalObject;

class DeferredWorkTimer final : public JSRunLoopTimer {
public:
    using Base = JSRunLoopTimer;

    enum class WorkType : uint8_t {
        ImminentlyScheduled,
        AtSomePoint,
    };

    class TicketData : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<TicketData>  {
    private:
        WTF_MAKE_TZONE_ALLOCATED(TicketData);
        WTF_MAKE_NONCOPYABLE(TicketData);
    public:
        inline static Ref<TicketData> create(WorkType, JSGlobalObject*, JSObject* scriptExecutionOwner, Vector<Weak<JSCell>>&& dependencies);

        WorkType type() const { return m_type; }
        inline VM& vm();
        JSObject* target();
        inline bool hasValidTarget() const;
        inline FixedVector<Weak<JSCell>>& dependencies();
        inline JSObject* scriptExecutionOwner();
        inline JSGlobalObject* globalObject();

        inline void cancel();
        bool isCancelled() const { return !m_scriptExecutionOwner.get() || !m_globalObject.get() || !hasValidTarget(); }

    private:
        inline TicketData(WorkType, JSGlobalObject*, JSObject* scriptExecutionOwner, Vector<Weak<JSCell>>&& dependencies);

        WorkType m_type;
        FixedVector<Weak<JSCell>> m_dependencies;
        Weak<JSObject> m_scriptExecutionOwner;
        Weak<JSGlobalObject> m_globalObject;
    };

    using Ticket = TicketData*;

    void doWork(VM&) final;

    JS_EXPORT_PRIVATE Ticket addPendingWork(WorkType, VM&, JSObject* target, Vector<Weak<JSCell>>&& dependencies);

    JS_EXPORT_PRIVATE bool hasAnyPendingWork() const;
    JS_EXPORT_PRIVATE bool hasImminentlyScheduledWork() const;
    bool hasPendingWork(Ticket);
    bool hasDependencyInPendingWork(Ticket, JSCell* dependency);
    bool cancelPendingWork(Ticket);
    void cancelPendingWorkSafe(JSGlobalObject*);

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

    WTF::Function<void(Ref<TicketData>&&, WorkType)> onAddPendingWork;
    WTF::Function<void(Ticket, Task&&)> onScheduleWorkSoon;
    WTF::Function<void(Ticket)> onCancelPendingWork;
private:
    DeferredWorkTimer(VM&);

    Lock m_taskLock;
    bool m_runTasks { true };
    bool m_shouldStopRunLoopWhenAllTicketsFinish { false };
    bool m_currentlyRunningTask { false };
    Deque<std::tuple<Ticket, Task>> m_tasks WTF_GUARDED_BY_LOCK(m_taskLock);
    HashSet<Ref<TicketData>> m_pendingTickets;
};

inline JSObject* DeferredWorkTimer::TicketData::target()
{
    ASSERT(!isCancelled());
    return jsCast<JSObject*>(m_dependencies.last().get());
}

inline bool DeferredWorkTimer::TicketData::hasValidTarget() const
{
    return !m_dependencies.isEmpty() && !!m_dependencies.last().get();
}

inline FixedVector<Weak<JSCell>>& DeferredWorkTimer::TicketData::dependencies()
{
    ASSERT(!isCancelled());
    return m_dependencies;
}

inline JSObject* DeferredWorkTimer::TicketData::scriptExecutionOwner()
{
    ASSERT(!isCancelled());
    return m_scriptExecutionOwner.get();
}

inline JSGlobalObject* DeferredWorkTimer::TicketData::globalObject()
{
    ASSERT(!isCancelled());
    return m_globalObject.get();
}

} // namespace JSC
