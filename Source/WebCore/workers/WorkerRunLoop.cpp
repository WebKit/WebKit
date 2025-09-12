/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2016-2024 Apple Inc. All rights reserved.
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
#include "Logging.h"
#include "SWServer.h"
#include "ScriptExecutionContext.h"
#include "ServiceWorkerGlobalScope.h"
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

#if PLATFORM(COCOA)
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

#if USE(GLIB)
#include <glib.h>
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WorkerRunLoop);
WTF_MAKE_TZONE_ALLOCATED_IMPL(WorkerDedicatedRunLoop);
WTF_MAKE_TZONE_ALLOCATED_IMPL(WorkerDedicatedRunLoop::Task);

class WorkerSharedTimer final : public SharedTimer {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(WorkerSharedTimer);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(WorkerSharedTimer);
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
        if (!m_runLoop.m_nestedCount) {
            m_runLoop.m_sharedTimer = makeUnique<WorkerSharedTimer>();
            threadGlobalDataSingleton().threadTimers().setSharedTimer(m_runLoop.m_sharedTimer.get());
        }
        m_runLoop.m_nestedCount++;
        if (m_isForDebugging == IsForDebugging::Yes)
            m_runLoop.m_debugCount++;
    }

    ~RunLoopSetup()
    {
        m_runLoop.m_nestedCount--;
        if (!m_runLoop.m_nestedCount) {
            threadGlobalDataSingleton().threadTimers().setSharedTimer(nullptr);
            m_runLoop.m_sharedTimer = nullptr;
        }
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

    // <rdar://155433911> - ServiceWorkers sometimes end up spinning their worker runloop endlessly,
    // repeatedly timing out getting a task from the message queue, not firing the web shared timer,
    // then firing their CFRunLoopTimers which do very little (or nothing).
    // This spinning is wasteful and - we suspect - unproductive.
    // For ServiceWorkers only, we will log this scenario to learn more.
    // The requirements are:
    // 1 - The RunLoop must spin repeatedly, never handling a message or firing the web shared timer
    // 2 - It must do so spanning at least "defaultTerminationDelay" seconds
    // 3 - It must do so at a frequency we expect to be bad. (I'm arbitrarily choosing 10hz)
    struct RunLoopStatus {
        enum class ShouldLogExcessiveRunLoopSpinning : bool { No, Yes };
        ShouldLogExcessiveRunLoopSpinning addRunLoopSpin()
        {
            numberOfRunLoopSpinsInARow++;
            auto now = MonotonicTime::now();

            if (timeOfFirstRunLoopOnlySpin == MonotonicTime::infinity()) {
                timeOfFirstRunLoopOnlySpin = now;
                return ShouldLogExcessiveRunLoopSpinning::No;
            }

            static constexpr Seconds lengthOfAllowableSpinning = SWServer::defaultTerminationDelay;

            auto timeSpinning = now - timeOfFirstRunLoopOnlySpin;
            if (timeSpinning < lengthOfAllowableSpinning)
                return ShouldLogExcessiveRunLoopSpinning::No;

            static const double frequencyLimit = 10.0;
            if (numberOfRunLoopSpinsInARow / timeSpinning.seconds() < frequencyLimit)
                return ShouldLogExcessiveRunLoopSpinning::No;

            return ShouldLogExcessiveRunLoopSpinning::Yes;
        }

        double secondsSpentSpinning()
        {
            return std::max(0.0, (MonotonicTime::now() - timeOfFirstRunLoopOnlySpin).seconds());
        }

    private:
        MonotonicTime timeOfFirstRunLoopOnlySpin { MonotonicTime::infinity() };
        size_t numberOfRunLoopSpinsInARow { 0 };
    };
    RunLoopStatus currentRunLoopStatus;
    RunInModeResult result;

    do {
        result = runInMode(context, modePredicate);

        // Only do further RunLoop status tracking for ServiceWorker contexts.
        if (!is<ServiceWorkerGlobalScope>(context))
            continue;

        // We consider the message queue handling a message or this thread's shared timer firing to signify
        // web content "making progress", so either are an acceptable result;
        if (result.messageQueueResult != MessageQueueTimeout || result.firedSharedTimer) {
            currentRunLoopStatus = { };
            continue;
        }

        if (currentRunLoopStatus.addRunLoopSpin() == RunLoopStatus::ShouldLogExcessiveRunLoopSpinning::No)
            continue;

        auto reason = makeString("ServiceWorker message queue spun excessively without making web content progress for "_s, currentRunLoopStatus.secondsSpentSpinning(), " seconds. Shared timer firing in "_s, m_sharedTimer->fireTimeDelay().seconds(), " seconds. RunLoop rimers before: "_s, result.activeRunLoopTimersBeforeFiring, ". RunLoop timers after: "_s, result.activeRunLoopTimersAfterFiring);
        RELEASE_LOG(ServiceWorker, "%s", reason.utf8().data());

#if PLATFORM(COCOA)
        if (WTF::CocoaApplication::isAppleApplication())
            os_fault_with_payload(OS_REASON_WEBKIT, 0, nullptr, 0, reason.utf8().data(), 0);
#endif

        // Reset status to start tracking a new sequence of spinning.
        currentRunLoopStatus = { };
    } while (result.messageQueueResult != MessageQueueTerminated);
    runCleanupTasks(context);
}

