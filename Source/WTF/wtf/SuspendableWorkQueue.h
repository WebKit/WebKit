/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/Lock.h>
#include <wtf/RunLoop.h>
#include <wtf/Vector.h>
#include <wtf/WorkQueue.h>

namespace WTF {

class SuspendableWorkQueue final : public WorkQueue {
public:
    using QOS = WorkQueue::QOS;
    WTF_EXPORT_PRIVATE static Ref<SuspendableWorkQueue> create(const char* name, QOS = QOS::Default);

    template<typename C1, typename C2>
    void suspend(C1&& presuspendTask, C2&& suspendedTask)
    {
        dispatch([presuspendTask = WTFMove(presuspendTask), suspendedTask = WTFMove(suspendedTask), runloop = Ref { RunLoop::current() }] () mutable {
            presuspendTask();
            runloop->dispatch(WTFMove(suspendedTask));
        });
        resume();
        suspend();
    }

    enum SuspendState {
        WasRunning,
        WasSuspended
    };
    WTF_EXPORT_PRIVATE SuspendState suspend();
    WTF_EXPORT_PRIVATE SuspendState resume();
    WTF_EXPORT_PRIVATE void dispatch(Function<void()>&&) final;
    // Note: Will hold ref the until the timeout expires.
    WTF_EXPORT_PRIVATE void dispatchAfter(Seconds, Function<void()>&&) final;
    // Note: if sending sync to a suspended queue, other thread must resume the queue or deadlock occurs.
    WTF_EXPORT_PRIVATE void dispatchSync(Function<void()>&&) final;

private:
    SuspendableWorkQueue(const char* name, QOS);
    Lock m_lock;
    bool m_isSuspended WTF_GUARDED_BY_LOCK(m_lock) { false };
    Vector<Function<void()>> m_suspendedTasks WTF_GUARDED_BY_LOCK(m_lock);
};

} // namespace WTF

using WTF::SuspendableWorkQueue;
