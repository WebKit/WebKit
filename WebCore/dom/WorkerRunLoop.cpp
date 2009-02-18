/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
#include "config.h"

#if ENABLE(WORKERS)

#include "ScriptExecutionContext.h"
#include "SharedTimer.h"
#include "ThreadGlobalData.h"
#include "ThreadTimers.h"
#include "WorkerRunLoop.h"
#include "WorkerContext.h"
#include "WorkerThread.h"

namespace WebCore {

class WorkerSharedTimer : public SharedTimer {
public:
    WorkerSharedTimer()
        : m_sharedTimerFunction(0)
        , m_nextFireTime(0)
    {
    }

    // SharedTimer interface.
    virtual void setFiredFunction(void (*function)()) { m_sharedTimerFunction = function; }
    virtual void setFireTime(double fireTime) { m_nextFireTime = fireTime; }
    virtual void stop() { m_nextFireTime = 0; }

    bool isActive() { return m_sharedTimerFunction && m_nextFireTime; }
    double fireTime() { return m_nextFireTime; }
    void fire() { m_sharedTimerFunction(); }

private:
    void (*m_sharedTimerFunction)();
    double m_nextFireTime;
};

WorkerRunLoop::WorkerRunLoop()
    : m_sharedTimer(new WorkerSharedTimer)
{
}

WorkerRunLoop::~WorkerRunLoop()
{
}

void WorkerRunLoop::run(WorkerContext* context)
{
    ASSERT(context);
    ASSERT(context->thread());
    ASSERT(context->thread()->threadID() == currentThread());

    threadGlobalData().threadTimers().setSharedTimer(m_sharedTimer.get());

    while (true) {
        RefPtr<ScriptExecutionContext::Task> task;
        MessageQueueWaitResult result;

        if (m_sharedTimer->isActive())
            result = m_messageQueue.waitForMessageTimed(task, m_sharedTimer->fireTime());
        else
            result = (m_messageQueue.waitForMessage(task) ? MessageQueueMessageReceived : MessageQueueTerminated);

        if (result == MessageQueueTerminated)
            break;
        
        if (result == MessageQueueMessageReceived)
            task->performTask(context);
        else {
            ASSERT(result == MessageQueueTimeout);
            m_sharedTimer->fire();
        }
    }

    threadGlobalData().threadTimers().setSharedTimer(0);
}

void WorkerRunLoop::terminate()
{
    m_messageQueue.kill();
}

void WorkerRunLoop::postTask(PassRefPtr<ScriptExecutionContext::Task> task)
{
    m_messageQueue.append(task);
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
