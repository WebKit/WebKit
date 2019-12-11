/*
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>.
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

#include <wtf/AutomaticThread.h>
#include <wtf/Deque.h>
#include <wtf/Function.h>
#include <wtf/NumberOfCores.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WTF {

class WorkerPool : public ThreadSafeRefCounted<WorkerPool> {
public:
    WTF_EXPORT_PRIVATE void postTask(Function<void()>&&);

    WTF_EXPORT_PRIVATE ~WorkerPool();

    // If timeout is infinity, it means AutomaticThread will be never automatically destroyed.
    static Ref<WorkerPool> create(ASCIILiteral name, unsigned numberOfWorkers  = WTF::numberOfProcessorCores(), Seconds timeout = Seconds::infinity())
    {
        ASSERT(numberOfWorkers >= 1);
        return adoptRef(*new WorkerPool(name, numberOfWorkers, timeout));
    }

    ASCIILiteral name() const { return m_name; }

private:
    class Worker;
    friend class Worker;

    WTF_EXPORT_PRIVATE WorkerPool(ASCIILiteral name, unsigned numberOfWorkers, Seconds timeout);

    bool shouldSleep(const AbstractLocker&);

    Box<Lock> m_lock;
    Ref<AutomaticThreadCondition> m_condition;
    Seconds m_timeout;
    MonotonicTime m_lastTimeoutTime { MonotonicTime::nan() };
    unsigned m_numberOfActiveWorkers { 0 };
    Vector<Ref<Worker>> m_workers;
    Deque<Function<void()>> m_tasks;
    ASCIILiteral m_name;
};

}

using WTF::WorkerPool;
