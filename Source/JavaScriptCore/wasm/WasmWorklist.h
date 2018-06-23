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

#if ENABLE(WEBASSEMBLY)

#include <queue>

#include <wtf/AutomaticThread.h>
#include <wtf/PriorityQueue.h>
#include <wtf/Vector.h>

namespace JSC {

namespace Wasm {

struct Context;
class Plan;

class Worklist {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Worklist();
    ~Worklist();

    JS_EXPORT_PRIVATE void enqueue(Ref<Plan>);
    void stopAllPlansForContext(Context&);

    JS_EXPORT_PRIVATE void completePlanSynchronously(Plan&);

    enum class Priority {
        Shutdown,
        Synchronous,
        Compilation,
        Preparation
    };
    const char* priorityString(Priority);

private:
    class Thread;
    friend class Thread;

    typedef uint64_t Ticket;
    Ticket nextTicket() { return m_lastGrantedTicket++; }

    struct QueueElement {
        Priority priority;
        Ticket ticket;
        RefPtr<Plan> plan;

        void setToNextPriority();
    };

    static bool isHigherPriority(const QueueElement& left, const QueueElement& right)
    {
        if (left.priority == right.priority)
            return left.ticket > right.ticket;
        return left.priority > right.priority;
    }

    Box<Lock> m_lock;
    Ref<AutomaticThreadCondition> m_planEnqueued;
    // Technically, this could overflow but that's unlikely. Even if it did, we will just compile things of the same
    // Priority it the wrong order, which isn't wrong, just suboptimal.
    Ticket m_lastGrantedTicket { 0 };
    PriorityQueue<QueueElement, isHigherPriority, 10> m_queue;
    Vector<std::unique_ptr<Thread>> m_threads;
};

Worklist* existingWorklistOrNull();
JS_EXPORT_PRIVATE Worklist& ensureWorklist();

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
