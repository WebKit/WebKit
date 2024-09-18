/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2016-2023 Apple Inc.  All rights reserved.
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
#include "WorkerRunLoop.h"

#include "JSDOMExceptionHandling.h"
#include "JSDOMGlobalObject.h"
#include "ScriptExecutionContext.h"
#include "SharedTimer.h"
#include "ThreadGlobalData.h"
#include "ThreadTimers.h"
#include "WorkerEventLoop.h"
#include "WorkerOrWorkletGlobalScope.h"
#include "WorkerOrWorkletScriptController.h"
#include "WorkerThread.h"
#include <JavaScriptCore/CatchScope.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <JavaScriptCore/JSRunLoopTimer.h>
#include <wtf/AutodrainedPool.h>
#include <wtf/TZoneMallocInlines.h>

#if USE(GLIB)
#include <glib.h>
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WorkerRunLoop);
WTF_MAKE_TZONE_ALLOCATED_IMPL(WorkerDedicatedRunLoop);
WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED(WorkerDedicatedRunLoopTask, WorkerDedicatedRunLoop::Task);

class WorkerSharedTimer final : public SharedTimer {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(WorkerSharedTimer);
public:
    // SharedTimer interface.
    void setFiredFunction(Function<void()>&& function) final { m_sharedTimerFunction = WTFMove(function); }
    void setFireInterval(Seconds interval) final { m_nextFireTime = MonotonicTime::now() + interval; }
    void stop() final { m_nextFireTime = MonotonicTime { }; }

    bool isActive() { return m_sharedTimerFunction && m_nextFireTime; }
    Seconds fireTimeDelay() { return std::max(0_s, m_nextFireTime - MonotonicTime::now()); }
    void fire() { m_sharedTimerFunction(); }

private:
    Function<void()> m_sharedTimerFunction;
    MonotonicTime m_nextFireTime;
};

class ModePredicate {
public:
    explicit ModePredicate(String&& mode, bool allowEventLoopTasks)
        : m_mode(WTFMove(mode))
        , m_defaultMode(m_mode == WorkerRunLoop::defaultMode())
        , m_allowEventLoopTasks(allowEventLoopTasks)
    {
    }

    const String mode() const
    {
        return m_mode;
    }

    bool isDefaultMode() const
    {
        return m_defaultMode;
    }

    bool operator()(const WorkerDedicatedRunLoop::Task& task) const
    {
        return m_defaultMode || m_mode == task.mode() || (m_allowEventLoopTasks && task.mode() == WorkerEventLoop::taskMode());
    }

private:
    String m_mode;
    bool m_defaultMode;
    bool m_allowEventLoopTasks;
};

WorkerDedicatedRunLoop::WorkerDedicatedRunLoop()
    : m_sharedTimer(makeUnique<WorkerSharedTimer>())
{
}

WorkerDedicatedRunLoop::~WorkerDedicatedRunLoop()
{
    ASSERT(!m_nestedCount);
}

String WorkerRunLoop::defaultMode()
{
    return String();
}

static String debuggerMode()
{
    return "debugger"_s;
}

class RunLoopSetup {
    WTF_MAKE_NONCOPYABLE(RunLoopSetup);
public:
    enum class IsForDebugging : bool { No, Yes };
    RunLoopSetup(WorkerDedicatedRunLoop& runLoop, IsForDebugging isForDebugging)
        : m_runLoop(runLoop)
        , m_isForDebugging(isForDebugging)
    {
        if (!m_runLoop.m_nestedCount)
            threadGlobalData().threadTimers().setSharedTimer(m_runLoop.m_sharedTimer.get());
        m_runLoop.m_nestedCount++;
        if (m_isForDebugging == IsForDebugging::Yes)
            m_runLoop.m_debugCount++;
    }

    ~RunLoopSetup()
    {
        m_runLoop.m_nestedCount--;
        if (!m_runLoop.m_nestedCount)
            threadGlobalData().threadTimers().setSharedTimer(nullptr);
        if (m_isForDebugging == IsForDebugging::Yes)
            m_runLoop.m_debugCount--;
    }
private:
    WorkerDedicatedRunLoop& m_runLoop;
    IsForDebugging m_isForDebugging { IsForDebugging::No };
};