MessageQueueWaitResult WorkerDedicatedRunLoop::runInDebuggerMode(WorkerOrWorkletGlobalScope& context)
{
    RunLoopSetup setup(*this, RunLoopSetup::IsForDebugging::Yes);
    return runInMode(&context, ModePredicate { debuggerMode(), false }).messageQueueResult;
}

bool WorkerDedicatedRunLoop::runInMode(WorkerOrWorkletGlobalScope* context, const String& mode, bool allowEventLoopTasks)
{
    ASSERT(mode != debuggerMode());
    RunLoopSetup setup(*this, RunLoopSetup::IsForDebugging::No);
    ModePredicate modePredicate(String { mode }, allowEventLoopTasks);
    return runInMode(context, modePredicate).messageQueueResult != MessageQueueWaitResult::MessageQueueTerminated;
}

WorkerDedicatedRunLoop::RunInModeResult WorkerDedicatedRunLoop::runInMode(WorkerOrWorkletGlobalScope* context, const ModePredicate& predicate)
{
    ASSERT(context);
    ASSERT(context->workerOrWorkletThread()->thread() == &Thread::currentSingleton());

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
    CFAbsoluteTime nextCFRunLoopTimerFireDate = CFRunLoopGetNextTimerFireDate(RetainPtr { CFRunLoopGetCurrent() }.get(), kCFRunLoopDefaultMode);
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

    RunInModeResult runInModeResult;
    runInModeResult.messageQueueResult = result;

    // If the context is closing, don't execute any further JavaScript tasks (per section 4.1.1 of the Web Workers spec).
    // However, there may be implementation cleanup tasks in the queue, so keep running through it.
    switch (result) {
    case MessageQueueTerminated:
        break;

    case MessageQueueMessageReceived:
        task->performTask(context);
        break;

    case MessageQueueTimeout:
        if (!context->isClosing() && !isBeingDebugged()) {
            runInModeResult.firedSharedTimer = true;
            m_sharedTimer->fire();
        }
        break;
    }

#if USE(CF)
    if (result != MessageQueueTerminated) {
        if (nextCFRunLoopTimerFireDate <= CFAbsoluteTimeGetCurrent()) {
            runInModeResult.firedRunLoopTimer = true;

            runInModeResult.activeRunLoopTimersBeforeFiring = RunLoop::currentSingleton().listActiveTimersForLogging();
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, /*returnAfterSourceHandled*/ false);
            runInModeResult.activeRunLoopTimersAfterFiring = RunLoop::currentSingleton().listActiveTimersForLogging();
        }
    }
#endif

    return runInModeResult;
}

void WorkerDedicatedRunLoop::runCleanupTasks(WorkerOrWorkletGlobalScope* context)
{
    ASSERT(context);
    ASSERT(context->workerOrWorkletThread()->thread() == &Thread::currentSingleton());
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
        if (context->script() && scope.exception()) [[unlikely]] {
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

    RunLoop::mainSingleton().dispatch([weakThis = WeakPtr { *this }, task = WTFMove(task)]() mutable {
        if (!weakThis || weakThis->m_terminated)
            return;
        RefPtr workerOrWorkletGlobalScope = weakThis->m_workerOrWorkletGlobalScope.get();
        if (!workerOrWorkletGlobalScope)
            return;

        weakThis->m_terminated = true;
        task.performTask(*workerOrWorkletGlobalScope);
    });
}

void WorkerMainRunLoop::postTaskForMode(ScriptExecutionContext::Task&& task, const String& /*mode*/)
{
    if (m_terminated)
        return;

    RunLoop::mainSingleton().dispatch([weakThis = WeakPtr { *this }, task = WTFMove(task)]() mutable {
        if (!weakThis || weakThis->m_terminated)
            return;
        RefPtr workerOrWorkletGlobalScope = weakThis->m_workerOrWorkletGlobalScope.get();
        if (!workerOrWorkletGlobalScope)
            return;

        task.performTask(*workerOrWorkletGlobalScope);
    });
}

bool WorkerMainRunLoop::runInMode(WorkerOrWorkletGlobalScope*, const String&, bool)
{
    RunLoop::mainSingleton().cycle();
    return true;
}

} // namespace WebCore
