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

#include "ScriptExecutionContext.h"
#include "SharedTimer.h"
#include "ThreadGlobalData.h"
#include "ThreadTimers.h"
#include "WorkerRunLoop.h"
#include "WorkerGlobalScope.h"
#include "WorkerThread.h"
#include <wtf/CurrentTime.h>

namespace WebCore {

class WorkerSharedTimer final : public SharedTimer {
public:
    // SharedTimer interface.
    virtual void setFiredFunction(std::function<void()>&& function) override { m_sharedTimerFunction = WTFMove(function); }
    virtual void setFireInterval(double interval) override { m_nextFireTime = interval + currentTime(); }
    virtual void stop() override { m_nextFireTime = 0; }

    bool isActive() { return m_sharedTimerFunction && m_nextFireTime; }
    double fireTime() { return m_nextFireTime; }
    void fire() { m_sharedTimerFunction(); }

private:
    std::function<void()> m_sharedTimerFunction;
    double m_nextFireTime { 0 };
};

class ModePredicate {
public:
    ModePredicate(const String& mode)
        : m_mode(mode)
        , m_defaultMode(mode == WorkerRunLoop::defaultMode())
    {
    }

    bool isDefaultMode() const
    {
        return m_defaultMode;
    }

    bool operator()(const WorkerRunLoop::Task& task) const
    {
        return m_defaultMode || m_mode == task.mode();
    }

private:
    String m_mode;
    bool m_defaultMode;
};

WorkerRunLoop::WorkerRunLoop()
    : m_sharedTimer(std::make_unique<WorkerSharedTimer>())
    , m_nestedCount(0)
    , m_uniqueId(0)
{
}

WorkerRunLoop::~WorkerRunLoop()
{
    ASSERT(!m_nestedCount);
}

String WorkerRunLoop::defaultMode()
{
    return String();
}

class RunLoopSetup {
    WTF_MAKE_NONCOPYABLE(RunLoopSetup);
public:
    RunLoopSetup(WorkerRunLoop& runLoop)
        : m_runLoop(runLoop)
    {
        if (!m_runLoop.m_nestedCount)
            threadGlobalData().threadTimers().setSharedTimer(m_runLoop.m_sharedTimer.get());
        m_runLoop.m_nestedCount++;
    }

    ~RunLoopSetup()
    {
        m_runLoop.m_nestedCount--;
        if (!m_runLoop.m_nestedCount)
            threadGlobalData().threadTimers().setSharedTimer(0);
    }
private:
    WorkerRunLoop& m_runLoop;
};

void WorkerRunLoop::run(WorkerGlobalScope* context)
{
    RunLoopSetup setup(*this);
    ModePredicate modePredicate(defaultMode());
    MessageQueueWaitResult result;
    do {
        result = runInMode(context, modePredicate, WaitForMessage);
    } while (result != MessageQueueTerminated);
    runCleanupTasks(context);
}

MessageQueueWaitResult WorkerRunLoop::runInMode(WorkerGlobalScope* context, const String& mode, WaitMode waitMode)
{
    RunLoopSetup setup(*this);
    ModePredicate modePredicate(mode);
    MessageQueueWaitResult result = runInMode(context, modePredicate, waitMode);
    return result;
}

MessageQueueWaitResult WorkerRunLoop::runInMode(WorkerGlobalScope* context, const ModePredicate& predicate, WaitMode waitMode)
{
    ASSERT(context);
    ASSERT(context->thread().threadID() == currentThread());

    double deadline = MessageQueue<Task>::infiniteTime();

#if USE(CF)
    CFAbsoluteTime nextCFRunLoopTimerFireDate = CFRunLoopGetNextTimerFireDate(CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    double timeUntilNextCFRunLoopTimerInSeconds = nextCFRunLoopTimerFireDate - CFAbsoluteTimeGetCurrent();
    deadline = currentTime() + std::max(0.0, timeUntilNextCFRunLoopTimerInSeconds);
#endif

    double absoluteTime = 0.0;
    if (waitMode == WaitForMessage) {
        if (predicate.isDefaultMode() && m_sharedTimer->isActive())
            absoluteTime = std::min(deadline, m_sharedTimer->fireTime());
        else
            absoluteTime = deadline;
    }
    MessageQueueWaitResult result;
    auto task = m_messageQueue.waitForMessageFilteredWithTimeout(result, predicate, absoluteTime);

    // If the context is closing, don't execute any further JavaScript tasks (per section 4.1.1 of the Web Workers spec).  However, there may be implementation cleanup tasks in the queue, so keep running through it.

    switch (result) {
    case MessageQueueTerminated:
        break;

    case MessageQueueMessageReceived:
        task->performTask(*this, context);
        break;

    case MessageQueueTimeout:
        if (!context->isClosing())
            m_sharedTimer->fire();
#if USE(CF)
        if (nextCFRunLoopTimerFireDate <= CFAbsoluteTimeGetCurrent())
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, /*returnAfterSourceHandled*/ false);
#endif
        break;
    }

    return result;
}

void WorkerRunLoop::runCleanupTasks(WorkerGlobalScope* context)
{
    ASSERT(context);
    ASSERT(context->thread().threadID() == currentThread());
    ASSERT(m_messageQueue.killed());

    while (true) {
        auto task = m_messageQueue.tryGetMessageIgnoringKilled();
        if (!task)
            return;
        task->performTask(*this, context);
    }
}

void WorkerRunLoop::terminate()
{
    m_messageQueue.kill();
}

void WorkerRunLoop::postTask(ScriptExecutionContext::Task task)
{
    postTaskForMode(WTFMove(task), defaultMode());
}

void WorkerRunLoop::postTaskAndTerminate(ScriptExecutionContext::Task task)
{
    m_messageQueue.appendAndKill(std::make_unique<Task>(WTFMove(task), defaultMode()));
}

void WorkerRunLoop::postTaskForMode(ScriptExecutionContext::Task task, const String& mode)
{
    m_messageQueue.append(std::make_unique<Task>(WTFMove(task), mode));
}

void WorkerRunLoop::Task::performTask(const WorkerRunLoop& runLoop, WorkerGlobalScope* context)
{
    if ((!context->isClosing() && !runLoop.terminated()) || m_task.isCleanupTask())
        m_task.performTask(*context);
}

WorkerRunLoop::Task::Task(ScriptExecutionContext::Task task, const String& mode)
    : m_task(WTFMove(task))
    , m_mode(mode.isolatedCopy())
{
}

} // namespace WebCore