void WorkerDedicatedRunLoop::run(WorkerOrWorkletGlobalScope* context)
{
    RunLoopSetup setup(*this, RunLoopSetup::IsForDebugging::No);
    ModePredicate modePredicate(defaultMode(), false);
    MessageQueueWaitResult result;
    do {
        result = runInMode(context, modePredicate);
    } while (result != MessageQueueTerminated);
    runCleanupTasks(context);
}

MessageQueueWaitResult WorkerDedicatedRunLoop::runInDebuggerMode(WorkerOrWorkletGlobalScope& context)
{
    RunLoopSetup setup(*this, RunLoopSetup::IsForDebugging::Yes);
    return runInMode(&context, ModePredicate { debuggerMode(), false });
}

bool WorkerDedicatedRunLoop::runInMode(WorkerOrWorkletGlobalScope* context, const String& mode, bool allowEventLoopTasks)
{
    ASSERT(mode != debuggerMode());
    RunLoopSetup setup(*this, RunLoopSetup::IsForDebugging::No);
    ModePredicate modePredicate(String { mode }, allowEventLoopTasks);
    return runInMode(context, modePredicate) != MessageQueueWaitResult::MessageQueueTerminated;
}

MessageQueueWaitResult WorkerDedicatedRunLoop::runInMode(WorkerOrWorkletGlobalScope* context, const ModePredicate& predicate)
{
    ASSERT(context);
    ASSERT(context->workerOrWorkletThread()->thread() == &Thread::current());

    AutodrainedPool pool;

    const String predicateMode = predicate.mode();
    JSC::JSRunLoopTimer::TimerNotificationCallback timerAddedTask = createSharedTask<JSC::JSRunLoopTimer::TimerNotificationType>([this, predicateMode] {
        // We don't actually do anything here, we just want to loop around runInMode
        // to both recalculate our deadline and to potentially run the run loop.
        this->postTaskForMode([predicateMode](ScriptExecutionContext&) { }, predicateMode);
    });

#if USE(GLIB)
    GMainContext* mainContext = g_main_context_get_thread_default();
    if (g_main_context_pending(mainContext))
        g_main_context_iteration(mainContext, FALSE);
#endif

    Seconds timeoutDelay = Seconds::infinity();

#if USE(CF)
    CFAbsoluteTime nextCFRunLoopTimerFireDate = CFRunLoopGetNextTimerFireDate(CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    double timeUntilNextCFRunLoopTimerInSeconds = nextCFRunLoopTimerFireDate - CFAbsoluteTimeGetCurrent();
    timeoutDelay = std::max(0_s, Seconds(timeUntilNextCFRunLoopTimerInSeconds));
#endif

#if OS(WINDOWS)
    RunLoop::cycle();

    if (auto* script = context->script()) {
        JSC::VM& vm = script->vm();
        timeoutDelay = vm.deferredWorkTimer->timeUntilFire().value_or(Seconds::infinity());
    }
#endif

    if (predicate.isDefaultMode() && m_sharedTimer->isActive())
        timeoutDelay = std::min(timeoutDelay, m_sharedTimer->fireTimeDelay());

    if (auto* script = context->script()) {
        script->releaseHeapAccess();
        script->addTimerSetNotification(timerAddedTask);
    }
    MessageQueueWaitResult result;
    auto task = m_messageQueue.waitForMessageFilteredWithTimeout(result, predicate, timeoutDelay);
    if (auto* script = context->script()) {
        script->acquireHeapAccess();
        script->removeTimerSetNotification(timerAddedTask);
    }

    // If the context is closing, don't execute any further JavaScript tasks (per section 4.1.1 of the Web Workers spec).  However, there may be implementation cleanup tasks in the queue, so keep running through it.

    switch (result) {
    case MessageQueueTerminated:
        break;

    case MessageQueueMessageReceived:
        task->performTask(context);
        break;

    case MessageQueueTimeout:
        if (!context->isClosing() && !isBeingDebugged())
            m_sharedTimer->fire();
        break;
    }

#if USE(CF)
    if (result != MessageQueueTerminated) {
        if (nextCFRunLoopTimerFireDate <= CFAbsoluteTimeGetCurrent())
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, /*returnAfterSourceHandled*/ false);
    }
#endif

    return result;
}

void WorkerDedicatedRunLoop::runCleanupTasks(WorkerOrWorkletGlobalScope* context)
{
    ASSERT(context);
    ASSERT(context->workerOrWorkletThread()->thread() == &Thread::current());
    ASSERT(m_messageQueue.killed());

    while (true) {
        auto task = m_messageQueue.tryGetMessageIgnoringKilled();
        if (!task)
            return;
        task->performTask(context);
    }
}

void WorkerDedicatedRunLoop::terminate()
{
    m_messageQueue.kill();
}

void WorkerRunLoop::postTask(ScriptExecutionContext::Task&& task)
{
    postTaskForMode(WTFMove(task), defaultMode());
}

void WorkerDedicatedRunLoop::postTaskAndTerminate(ScriptExecutionContext::Task&& task)
{
    m_messageQueue.appendAndKill(makeUnique<Task>(WTFMove(task), defaultMode()));
}

void WorkerDedicatedRunLoop::postTaskForMode(ScriptExecutionContext::Task&& task, const String& mode)
{
    m_messageQueue.append(makeUnique<Task>(WTFMove(task), mode));
}

void WorkerRunLoop::postDebuggerTask(ScriptExecutionContext::Task&& task)
{
    postTaskForMode(WTFMove(task), debuggerMode());
}

void WorkerDedicatedRunLoop::Task::performTask(WorkerOrWorkletGlobalScope* context)
{
    if (m_task.isCleanupTask())
        m_task.performTask(*context);
    else if (!context->isClosing() && context->script() && !context->script()->isTerminatingExecution()) {
        JSC::VM& vm = context->script()->vm();
        auto scope = DECLARE_CATCH_SCOPE(vm);
        m_task.performTask(*context);
        if (UNLIKELY(context->script() && scope.exception())) {
            if (vm.hasPendingTerminationException()) {
                context->script()->forbidExecution();
                return;
            }
            Locker<JSC::JSLock> locker(vm.apiLock());
            reportException(context->script()->globalScopeWrapper(), scope.exception());
        }
    }
}

WorkerDedicatedRunLoop::Task::Task(ScriptExecutionContext::Task&& task, const String& mode)
    : m_task(WTFMove(task))
    , m_mode(mode.isolatedCopy())
{
}

WorkerMainRunLoop::WorkerMainRunLoop()
{
}

void WorkerMainRunLoop::setGlobalScope(WorkerOrWorkletGlobalScope& globalScope)
{
    m_workerOrWorkletGlobalScope = globalScope;
}

void WorkerMainRunLoop::postTaskAndTerminate(ScriptExecutionContext::Task&& task)
{
    if (m_terminated)
        return;

    RunLoop::main().dispatch([weakThis = WeakPtr { *this }, task = WTFMove(task)]() mutable {
        if (!weakThis || !weakThis->m_workerOrWorkletGlobalScope || weakThis->m_terminated)
            return;

        weakThis->m_terminated = true;
        task.performTask(*weakThis->m_workerOrWorkletGlobalScope);
    });
}

void WorkerMainRunLoop::postTaskForMode(ScriptExecutionContext::Task&& task, const String& /*mode*/)
{
    if (m_terminated)
        return;

    RunLoop::main().dispatch([weakThis = WeakPtr { *this }, task = WTFMove(task)]() mutable {
        if (!weakThis || !weakThis->m_workerOrWorkletGlobalScope || weakThis->m_terminated)
            return;

        task.performTask(*weakThis->m_workerOrWorkletGlobalScope);
    });
}

bool WorkerMainRunLoop::runInMode(WorkerOrWorkletGlobalScope*, const String&, bool)
{
    RunLoop::main().cycle();
    return true;
}

} // namespace WebCore
